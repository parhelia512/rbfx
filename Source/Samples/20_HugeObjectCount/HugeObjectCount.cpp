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

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/Profiler.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/StaticModelGroup.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/DataComponent.h>
#include <Urho3D/Scene/LogicComponent.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/UI.h>

#include "HugeObjectCount.h"

#include <Urho3D/DebugNew.h>

URHO3D_DEFINE_APPLICATION_MAIN(HugeObjectCount)

class FastSwitcher
{
    URHO3D_DATA_COMPONENT(FastSwitcher);

public:
    static void RegisterAttributes(Context* context)
    {
        URHO3D_ATTRIBUTE("Phase Offset", float, Data().phaseOffset_, 0.0f, AM_DEFAULT);
    }

    void SetPhaseOffset(float phaseOffset) { phaseOffset_ = phaseOffset; time_ = phaseOffset; }

    void Update(Node* node, float timeStep)
    {
        time_ += timeStep;
        if (time_ > 1.0f)
        {
            time_ -= 1.0f;
            auto model = node->GetComponent<StaticModel>();
            model->SetEnabled(!model->IsEnabled());
        }
    }

protected:
    float phaseOffset_{};
    float time_{};
};

ENTT_NAMED_TYPE(FastSwitcher);

class ClassicSwitcher : public LogicComponent, public FastSwitcher
{
    URHO3D_OBJECT(ClassicSwitcher, LogicComponent);

public:
    ClassicSwitcher(Context* context) : LogicComponent(context) {}

    static void RegisterObject(Context* context)
    {
        context->RegisterFactory<ClassicSwitcher>();

        URHO3D_ATTRIBUTE("Phase Offset", float, phaseOffset_, 0.0f, AM_DEFAULT);
    }

    void Update(float timeStep) override
    {
        FastSwitcher::Update(node_, timeStep);
    }
};

HugeObjectCount::HugeObjectCount(Context* context) :
    Sample(context),
    animate_(false),
    useGroups_(false),
    useDataComponents_(false)
{
}

void HugeObjectCount::Start()
{
    // Register things
    ClassicSwitcher::RegisterObject(context_);
    context_->RegisterDataComponentFactory<FastSwitcher>();

    // Execute base class startup
    Sample::Start();

    // Create the scene content
    CreateScene();

    // Create the UI content
    CreateInstructions();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Hook up to the frame update events
    SubscribeToEvents();

    // Set the mouse mode to use in the sample
    Sample::InitMouseMode(MM_RELATIVE);
}

void HugeObjectCount::CreateScene()
{
    auto* cache = GetSubsystem<ResourceCache>();

    if (!scene_)
        scene_ = new Scene(context_);
    else
    {
        scene_->Clear();
        boxNodes_.clear();
    }

    // Create the Octree component to the scene so that drawable objects can be rendered. Use default volume
    // (-1000, -1000, -1000) to (1000, 1000, 1000)
    scene_->CreateComponent<Octree>();

    // Create a Zone for ambient light & fog control
    Node* zoneNode = scene_->CreateChild("Zone");
    auto* zone = zoneNode->CreateComponent<Zone>();
    zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));
    zone->SetFogColor(Color(0.2f, 0.2f, 0.2f));
    zone->SetFogStart(200.0f);
    zone->SetFogEnd(300.0f);

    // Create a directional light
    Node* lightNode = scene_->CreateChild("DirectionalLight");
    lightNode->SetDirection(Vector3(-0.6f, -1.0f, -0.8f)); // The direction vector does not need to be normalized
    auto* light = lightNode->CreateComponent<Light>();
    light->SetLightType(LIGHT_DIRECTIONAL);

    if (!useGroups_)
    {
        light->SetColor(Color(0.7f, 0.35f, 0.0f));

        // Create individual box StaticModels in the scene
        for (int y = -75; y < 75; ++y)
        {
            for (int x = -75; x < 75; ++x)
            {
                Node* boxNode = scene_->CreateChild("Box");
                boxNode->SetPosition(Vector3(x * 0.3f, 0.0f, y * 0.3f));
                boxNode->SetScale(0.25f);
                auto* boxObject = boxNode->CreateComponent<StaticModel>();
                boxObject->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
                boxNodes_.push_back(SharedPtr<Node>(boxNode));

                if (!useDataComponents_)
                {
                    auto rotator = boxNode->CreateComponent<ClassicSwitcher>();
                    rotator->SetPhaseOffset(Random(0.0f, 2.0f));
                }
                else
                {
                    auto rotator = boxNode->CreateDataComponent<FastSwitcher>();
                    rotator->SetPhaseOffset(Random(0.0f, 2.0f));
                }
            }
        }
    }
    else
    {
        light->SetColor(Color(0.6f, 0.6f, 0.6f));
        light->SetSpecularIntensity(1.5f);

        // Create StaticModelGroups in the scene
        StaticModelGroup* lastGroup = nullptr;

        for (int y = -125; y < 125; ++y)
        {
            for (int x = -125; x < 125; ++x)
            {
                // Create new group if no group yet, or the group has already "enough" objects. The tradeoff is between culling
                // accuracy and the amount of CPU processing needed for all the objects. Note that the group's own transform
                // does not matter, and it does not render anything if instance nodes are not added to it
                if (!lastGroup || lastGroup->GetNumInstanceNodes() >= 25 * 25)
                {
                    Node* boxGroupNode = scene_->CreateChild("BoxGroup");
                    lastGroup = boxGroupNode->CreateComponent<StaticModelGroup>();
                    lastGroup->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
                }

                Node* boxNode = scene_->CreateChild("Box");
                boxNode->SetPosition(Vector3(x * 0.3f, 0.0f, y * 0.3f));
                boxNode->SetScale(0.25f);
                boxNodes_.push_back(SharedPtr<Node>(boxNode));
                lastGroup->AddInstanceNode(boxNode);

                if (!useDataComponents_)
                {
                    auto rotator = boxNode->CreateComponent<ClassicSwitcher>();
                    rotator->SetPhaseOffset(Random(0.0f, 2.0f));
                }
                else
                {
                    context_->GetDataComponentFactory<FastSwitcher>()->CreateComponent(boxNode);
                    auto rotator = boxNode->GetDataComponent<FastSwitcher>();
                    //auto rotator = boxNode->CreateDataComponent<FastSwitcher>();
                    rotator->SetPhaseOffset(Random(0.0f, 2.0f));
                }
            }
        }
    }

    // Create the camera. Create it outside the scene so that we can clear the whole scene without affecting it
    if (!cameraNode_)
    {
        cameraNode_ = context_->GetDefaultScene()->CreateChild();
        cameraNode_->SetPosition(Vector3(0.0f, 10.0f, -100.0f));
        auto* camera = cameraNode_->CreateComponent<Camera>();
        camera->SetFarClip(300.0f);
    }
}

