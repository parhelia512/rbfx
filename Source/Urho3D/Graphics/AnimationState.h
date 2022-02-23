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

/// \file

#pragma once

#include <EASTL/unordered_map.h>

#include "../Container/Ptr.h"
#include "../Graphics/Skeleton.h"
#include "../Math/StringHash.h"
#include "../Math/Transform.h"

namespace Urho3D
{

class Animation;
class AnimationController;
class AnimatedModel;
class Deserializer;
class Node;
class Serializer;
class Serializable;
class Skeleton;
struct AnimationTrack;
struct VariantAnimationTrack;
struct Bone;

/// %Animation blending mode.
enum AnimationBlendMode
{
    // Lerp blending (default)
    ABM_LERP = 0,
    // Additive blending based on difference from bind pose
    ABM_ADDITIVE
};

/// Per-track data of skinned model animation.
/// TODO(animation): Handle Animation reload when tracks are playing?
/// TODO(animation): Do we want per-bone weights?
struct ModelAnimationStateTrack
{
    const AnimationTrack* track_{};
    unsigned boneIndex_{};
    Bone* bone_{};
    WeakPtr<Node> node_;
    // Single AnimationState is never applied to more than one AnimatedModel, so it's okay to have it mutable here.
    mutable unsigned keyFrame_{};
};

/// Output that aggregates all ModelAnimationStateTrack-s targeted at the same node.
struct ModelAnimationOutput
{
    AnimationChannelFlags dirty_;
    Transform localToParent_;
    // Unused by AnimationState, but it's just convinient to have here.
    Matrix3x4 localToComponent_;
};

/// Per-track data of node model animation.
struct NodeAnimationStateTrack
{
    const AnimationTrack* track_{};
    WeakPtr<Node> node_;
    unsigned keyFrame_{};
};

/// Custom attribute type, used to support sub-attribute animation in special cases.
enum class AnimatedAttributeType
{
    Default,
    NodeVariables,
    AnimatedModelMorphs
};

/// Reference to attribute or sub-attribute;
struct AnimatedAttributeReference
{
    WeakPtr<Serializable> serializable_;
    unsigned attributeIndex_{};
    AnimatedAttributeType attributeType_{};
    unsigned subAttributeKey_{};
};

/// Per-track data of attribute animation.
struct AttributeAnimationStateTrack
{
    const VariantAnimationTrack* track_{};
    AnimatedAttributeReference attribute_;
    unsigned keyFrame_{};
};

/// %Animation instance.
class URHO3D_API AnimationState : public RefCounted
{
public:
    /// Construct with animated model and animation pointers.
    AnimationState(AnimationController* controller, AnimatedModel* model);
    /// Construct with root scene node and animation pointers.
    AnimationState(AnimationController* controller, Node* node);
    /// Destruct.
    ~AnimationState() override;
    /// Initialize static properties of the state and dirty tracks if changed.
    void Initialize(Animation* animation, const ea::string& startBone, AnimationBlendMode blendMode);
    /// Update dynamic properies of the state.
    void Update(bool looped, float time, float weight);

    /// Modify tracks. For internal use only.
    /// @{
    bool AreTracksDirty() const;
    void MarkTracksDirty();
    void ClearAllTracks();
    void AddModelTrack(const ModelAnimationStateTrack& track);
    void AddNodeTrack(const NodeAnimationStateTrack& track);
    void AddAttributeTrack(const AttributeAnimationStateTrack& track);
    void OnTracksReady();
    /// @}

    /// Set looping enabled/disabled.
    /// @property
    void SetLooped(bool looped);
    /// Set blending weight.
    /// @property
    void SetWeight(float weight);
    /// Set time position. Does not fire animation triggers.
    /// @property
    void SetTime(float time);

    /// Return animation.
    /// @property
    Animation* GetAnimation() const { return animation_; }

    /// Return animated model this state belongs to (model mode).
    /// @property
    AnimatedModel* GetModel() const;
    /// Return root scene node this state controls (node hierarchy mode).
    /// @property
    Node* GetNode() const;

    /// Return name of start bone.
    const ea::string& GetStartBone() const { return startBone_; }

    /// Return whether weight is nonzero.
    /// @property
    bool IsEnabled() const { return weight_ > 0.0f; }

    /// Return whether looped.
    /// @property
    bool IsLooped() const { return looped_; }

    /// Return blending weight.
    /// @property
    float GetWeight() const { return weight_; }

    /// Return blending mode.
    /// @property
    AnimationBlendMode GetBlendMode() const { return blendingMode_; }

    /// Return time position.
    /// @property
    float GetTime() const { return time_; }

    /// Return animation length.
    /// @property
    float GetLength() const;

    /// Apply animation to a skeleton.
    void CalculateModelTracks(ea::vector<ModelAnimationOutput>& output) const;
    /// Apply animation to a scene node hierarchy.
    void ApplyNodeTracks();
    /// Apply animation to attributes.
    void ApplyAttributeTracks();

private:
    /// Apply value of transformation track to the output.
    void CalulcateTransformTrack(ModelAnimationOutput& output, const AnimationTrack& track, unsigned& frame, float weight) const;
    /// Apply single transformation track to target object. Key frame hint is updated on call.
    void ApplyTransformTrack(const AnimationTrack& track,
        Node* node, Bone* bone, unsigned& frame, float weight, bool silent);
    /// Apply single attribute track to target object. Key frame hint is updated on call.
    void ApplyAttributeTrack(AttributeAnimationStateTrack& stateTrack, float weight);

    /// Owner controller.
    WeakPtr<AnimationController> controller_;
    /// Animated model (model mode).
    WeakPtr<AnimatedModel> model_;
    /// Root scene node (node hierarchy mode).
    WeakPtr<Node> node_;
    /// Animation.
    SharedPtr<Animation> animation_;

    /// Whether the animation state tracks are dirty and should be updated.
    bool tracksDirty_{ true };

    /// Dynamic properties of AnimationState.
    /// @{
    bool looped_{};
    float weight_{};
    float time_{};
    AnimationBlendMode blendingMode_{};
    ea::string startBone_;
    /// @}

    /// Tracks that are actually applied to the objects.
    /// @{
    ea::vector<ModelAnimationStateTrack> modelTracks_;
    ea::vector<NodeAnimationStateTrack> nodeTracks_;
    ea::vector<AttributeAnimationStateTrack> attributeTracks_;
    /// @}
};

using AnimationStateVector = ea::vector<SharedPtr<AnimationState>>;

}
