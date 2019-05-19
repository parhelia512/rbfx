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

#include "../Core/Context.h"
#include "../Scene/Component.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneEvents.h"

namespace Urho3D
{

class DataComponentFactory;

/// Helper class to temporarily enable/disable data component events.
class URHO3D_API DataComponentEventScope
{
public:
    /// Construct.
    explicit DataComponentEventScope(Scene* scene, bool enable = true);
    /// Reset.
    void Reset();
    /// Destruct.
    ~DataComponentEventScope();

private:
    /// Scene.
    WeakPtr<Scene> scene_{};
    /// Whether the events were enabled before.
    bool wereEnabled_{};
};

/// Data component wrapper.
class URHO3D_API DataComponentWrapper : public Serializable
{
    URHO3D_OBJECT(DataComponentWrapper, Serializable);

public:
    /// Construct.
    DataComponentWrapper(Node* node, DataComponentFactory* factory);

    /// Save as binary data. Return true if successful.
    bool Save(Serializer& dest) const override;
    /// Save as XML data. Return true if successful.
    bool SaveXML(XMLElement& dest) const override;
    /// Save as JSON data. Return true if successful.
    bool SaveJSON(JSONValue& dest) const override;

    /// Remove component from the node.
    virtual void Remove() = 0;
    /// Return component name.
    virtual const ea::string& GetComponentType() const = 0;
    /// Return whether the component is expired.
    virtual bool IsComponentExpired() const = 0;

