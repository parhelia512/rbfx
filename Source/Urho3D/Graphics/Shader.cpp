//
// Copyright (c) 2008-2022 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../Precompiled.h"

#include <EASTL/sort.h>

#include "../Core/Context.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Shader.h"
#include "../Graphics/ShaderVariation.h"
#include "../IO/Deserializer.h"
#include "../IO/Log.h"
#include "../IO/VirtualFileSystem.h"
#include "../Resource/ResourceCache.h"

#include "../DebugNew.h"

namespace Urho3D
{

namespace
{

ea::array<bool, 128> GenerateAllowedCharacterMask()
{
    ea::array<bool, 128> result;
    // Allow letters, numbers and whitespace
    for (unsigned ch = 0; ch < 128; ++ch)
        result[ch] = std::isalnum(ch) || std::isspace(ch);
    // Allow specific symbols (see https://www.khronos.org/files/opengles_shading_language.pdf)
    const char specialSymbols[] = {
        '_', '.', '+', '-', '/', '*', '%',
        '<', '>', '[', ']', '(', ')', '{', '}',
        '^', '|', '&', '~', '=', '!', ':', ';', ',', '?',
        '#'
    };
    for (char ch : specialSymbols)
        result[ch] = true;
    return result;
};

template <class Iter, class Value>
Iter FindNth(Iter begin, Iter end, const Value& value, unsigned count)
{
    auto iter = ea::find(begin, end, value);
    while (count > 0 && iter != end)
    {
        iter = ea::find(ea::next(iter), end, value);
        --count;
    }
    return iter;
}

void CommentOutFunction(ea::string& code, const ea::string& signature)
{
    unsigned startPos = code.find(signature);
    unsigned braceLevel = 0;
    if (startPos == ea::string::npos)
        return;

    code.insert(startPos, "/*");

    for (unsigned i = startPos + 2 + signature.length(); i < code.length(); ++i)
    {
        if (code[i] == '{')
            ++braceLevel;
        else if (code[i] == '}')
        {
            --braceLevel;
            if (braceLevel == 0)
            {
                code.insert(i + 1, "*/");
                return;
            }
        }
    }
}

ea::string FormatLineDirective(bool isGLSL, const ea::string& fileName, unsigned fileIndex, unsigned line)
{
    if (isGLSL)
        return Format("/// #include {}\n#line {} {}\n", fileName, line, fileIndex);
    else
        return Format("#line {} \"{}\"\n", line, fileName);
}

ea::string NormalizeDefines(ea::string_view defines)
{
    ea::vector<ea::string> definesVec = ea::string{defines}.to_upper().split(' ');
    ea::quick_sort(definesVec.begin(), definesVec.end());
    return ea::string::joined(definesVec, " ");
}

}

ea::unordered_map<ea::string, unsigned> Shader::fileToIndexMapping;

Shader::Shader(Context* context)
    : Resource(context)
{
    RefreshMemoryUse();
}

Shader::~Shader()
{
    if (!context_.Expired())
    {
        auto* cache = GetSubsystem<ResourceCache>();
        if (cache)
            cache->ResetDependencies(this);
    }
}

void Shader::RegisterObject(Context* context)
{
    context->AddFactoryReflection<Shader>();
}

bool Shader::BeginLoad(Deserializer& source)
{
    auto* graphics = GetSubsystem<Graphics>();
    if (!graphics)
        return false;

    // Load the shader source code and resolve any includes
    ea::string shaderCode;
    FileTime timeStamp{};
    ProcessSource(shaderCode, timeStamp, source);

    // Validate shader code
    if (graphics->GetSettings().validateShaders_)
    {
        static const auto characterMask = GenerateAllowedCharacterMask();
        static const unsigned maxSnippetSize = 5;

        const auto isAllowed = [](char ch) { return ch >= 0 && ch <= 127 && characterMask[ch]; };
        const auto badCharacterIter = ea::find_if_not(shaderCode.begin(), shaderCode.end(), isAllowed);
        if (badCharacterIter != shaderCode.end())
        {
            const auto snippetEnd = FindNth(badCharacterIter, shaderCode.end(), '\n', maxSnippetSize / 2);
            const auto snippetBegin = FindNth(
                ea::make_reverse_iterator(badCharacterIter), shaderCode.rend(), '\n', maxSnippetSize / 2).base();
            const ea::string snippet(snippetBegin, snippetEnd);
            URHO3D_LOGWARNING("Unexpected character #{} '{}' in shader code:\n{}",
                static_cast<unsigned>(static_cast<unsigned char>(*badCharacterIter)), *badCharacterIter, snippet);
        }
    }

    sourceCode_ = ea::move(shaderCode);
    timeStamp_ = timeStamp;

    RefreshMemoryUse();
    return true;
}

bool Shader::EndLoad()
{
    OnReloaded(this);
    return true;
}

ea::string Shader::GetShaderName() const
{
    // TODO: Revisit this in the future, we don't really need GLSL/v2 prefix anymore.
    static const ea::string prefix = "Shaders/GLSL/v2";
    const ea::string& name = GetName();
    if (!name.starts_with(prefix))
    {
        URHO3D_LOGWARNING("Shader '{}' is stored in an unexpected location", name);
        return name;
    }

    return name.substr(prefix.length());
}

ShaderVariation* Shader::GetVariation(ShaderType type, ea::string_view defines)
{
    const ShaderVariationKey key{type, StringHash{defines}};

    const auto iter = variations_.find(key);
    if (iter != variations_.end())
        return iter->second;

    // If shader not found, normalize the defines (to prevent duplicates) and check again. In that case make an alias
    // so that further queries are faster
    const ea::string definesNormalized = NormalizeDefines(defines);
    const ShaderVariationKey keyNormalized{type, StringHash{definesNormalized}};

    const auto iterNormalized = variations_.find(keyNormalized);
    if (iterNormalized != variations_.end())
    {
        variations_.insert(ea::make_pair(key, iterNormalized->second));
        return iterNormalized->second;
    }

    // No shader variation found. Create new.
    auto variation = MakeShared<ShaderVariation>(this, type, definesNormalized);
    variations_.emplace(key, variation);
    ++numVariations_;
    RefreshMemoryUse();

    return variation;
}

void Shader::ProcessSource(ea::string& code, FileTime& timeStamp, Deserializer& source)
{
    auto* cache = GetSubsystem<ResourceCache>();
    auto* vfs = GetSubsystem<VirtualFileSystem>();
    auto* graphics = GetSubsystem<Graphics>();

    const ea::string& fileName = source.GetName();
    // TODO: Support HLSL and MSL shaders.
    const bool isGLSL = true;

    // Add file to index
    unsigned& fileIndex = fileToIndexMapping[fileName];
    if (!fileIndex)
        fileIndex = fileToIndexMapping.size();

    // If the source if a non-packaged file, store the timestamp
    const FileTime sourceTimeStamp = vfs->GetLastModifiedTime(FileIdentifier::FromUri(source.GetName()), false);
    timeStamp = ea::max(timeStamp, sourceTimeStamp);

    // Store resource dependencies for includes so that we know to reload if any of them changes
    if (source.GetName() != GetName())
        cache->StoreResourceDependency(this, source.GetName());

    unsigned numNewLines = 0;
    unsigned currentLine = 1;
    code += FormatLineDirective(isGLSL, fileName, fileIndex, currentLine);
    while (!source.IsEof())
    {
        ea::string line = source.ReadLine();

        if (line.starts_with("#include"))
        {
            ea::string includeFileName = GetPath(source.GetName()) + line.substr(9).replaced("\"", "").trimmed();

            // Add included code or error directive
            AbstractFilePtr includeFile = cache->GetFile(includeFileName);
            if (includeFile)
                ProcessSource(code, timeStamp, *includeFile);
            else
                code += Format("#error Missing include file <{}>\n", includeFileName);

            code += FormatLineDirective(isGLSL, fileName, fileIndex, currentLine);
        }
        else
        {
            const bool isLineContinuation = line.length() >= 1 && line.back() == '\\';
            if (isLineContinuation)
                line.erase(line.end() - 1);

            // If shader validation is enabled, trim comments manually to avoid validating comment contents
            if (!graphics->GetSettings().validateShaders_ || !line.trimmed().starts_with("//"))
                code += line;

            ++numNewLines;
            if (!isLineContinuation)
            {
                // When line continuation chain is over, append skipped newlines to keep line numbers
                for (unsigned i = 0; i < numNewLines; ++i)
                    code += "\n";
                numNewLines = 0;
            }
        }
        ++currentLine;
    }

    // Finally insert an empty line to mark the space between files
    code += "\n";
}

void Shader::RefreshMemoryUse()
{
    SetMemoryUse(
        (unsigned)(sizeof(Shader) +
            sourceCode_.length() +
            numVariations_ * sizeof(ShaderVariation)));
}

ea::string Shader::GetShaderFileList()
{
    ea::vector<ea::pair<ea::string, unsigned>> fileList(fileToIndexMapping.begin(), fileToIndexMapping.end());
    ea::sort(fileList.begin(), fileList.end(),
        [](const ea::pair<ea::string, unsigned>& lhs, const ea::pair<ea::string, unsigned>& rhs)
    {
        return lhs.second < rhs.second;
    });

    ea::string result;
    result += "Shader Files:\n";
    for (const auto& item : fileList)
        result += Format("{}: {}\n", item.second, item.first);
    result += "\n";
    return result;
}

}