void HugeObjectCount::CreateInstructions()
{
    auto* cache = GetSubsystem<ResourceCache>();
    auto* ui = GetSubsystem<UI>();

    // Construct new Text object, set string to display and font to use
    auto* instructionText = ui->GetRoot()->CreateChild<Text>();
    instructionText->SetText(
        "Use WASD keys and mouse/touch to move\n"
        "Space to toggle animation\n"
        "G to toggle object group optimization"
    );
    instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    // The text has multiple rows. Center them in relation to each other
    instructionText->SetTextAlignment(HA_CENTER);

    // Position the text relative to the screen center
    instructionText->SetHorizontalAlignment(HA_CENTER);
    instructionText->SetVerticalAlignment(VA_CENTER);
    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
}

void HugeObjectCount::SetupViewport()
{
    auto* renderer = GetSubsystem<Renderer>();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
    SharedPtr<Viewport> viewport(new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);
}

void HugeObjectCount::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(HugeObjectCount, HandleUpdate));
}

void HugeObjectCount::MoveCamera(float timeStep)
{
    // Do not move if the UI has a focused element (the console)
    if (GetSubsystem<UI>()->GetFocusElement())
        return;

    auto* input = GetSubsystem<Input>();

    // Movement speed as world units per second
    const float MOVE_SPEED = 20.0f;
    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.1f;

    // Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
    IntVector2 mouseMove = input->GetMouseMove();
    yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
    pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
    pitch_ = Clamp(pitch_, -90.0f, 90.0f);

    // Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
    cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    if (input->GetKeyDown(KEY_W))
        cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_S))
        cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_A))
        cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_D))
        cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);
}

void HugeObjectCount::AnimateObjects(float timeStep)
{
    URHO3D_PROFILE("AnimateObjects");

    const float ROTATE_SPEED = 15.0f;
    // Rotate about the Z axis (roll)
    Quaternion rotateQuat(ROTATE_SPEED * timeStep, Vector3::FORWARD);

    for (unsigned i = 0; i < boxNodes_.size(); ++i)
        boxNodes_[i]->Rotate(rotateQuat);
}

void HugeObjectCount::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;

    // Take the frame time step, which is stored as a float
    float timeStep = eventData[P_TIMESTEP].GetFloat();

    // Toggle animation with space
    auto* input = GetSubsystem<Input>();
    if (input->GetKeyPress(KEY_SPACE))
        animate_ = !animate_;

    // Toggle grouped / ungrouped mode
    if (input->GetKeyPress(KEY_G))
    {
        useGroups_ = !useGroups_;
        CreateScene();
    }

    // Toggle classic / data components
    if (input->GetKeyPress(KEY_F))
    {
        useDataComponents_ = !useDataComponents_;
        CreateScene();
    }

    // Move the camera, scale movement with time step
    MoveCamera(timeStep);

    // Update rotators
    if (useDataComponents_)
    {
        scene_->EnumerateDataComponents<FastSwitcher>([&](Node* node, FastSwitcher& rotator)
        {
            rotator.Update(node, timeStep);
        });
    }

    // Animate scene if enabled
    if (animate_)
        AnimateObjects(timeStep);
}