    /// Return scene.
    Scene* GetScene() const { return node_->GetScene(); }
    /// Return node (safe). May return null.
    Node* GetNode() const { return node_.Get(); }
    /// Return factory.
    DataComponentFactory* GetFactory() const { return factory_; }

private:
    /// Weak pointer to the node.
    WeakPtr<Node> node_;
    /// Pointer to component factory used as data component type ID.
    WeakPtr<DataComponentFactory> factory_;
};

/// Data component wrapper implementation.
template <class T>
class DataComponentWrapperImpl : public DataComponentWrapper
{
public:
    /// Use base constructor.
    using DataComponentWrapper::DataComponentWrapper;
    /// Register object.
    static void RegisterObject(Urho3D::Context* context) { T::RegisterAttributes(context); }
    /// Remove component from the node.
    void Remove() override { GetNode()->template RemoveDataComponent<T>(); }
    /// Return component name.
    const ea::string& GetComponentType() const override { return T::GetTypeNameStatic(); }
    /// Return whether the component is expired.
    bool IsComponentExpired() const override { return !GetNode() || !GetNode()->template HasDataComponent<T>(); }
    /// Get component data (mutable).
    T& Data() { return *GetNode()->template GetDataComponent<T>(); }
    /// Get component data (constant).
    const T& Data() const { return *GetNode()->template GetDataComponent<T>(); }
};

/// Factory and dynamic manager for compile-time data components.
class URHO3D_API DataComponentFactory : public RefCounted
{
public:
    /// Register wrapper object in the context.
    virtual void RegisterWrapperObject(Context* context) const = 0;
    /// Connect scene to events.
    virtual void ConnectSceneToEvents(Scene* scene) const = 0;
    /// Disconnect scene from events.
    virtual void DisconnectSceneFromEvents(Scene* scene) const = 0;
    /// Get EnTT type index of the component.
    virtual unsigned GetComponentTypeIndex() const = 0;
    /// Get data component type name.
    virtual const ea::string& GetComponentTypeName() const = 0;
    /// Create data component for given node.
    virtual void CreateComponent(Node* node) = 0;
    /// Return whether the node has given component.
    virtual bool HasComponent(const Node* node) const = 0;
    /// Destroy data component for given node.
    virtual bool RemoveComponent(Node* node) = 0;
    /// Create data component wrapper.
    virtual SharedPtr<DataComponentWrapper> CreateWrapper(Node* node) = 0;
};

/// Use this macro inside data component class or structure.
/// Data component must have method `static void RegisterAttributes(Context* context)` similar to `RegisterObject`.
/// Use `Data().` prefix while declaring attributes, e.g. `URHO3D_ATTRIBUTE("Float Value", float, Data().floatValue_, 0.0f, AM_DEFAULT)`
#define URHO3D_DATA_COMPONENT(typeName) \
    public: \
        static const ea::string& GetTypeNameStatic() { static const ea::string typeName_ = #typeName; return typeName_; } \
        class typeName##_Wrapper : public Urho3D::DataComponentWrapperImpl<typeName> \
        { \
            URHO3D_OBJECT(typeName##_Wrapper, Urho3D::DataComponentWrapper); \
            using Urho3D::DataComponentWrapperImpl<typeName>::DataComponentWrapperImpl; \
        }; \
        using SerializableWrapper = typeName##_Wrapper; \
        using ClassName = SerializableWrapper

/// Implementation of data component factory.
template <class ComponentType>
class DataComponentFactoryImpl : public DataComponentFactory
{
public:
    /// Component wrapper type.
    using WrapperType = typename ComponentType::SerializableWrapper;

    /// Register wrapper object in the context.
    void RegisterWrapperObject(Context* context) const override { WrapperType::RegisterObject(context); }

    /// Connect scene to events.
    void ConnectSceneToEvents(Scene* scene) const override
    {
        scene->GetRegistry().on_construct<ComponentType>().template connect<&Scene::DataComponentAdded<ComponentType>>(scene);
        scene->GetRegistry().on_destroy<ComponentType>().template connect<&Scene::DataComponentRemoved<ComponentType>>(scene);
    }

    /// Disconnect scene from events.
    void DisconnectSceneFromEvents(Scene* scene) const override
    {
        scene->GetRegistry().on_construct<ComponentType>().template disconnect<&Scene::DataComponentAdded<ComponentType>>(scene);
        scene->GetRegistry().on_destroy<ComponentType>().template disconnect<&Scene::DataComponentRemoved<ComponentType>>(scene);
    }

    /// Get EnTT type index of the component.
    unsigned GetComponentTypeIndex() const override { return entt::registry::type<ComponentType>(); }

    /// Get component type name.
    const ea::string& GetComponentTypeName() const override { return ComponentType::GetTypeNameStatic(); }

    /// Create component for given node.
    void CreateComponent(Node* node) override
    {
        node->CreateDataComponent<ComponentType>();
    }

    /// Return whether the node has given component.
    bool HasComponent(const Node* node) const override
    {
        return node->HasDataComponent<ComponentType>();
    }

    /// Destroy component for given node.
    bool RemoveComponent(Node* node) override
    {
        return node->RemoveDataComponent<ComponentType>();
    }

    /// Create data component wrapper.
    SharedPtr<DataComponentWrapper> CreateWrapper(Node* node) override
    {
        return MakeShared<WrapperType>(node, WeakPtr<DataComponentFactory>(this));
    }
};

template <class T> void Context::RegisterDataComponentFactory()
{
    auto factory = MakeShared<DataComponentFactoryImpl<T>>();
    RegisterDataComponentFactory(factory);
}

template <class T> DataComponentFactory* Context::GetDataComponentFactory() const
{
    return GetDataComponentFactory(entt::registry::type<T>());
}

template <class T, class ... Args> T* Node::CreateDataComponent(Args && ... args)
{
    if (!IsRegistryValid())
    {
        assert(0);
        return nullptr;
    }

    entt::registry& reg = scene_->GetRegistry();
    dataComponentWrappersDirty_ = true;
    return &reg.assign_or_replace<T>(entity_, std::forward<Args>(args)...);
}

template <class T> T* Node::GetDataComponent()
{
    if (!IsRegistryValid())
    {
        assert(0);
        return nullptr;
    }

    entt::registry& reg = scene_->GetRegistry();
    return reg.try_get<T>(entity_);
}

template <class T> const T* Node::GetDataComponent() const
{
    if (!IsRegistryValid())
    {
        assert(0);
        return nullptr;
    }

    entt::registry& reg = scene_->GetRegistry();
    return reg.try_get<T>(entity_);
}

template <class T> bool Node::HasDataComponent() const
{
    if (!IsRegistryValid())
    {
        assert(0);
        return false;
    }

    entt::registry& reg = scene_->GetRegistry();
    return reg.has<T>(entity_);
}

template <class T> bool Node::RemoveDataComponent()
{
    if (!IsRegistryValid())
    {
        assert(0);
        return false;
    }

    entt::registry& reg = scene_->GetRegistry();
    if (reg.has<T>(entity_))
    {
        reg.remove<T>(entity_);
        dataComponentWrappersDirty_ = true;
        return true;
    }
    return false;
}

template <class T>
void Scene::DataComponentAdded(entt::registry& registry, entt::entity entity, T& /*component*/)
{
    using namespace Urho3D::DataComponentAdded;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_SCENE] = this;
    eventData[P_NODE] = GetNodeByEntityID(entity);
    eventData[P_DATACOMPONENTTYPE] = T::GetTypeNameStatic();

    SendEvent(E_DATACOMPONENTADDED, eventData);
}

template <class T>
void Scene::DataComponentRemoved(entt::registry& registry, entt::entity entity)
{
    using namespace Urho3D::DataComponentRemoved;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_SCENE] = this;
    eventData[P_NODE] = GetNodeByEntityID(entity);
    eventData[P_DATACOMPONENTTYPE] = T::GetTypeNameStatic();

    SendEvent(E_DATACOMPONENTREMOVED, eventData);
}

}
