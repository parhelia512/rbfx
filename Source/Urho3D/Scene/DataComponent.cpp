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

#include "../Precompiled.h"

#include "../Resource/JSONValue.h"
#include "../Resource/XMLElement.h"
#include "../Scene/DataComponent.h"

#include "../DebugNew.h"

#ifdef _MSC_VER
#pragma warning(disable:6293)
#endif

namespace Urho3D
{

DataComponentEventScope::DataComponentEventScope(Scene* scene, bool enable /*= true*/)
    : scene_(scene)
    , wereEnabled_(scene->AreDataComponentEventsEnabled())
{
    scene->SetDataComponentEventsEnabled(enable);
}

void DataComponentEventScope::Reset()
{
    if (scene_)
    {
        scene_->SetDataComponentEventsEnabled(wereEnabled_);
        scene_ = nullptr;
    }
}

DataComponentEventScope::~DataComponentEventScope()
{
    Reset();
}

DataComponentWrapper::DataComponentWrapper(Node* node, DataComponentFactory* factory)
    : Serializable(node->GetContext())
    , node_(node)
    , factory_(factory)
{

}

bool DataComponentWrapper::Save(Serializer& dest) const
{
    // Write type
    if (!dest.WriteString(GetComponentType()))
        return false;

    // Write attributes
    return Serializable::Save(dest);
}

bool DataComponentWrapper::SaveXML(XMLElement& dest) const
{
    // Write type
    if (!dest.SetString("type", GetComponentType()))
        return false;

    // Write attributes
    return Serializable::SaveXML(dest);
}

bool DataComponentWrapper::SaveJSON(JSONValue& dest) const
{
    // Write type
    dest.Set("type", GetComponentType());

    // Write attributes
    return Serializable::SaveJSON(dest);
}

}
