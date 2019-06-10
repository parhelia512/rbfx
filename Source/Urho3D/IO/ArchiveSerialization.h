//
// Copyright (c) 2008-2019 the Urho3D project.
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

#pragma once

#include "../Core/StringUtils.h"
#include "../Core/Variant.h"
#include "../IO/Archive.h"
#include "../Math/Color.h"
#include "../Math/Matrix3.h"
#include "../Math/Matrix3x4.h"
#include "../Math/Matrix4.h"
#include "../Math/Rect.h"
#include "../Math/Vector2.h"
#include "../Math/Vector3.h"
#include "../Math/Vector4.h"
#include "../Math/Quaternion.h"

#include <EASTL/string.h>

#include <type_traits>

namespace Urho3D
{

namespace Detail
{

/// Format float array to string.
inline ea::string FormatArray(float* values, unsigned size)
{
    ea::string result;
    for (unsigned i = 0; i < size; ++i)
    {
        if (i > 0)
            result += " ";
        result += ea::string(ea::string::CtorSprintf(), "%g", values[i]);
    }
    return result;
}

/// Format int array to string.
inline ea::string FormatArray(int* values, unsigned size)
{
    ea::string result;
    for (unsigned i = 0; i < size; ++i)
    {
        if (i > 0)
            result += " ";
        result += ea::to_string(values[i]);
    }
    return result;
}

/// Un-format float array from string.
inline unsigned UnFormatArray(const ea::string& string, float* values, unsigned maxSize)
{
    const ea::vector<ea::string> elements = string.split(' ');
    const unsigned size = elements.size() < maxSize ? elements.size() : maxSize;
    for (unsigned i = 0; i < size; ++i)
        values[i] = ToFloat(elements[i]);

    return elements.size();
}

/// Un-format int array from string.
inline unsigned UnFormatArray(const ea::string& string, int* values, unsigned maxSize)
{
    const ea::vector<ea::string> elements = string.split(' ');
    const unsigned size = elements.size() < maxSize ? elements.size() : maxSize;
    for (unsigned i = 0; i < size; ++i)
        values[i] = ToInt(elements[i]);

    return elements.size();
}

/// Serialize array of fixed size.
template <class T>
inline bool SerializeArray(Archive& archive, const char* name, T* values, unsigned size)
{
    if (!archive.IsHumanReadable())
        return archive.SerializeBytes(name, values, size * sizeof(T));
    else
    {
        if (archive.IsInput())
        {
            ea::string string;
            if (!archive.Serialize(name, string))
                return false;

            const unsigned realSize = Detail::UnFormatArray(string, values, size);
            return realSize == size;
        }
        else
        {
            ea::string string = Detail::FormatArray(values, size);
            return archive.Serialize(name, string);
        }
    }
}

/// Serialize type as fixed array.
template <unsigned N, class T>
inline bool SerializeArrayType(Archive& archive, const char* name, T& value)
{
    using ElementType = std::decay_t<decltype(*value.Data())>;

    if (archive.IsInput())
    {
        ElementType elements[N];
        if (!SerializeArray(archive, name, elements, N))
            return false;

        value = T{ elements };
        return true;
    }
    else
    {
        return SerializeArray(archive, name, const_cast<ElementType*>(value.Data()), N);
    }
}

/// Serialize value of the Variant (of specific type).
template <class T>
inline bool SerializeVariantValueType(Archive& archive, const char* name, Variant& variant)
{
    if (archive.IsInput())
    {
        T value{};
        if (!SerializeValue(archive, name, value))
            return false;
        variant = value;
        return true;
    }
    else
    {
        const T& value = variant.Get<T>();
        return SerializeValue(archive, name, const_cast<T&>(value));
    }
}

/// Format ResourceRefList to string.
inline ea::string FormatResourceRefList(ea::string_view typeString, const ea::string* names, unsigned size)
{
    ea::string result{ typeString };
    for (unsigned i = 0; i < size; ++i)
    {
        result += ";";
        result += names[i];
    }
    return result;
}

}

/// Serialize value directly using archive.
template <class T>
inline bool SerializeValue(Archive& archive, const char* name, T& value)
{
    return archive.Serialize(name, value);
}

/// Serialize Vector2.
inline bool SerializeValue(Archive& archive, const char* name, Vector2& value)
{
    return Detail::SerializeArrayType<2>(archive, name, value);
}

/// Serialize Vector3.
inline bool SerializeValue(Archive& archive, const char* name, Vector3& value)
{
    return Detail::SerializeArrayType<3>(archive, name, value);
}

/// Serialize Vector4.
inline bool SerializeValue(Archive& archive, const char* name, Vector4& value)
{
    return Detail::SerializeArrayType<4>(archive, name, value);
}

/// Serialize Matrix3.
inline bool SerializeValue(Archive& archive, const char* name, Matrix3& value)
{
    return Detail::SerializeArrayType<9>(archive, name, value);
}

/// Serialize Matrix3x4.
inline bool SerializeValue(Archive& archive, const char* name, Matrix3x4& value)
{
    return Detail::SerializeArrayType<12>(archive, name, value);
}

/// Serialize Matrix4.
inline bool SerializeValue(Archive& archive, const char* name, Matrix4& value)
{
    return Detail::SerializeArrayType<16>(archive, name, value);
}

/// Serialize Rect.
inline bool SerializeValue(Archive& archive, const char* name, Rect& value)
{
    return Detail::SerializeArrayType<4>(archive, name, value);
}

/// Serialize Quaternion.
inline bool SerializeValue(Archive& archive, const char* name, Quaternion& value)
{
    return Detail::SerializeArrayType<4>(archive, name, value);
}

/// Serialize Color.
inline bool SerializeValue(Archive& archive, const char* name, Color& value)
{
    return Detail::SerializeArrayType<4>(archive, name, value);
}

/// Serialize IntVector2.
inline bool SerializeValue(Archive& archive, const char* name, IntVector2& value)
{
    return Detail::SerializeArrayType<2>(archive, name, value);
}

/// Serialize IntVector3.
inline bool SerializeValue(Archive& archive, const char* name, IntVector3& value)
{
    return Detail::SerializeArrayType<3>(archive, name, value);
}

/// Serialize IntRect.
inline bool SerializeValue(Archive& archive, const char* name, IntRect& value)
{
    return Detail::SerializeArrayType<4>(archive, name, value);
}

/// Serialize StringHash (as is).
inline bool SerializeValue(Archive& archive, const char* name, StringHash& value)
{
    unsigned hashValue = value.Value();
    if (!SerializeValue(archive, name, hashValue))
        return false;
    value = StringHash{ hashValue };
    return true;
}

/// Serialize enum as integer (if binary archive) or as string (if text archive).
template <class EnumType, class UnderlyingInteger = std::underlying_type_t<EnumType>>
inline bool SerializeEnum(Archive& archive, const char* name, const char* const* enumConstants, EnumType& value)
{
    const bool loading = archive.IsInput();
    if (!archive.IsHumanReadable())
    {
        if (loading)
        {
            UnderlyingInteger intValue{};
            if (!SerializeValue(archive, name, intValue))
                return false;
            value = static_cast<EnumType>(intValue);
            return true;
        }
        else
        {
            UnderlyingInteger intValue = static_cast<UnderlyingInteger>(value);
            if (!SerializeValue(archive, name, intValue))
                return false;
            return true;
        }
    }
    else
    {
        if (!enumConstants)
        {
            assert(0);
            return false;
        }
        if (loading)
        {
            ea::string stringValue;
            if (!SerializeValue(archive, name, stringValue))
                return false;
            value = static_cast<EnumType>(GetStringListIndex(stringValue.c_str(), enumConstants, 0));
            return true;
        }
        else
        {
            // This is so unsafe
            ea::string stringValue = enumConstants[static_cast<UnderlyingInteger>(value)];
            return SerializeValue(archive, name, stringValue);
        }
    }
}

/// Serialize string hash as integer or as string.
inline bool SerializeStringHash(Archive& archive, const char* name, StringHash& value, const ea::string_view string)
{
    if (!archive.IsHumanReadable())
    {
        unsigned hashValue = value.Value();
        if (!SerializeValue(archive, name, hashValue))
            return false;
        value = StringHash{ hashValue };
    }
    else
    {
        if (archive.IsInput())
        {
            ea::string stringValue;
            if (!SerializeValue(archive, name, stringValue))
                return false;
            value = StringHash{ stringValue };
        }
        else
        {
            ea::string stringValue{ string };
            if (!SerializeValue(archive, name, stringValue))
                return false;
        }
    }
    return true;
}

/// Serialize vector with standard interface.
template <class T>
inline bool SerializeVector(Archive& archive, const char* name, const char* element, T& vector)
{
    using ValueType = typename T::value_type;
    if (auto block = archive.OpenArrayBlock(name, vector.size()))
    {
        if (archive.IsInput())
        {
            vector.clear();
            vector.resize(block.GetSizeHint());
            for (unsigned i = 0; i < block.GetSizeHint(); ++i)
            {
                if (!SerializeValue(archive, element, vector[i]))
                    return false;
            }
            return true;
        }
        else
        {
            for (ValueType& value : vector)
            {
                if (!SerializeValue(archive, element, value))
                    return false;
            }
            return true;
        }
    }
    return false;
}

/// Serialize vector as byte array, if possible.
template <class T>
inline bool SerializeVectorBytes(Archive& archive, const char* name, const char* element, T& vector)
{
    using ValueType = typename T::value_type;
    static_assert(std::is_standard_layout<ValueType>::value, "Type should have standard layout to safely use byte serialization");
    static_assert(std::is_trivially_copyable<ValueType>::value, "Type should be trivially copyable to safely use byte serialization");

    if (archive.IsHumanReadable())
        return SerializeVector(archive, name, element, vector);
    else
    {
        if (ArchiveBlock block = archive.OpenUnorderedBlock(name))
        {
            if (archive.IsInput())
            {
                unsigned sizeInBytes{};
                if (!archive.SerializeVLE("size", sizeInBytes))
                    return false;

                if (sizeInBytes % sizeof(ValueType) != 0)
                    return false;

                vector.resize(sizeInBytes / sizeof(ValueType));
                if (!archive.SerializeBytes("data", vector.data(), sizeInBytes))
                    return false;
            }
            else
            {
                unsigned sizeInBytes = vector.size() * sizeof(ValueType);
                if (!archive.SerializeVLE("size", sizeInBytes))
                    return false;

                if (!archive.SerializeBytes("data", vector.data(), sizeInBytes))
                    return false;
            }
            return true;
        }
        return false;
    }
}

/// Serialize custom vector.
/// While writing, serializer may skip vector elements. Size should match actual number of elements to be written.
/// While reading, serializer must push elements into vector on its own.
template <class T, class U>
inline bool SerializeCustomVector(Archive& archive, const char* name, unsigned size, T& vector, U serializer)
{
    using ValueType = typename T::value_type;
    if (auto block = archive.OpenArrayBlock(name, size))
    {
        if (archive.IsInput())
        {
            for (unsigned i = 0; i < block.GetSizeHint(); ++i)
            {
                ValueType element;
                if (!serializer(element, true))
                    return false;
            }
            return true;
        }
        else
        {
            for (ValueType& value : vector)
            {
                if (!serializer(value, false))
                    return false;
            }
            return true;
        }
    }
    return false;
}

/// Serialize map or hash map with string key with standard interface.
template <class T>
inline bool SerializeStringMap(Archive& archive, const char* name, const char* element, T& map)
{
    using ValueType = typename T::mapped_type;
    if (auto block = archive.OpenMapBlock(name, map.size()))
    {
        if (archive.IsInput())
        {
            map.clear();
            for (int i = 0; i < block.GetSizeHint(); ++i)
            {
                ea::string key{};
                ValueType value{};
                archive.SerializeKey(key);
                if (!SerializeValue(archive, element, value))
                    return false;
                map.emplace(std::move(key), std::move(value));
            }
            return true;
        }
        else
        {
            for (auto& keyValue : map)
            {
                archive.SerializeKey(const_cast<ea::string&>(keyValue.first));
                SerializeValue(archive, element, keyValue.second);
            }
            return true;
        }
    }
    return false;
}

/// Serialize map or hash map with unsigned integer key with standard interface.
template <class T>
inline bool SerializeStringHashMap(Archive& archive, const char* name, const char* element, T& map)
{
    using ValueType = typename T::mapped_type;
    if (auto block = archive.OpenMapBlock(name, map.size()))
    {
        if (archive.IsInput())
        {
            map.clear();
            for (int i = 0; i < block.GetSizeHint(); ++i)
            {
                ValueType value{};
                unsigned key{};
                archive.SerializeKey(key);
                if (!SerializeValue(archive, element, value))
                    return false;
                map.emplace(StringHash{ key }, std::move(value));
            }
            return true;
        }
        else
        {
            for (auto& keyValue : map)
            {
                unsigned key = keyValue.first.Value();
                archive.SerializeKey(key);
                SerializeValue(archive, element, keyValue.second);
            }
            return true;
        }
    }
    return false;
}

/// Serialize ResourceRef.
inline bool SerializeValue(Archive& archive, const char* name, ResourceRef& value)
{
    if (!archive.IsHumanReadable())
    {
        if (ArchiveBlock block = archive.OpenUnorderedBlock(name))
        {
            bool success = true;
            success &= SerializeValue(archive, "type", value.type_);
            success &= SerializeValue(archive, "name", value.name_);
            return success;
        }
        return false;
    }
    else
    {
        Context* context = archive.GetContext();
        if (!context)
            return false;

        if (archive.IsInput())
        {
            ea::string stringValue;
            if (!SerializeValue(archive, name, stringValue))
                return false;

            ea::vector<ea::string> chunks = stringValue.split(';');
            if (chunks.size() != 2)
                return false;

            value.type_ = chunks[0];
            value.name_ = chunks[1];
        }
        else
        {
            ea::string stringValue = Detail::FormatResourceRefList(context->GetTypeName(value.type_), &value.name_, 1);
            if (!SerializeValue(archive, name, stringValue))
                return false;
        }
        return true;
    }
}

/// Serialize ResourceRefList.
inline bool SerializeValue(Archive& archive, const char* name, ResourceRefList& value)
{
    if (!archive.IsHumanReadable())
    {
        if (ArchiveBlock block = archive.OpenUnorderedBlock(name))
        {
            bool success = true;
            success &= SerializeValue(archive, "type", value.type_);
            success &= SerializeVector(archive, "name", "element", value.names_);
            return success;
        }
        return false;
    }
    else
    {
        Context* context = archive.GetContext();
        if (!context)
            return false;

        if (archive.IsInput())
        {
            ea::string stringValue;
            if (!SerializeValue(archive, name, stringValue))
                return false;

            ea::vector<ea::string> chunks = stringValue.split(';');
            if (chunks.size() != 2)
                return false;

            value.type_ = chunks[0];
            chunks.pop_front();
            value.names_ = std::move(chunks);
        }
        else
        {
            ea::string stringValue = Detail::FormatResourceRefList(context->GetTypeName(value.type_), value.names_.data(), value.names_.size());
            if (!SerializeValue(archive, name, stringValue))
                return false;
        }
        return true;
    }
}

/// Serialize value of the Variant.
URHO3D_API bool SerializeVariantValue(Archive& archive, VariantType variantType, const char* name, Variant& value);

/// Serialize Variant.
inline bool SerializeValue(Archive& archive, const char* name, Variant& value)
{
    if (ArchiveBlock block = archive.OpenUnorderedBlock(name))
    {
        VariantType variantType = value.GetType();
        if (!SerializeEnum(archive, "type", Variant::GetTypeNameList(), variantType))
            return false;

        return SerializeVariantValue(archive, variantType, "value", value);
    }
    return false;
}

}
