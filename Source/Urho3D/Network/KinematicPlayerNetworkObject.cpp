//
// Copyright (c) 2008-2020 the Urho3D project.
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

#include "../Network/KinematicPlayerNetworkObject.h"

#ifdef URHO3D_PHYSICS
#include "../Core/Context.h"
#include "../Core/CoreEvents.h"
#include "../Physics/KinematicCharacterController.h"
#include "../Physics/PhysicsEvents.h"
#include "../Physics/PhysicsWorld.h"
#include "../Scene/Node.h"
#include "../Scene/Scene.h"
#include "../Network/NetworkEvents.h"

namespace Urho3D
{

KinematicPlayerNetworkObject::KinematicPlayerNetworkObject(Context* context)
    : DefaultNetworkObject(context)
{
}

KinematicPlayerNetworkObject::~KinematicPlayerNetworkObject()
{
}

void KinematicPlayerNetworkObject::RegisterObject(Context* context)
{
    context->RegisterFactory<KinematicPlayerNetworkObject>();
}

void KinematicPlayerNetworkObject::SetWalkVelocity(const Vector3& velocity)
{
    if (GetNetworkMode() == NetworkObjectMode::ClientReplicated)
    {
        URHO3D_LOGWARNING(
            "KinematicPlayerNetworkObject::SetWalkVelocity is called for object {} even tho this client doesn't own it",
            ToString(GetNetworkId()));
        return;
    }

    velocity_ = velocity;
}

void KinematicPlayerNetworkObject::InitializeOnServer()
{
    BaseClassName::InitializeOnServer();

    const auto networkManager = GetServerNetworkManager();
    const unsigned traceCapacity = networkManager->GetTraceCapacity();
    feedbackVelocity_.Resize(traceCapacity);

    SubscribeToEvent(E_BEGINSERVERNETWORKUPDATE, [this](StringHash, VariantMap&) { OnServerNetworkFrameBegin(); });
}

void KinematicPlayerNetworkObject::ReadUnreliableFeedback(unsigned feedbackFrame, Deserializer& src)
{
    const unsigned n = src.ReadVLE();
    for (unsigned i = 0; i < n; ++i)
    {
        const Vector3 newVelocity = src.ReadVector3();
        feedbackVelocity_.Set(feedbackFrame - n + i + 1, newVelocity);
    }
}

void KinematicPlayerNetworkObject::ReadSnapshot(unsigned frame, Deserializer& src)
{
    BaseClassName::ReadSnapshot(frame, src);
    kinematicController_ = node_->GetComponent<KinematicCharacterController>();

    const auto physicsWorld = node_->GetScene()->GetComponent<PhysicsWorld>();
    SubscribeToEvent(physicsWorld, E_PHYSICSPOSTSTEP, [this](StringHash, VariantMap&) { OnPhysicsPostStepOnClient(); });
}

void KinematicPlayerNetworkObject::InterpolateState(
    const NetworkTime& replicaTime, const NetworkTime& inputTime, bool isNewInputFrame)
{
    // Do client-side predictions
    if (kinematicController_ && GetNetworkMode() == NetworkObjectMode::ClientOwned)
    {
        if (isNewInputFrame)
        {
            const float timeStep = 1.0f / GetScene()->GetComponent<PhysicsWorld>()->GetFps(); // TODO(network): Remove before merge!!!
            kinematicController_->SetWalkDirection(velocity_ * timeStep);

            trackNextStepAsFrame_ = inputTime.GetFrame();
        }
        return;
    }

    DefaultNetworkObject::InterpolateState(replicaTime, inputTime, isNewInputFrame);
}

unsigned KinematicPlayerNetworkObject::GetUnreliableFeedbackMask(unsigned frame)
{
    return 1;
}

void KinematicPlayerNetworkObject::WriteUnreliableFeedback(unsigned frame, unsigned mask, Serializer& dest)
{
    inputBuffer_.push_back(velocity_);
    if (inputBuffer_.size() >= 4)
        inputBuffer_.pop_front();

    dest.WriteVLE(inputBuffer_.size());
    for (const Vector3& v : inputBuffer_)
        dest.WriteVector3(v);
}

void KinematicPlayerNetworkObject::ReadUnreliableDeltaPayload(unsigned mask, unsigned frame, Deserializer& src)
{
    BaseClassName::ReadUnreliableDeltaPayload(mask, frame, src);

    if (!kinematicController_)
        return;

    // Skip frames without confirmed data (shouldn't happen too ofter)
    const auto confirmedPosition = GetRawTemporalWorldPosition(frame);
    if (!confirmedPosition)
        return;

    // Skip if no predicted frame (shouldn't happen too ofter as well)
    const auto greaterOrEqual = [&](const auto& frameAndPosition) { return frameAndPosition.first >= frame; };
    const auto iter = ea::find_if(predictedWorldPositions_.begin(), predictedWorldPositions_.end(), greaterOrEqual);
    predictedWorldPositions_.erase(predictedWorldPositions_.begin(), iter);
    if (predictedWorldPositions_.empty() || predictedWorldPositions_.front().first != frame)
        return;

    const Vector3 predictedPosition = predictedWorldPositions_.front().second;
    const Vector3 offset = *confirmedPosition - predictedPosition;
    if (!offset.Equals(Vector3::ZERO, 0.001f))
    {
        const auto networkManager = GetClientNetworkManager();
        const float smoothConstant = networkManager->GetSettings().positionSmoothConstant_;
        kinematicController_->AdjustRawPosition(offset, smoothConstant);
        predictedWorldPositions_.clear();
        //for (auto& [predictionFrame, otherPredictedPosition] : predictedWorldPositions_)
        //    otherPredictedPosition += offset;
    }
}

void KinematicPlayerNetworkObject::OnServerNetworkFrameBegin()
{
    if (AbstractConnection* owner = GetOwnerConnection())
    {
        auto networkManager = GetServerNetworkManager();
        const unsigned feedbackFrame = networkManager->GetCurrentFrame();
        if (const auto newVelocity = feedbackVelocity_.GetRaw(feedbackFrame))
        {
            auto kinematicController = node_->GetComponent<KinematicCharacterController>();
            const float timeStep = 1.0f / GetScene()->GetComponent<PhysicsWorld>()->GetFps(); // TODO(network): Remove before merge!!!
            kinematicController->SetWalkDirection(*newVelocity * timeStep);
        }
    }
}

void KinematicPlayerNetworkObject::OnPhysicsPostStepOnClient()
{
    if (kinematicController_ && trackNextStepAsFrame_)
    {
        predictedWorldPositions_.emplace_back(*trackNextStepAsFrame_, kinematicController_->GetRawPosition());
        trackNextStepAsFrame_ = ea::nullopt;
    }
}

}
#endif
