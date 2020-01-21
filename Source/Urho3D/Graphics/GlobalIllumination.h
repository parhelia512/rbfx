//
// Copyright (c) 2019-2020 the rbfx project.
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

#include "../Graphics/LightProbeGroup.h"
#include "../Math/Matrix3.h"
#include "../Math/Sphere.h"
#include "../Math/TetrahedralMesh.h"
#include "../Math/Vector3.h"
#include "../Scene/Component.h"

namespace Urho3D
{

/// Global illumination manager.
class URHO3D_API GlobalIllumination : public Component
{
    URHO3D_OBJECT(GlobalIllumination, Component);

public:
    /// Construct.
    explicit GlobalIllumination(Context* context);
    /// Destruct.
    ~GlobalIllumination() override;
    /// Register object factory. Drawable must be registered first.
    static void RegisterObject(Context* context);
    /// Visualize the component as debug geometry.
    void DrawDebugGeometry(DebugRenderer* debug, bool depthTest) override;

    /// Reset light probes.
    void ResetLightProbes();
    /// Compile all enabled light probe groups in the scene.
    void CompileLightProbes();

    /// Sample ambient spherical harmonics.
    SphericalHarmonicsDot9 SampleAmbientSH(const Vector3& position, unsigned& hint) const;
    /// Sample average ambient lighting.
    Vector3 SampleAverageAmbient(const Vector3& position, unsigned& hint) const;

    /// Set background static.
    void SetBackgroundStatic(bool backgroundStatic) { backgroundStatic_ = backgroundStatic; }
    /// Return whether the background is static.
    bool GetBackgroundStatic() const { return backgroundStatic_; }
    /// Set background brightness.
    void SetBackgroundBrightness(float brightness) { backgroundBrightness_ = brightness; }
    /// Return background brightness.
    float GetBackgroundBrightness() const { return backgroundBrightness_; }

    /// Serialize light probes data.
    void SerializeLightProbesData(Archive& archive);
    /// Set serialized light probes data.
    void SetLightProbesData(const ea::string& data);
    /// Return serialized light probes data.
    ea::string GetLightProbesData() const;

private:
    /// Whether the background (Zone and Skybox) is static.
    bool backgroundStatic_{};
    /// Background brightness multiplier.
    float backgroundBrightness_{};

    /// Light probes mesh.
    TetrahedralMesh lightProbesMesh_;
    /// Baked light probes data.
    LightProbeCollectionBakedData lightProbesBakedData_;
};

}
