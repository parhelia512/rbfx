//
// Copyright (c) 2017-2021 the rbfx project.
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
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR rhs
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR rhsWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR rhs DEALINGS IN
// THE SOFTWARE.
//

#include "../CommonUtils.h"

#include <Urho3D/Replica/NetworkValue.h>

namespace Urho3D
{

using DynamicFloat = ValueWithDerivative<float>;

bool operator==(const ea::optional<DynamicFloat>& lhs, float rhs)
{
    return lhs && lhs->value_ == rhs;
}

}

namespace
{

template <class T>
void Set(NetworkValueVector<T>& dest, unsigned frame, std::initializer_list<T> value)
{
    dest.Set(frame, {value.begin(), static_cast<unsigned>(value.size())});
}

template <class T>
bool IsSame(ea::span<const T> lhs, std::initializer_list<T> rhs)
{
    return ea::identical(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <class T>
bool IsSame(ea::optional<ea::span<const T>> lhs, std::initializer_list<T> rhs)
{
    return lhs && IsSame(*lhs, rhs);
}

template <class T>
bool IsSame(InterpolatedConstSpan<T> lhs, std::initializer_list<T> rhs)
{
    if (lhs.Size() != rhs.size())
        return false;
    for (unsigned i = 0; i < rhs.size(); ++i)
    {
        if (lhs[i] != rhs.begin()[i])
            return false;
    }
    return true;
}

template <class T>
bool IsSame(ea::optional<InterpolatedConstSpan<T>> lhs, std::initializer_list<T> rhs)
{
    return lhs && IsSame(*lhs, rhs);
}

}

TEST_CASE("NetworkValue is updated and sampled")
{
    NetworkValue<float> v;
    v.Resize(5);

    REQUIRE_FALSE(v.GetRaw(1));
    REQUIRE_FALSE(v.GetRaw(2));
    REQUIRE_FALSE(v.GetRaw(3));
    REQUIRE_FALSE(v.GetRaw(4));
    REQUIRE_FALSE(v.GetRaw(5));

    v.Set(2, 1000.0f);

    REQUIRE_FALSE(v.GetRaw(1));
    REQUIRE(v.GetRaw(2) == 1000.0f);
    REQUIRE_FALSE(v.GetRaw(3));
    REQUIRE_FALSE(v.GetRaw(4));
    REQUIRE_FALSE(v.GetRaw(5));

    REQUIRE(v.GetClosestRaw(1) == 1000.0f);
    REQUIRE(v.GetClosestRaw(2) == 1000.0f);
    REQUIRE(v.GetClosestRaw(5) == 1000.0f);

    REQUIRE(v.SampleValid(NetworkTime{1, 0.5f}) == 1000.0f);
    REQUIRE(v.SampleValid(NetworkTime{2, 0.0f}) == 1000.0f);
    REQUIRE(v.SampleValid(NetworkTime{2, 0.5f}) == 1000.0f);

    v.Set(2, 2000.0f);

    REQUIRE_FALSE(v.GetRaw(1));
    REQUIRE(v.GetRaw(2) == 2000.0f);
    REQUIRE_FALSE(v.GetRaw(3));
    REQUIRE_FALSE(v.GetRaw(4));
    REQUIRE_FALSE(v.GetRaw(5));

    REQUIRE(v.GetClosestRaw(1) == 2000.0f);
    REQUIRE(v.GetClosestRaw(2) == 2000.0f);
    REQUIRE(v.GetClosestRaw(5) == 2000.0f);

    REQUIRE(v.SampleValid(NetworkTime{1, 0.5f}) == 2000.0f);
    REQUIRE(v.SampleValid(NetworkTime{2, 0.0f}) == 2000.0f);
    REQUIRE(v.SampleValid(NetworkTime{2, 0.5f}) == 2000.0f);

    v.Set(4, 4000.0f);

    REQUIRE_FALSE(v.GetRaw(1));
    REQUIRE(v.GetRaw(2) == 2000.0f);
    REQUIRE_FALSE(v.GetRaw(3));
    REQUIRE(v.GetRaw(4) == 4000.0f);
    REQUIRE_FALSE(v.GetRaw(5));

    REQUIRE(v.GetClosestRaw(1) == 2000.0f);
    REQUIRE(v.GetClosestRaw(2) == 2000.0f);
    REQUIRE(v.GetClosestRaw(3) == 2000.0f);
    REQUIRE(v.GetClosestRaw(4) == 4000.0f);
    REQUIRE(v.GetClosestRaw(5) == 4000.0f);

    REQUIRE(v.SampleValid(NetworkTime{1, 0.5f}) == 2000.0f);
    REQUIRE(v.SampleValid(NetworkTime{2, 0.0f}) == 2000.0f);
    REQUIRE(v.SampleValid(NetworkTime{2, 0.5f}) == 2500.0f);
    REQUIRE(v.SampleValid(NetworkTime{3, 0.0f}) == 3000.0f);
    REQUIRE(v.SampleValid(NetworkTime{3, 0.5f}) == 3500.0f);
    REQUIRE(v.SampleValid(NetworkTime{4, 0.0f}) == 4000.0f);
    REQUIRE(v.SampleValid(NetworkTime{4, 0.5f}) == 4000.0f);

    v.Set(3, 3000.0f);
    v.Set(5, 5000.0f);
    v.Set(6, 6000.0f);

    REQUIRE_FALSE(v.GetRaw(1));
    REQUIRE(v.GetRaw(2) == 2000.0f);
    REQUIRE(v.GetRaw(3) == 3000.0f);
    REQUIRE(v.GetRaw(4) == 4000.0f);
    REQUIRE(v.GetRaw(5) == 5000.0f);
    REQUIRE(v.GetRaw(6) == 6000.0f);

    REQUIRE(v.GetClosestRaw(5) == 5000.0f);
    REQUIRE(v.GetClosestRaw(6) == 6000.0f);
    REQUIRE(v.GetClosestRaw(7) == 6000.0f);

    REQUIRE(v.SampleValid(NetworkTime{1, 0.5f}) == 2000.0f);
    REQUIRE(v.SampleValid(NetworkTime{2, 0.0f}) == 2000.0f);
    REQUIRE(v.SampleValid(NetworkTime{2, 0.5f}) == 2500.0f);
    REQUIRE(v.SampleValid(NetworkTime{3, 0.0f}) == 3000.0f);
    REQUIRE(v.SampleValid(NetworkTime{3, 0.5f}) == 3500.0f);
    REQUIRE(v.SampleValid(NetworkTime{4, 0.0f}) == 4000.0f);
    REQUIRE(v.SampleValid(NetworkTime{4, 0.5f}) == 4500.0f);

    REQUIRE(v.SampleValid(NetworkTime{5, 0.75f}) == 5750.0f);
    REQUIRE(v.SampleValid(NetworkTime{6, 0.0f}) == 6000.0f);
    REQUIRE(v.SampleValid(NetworkTime{6, 0.5f}) == 6000.0f);

    v.Set(9, 9000.0f);

    REQUIRE_FALSE(v.GetRaw(1));
    REQUIRE_FALSE(v.GetRaw(2));
    REQUIRE_FALSE(v.GetRaw(3));
    REQUIRE_FALSE(v.GetRaw(4));
    REQUIRE(v.GetRaw(5) == 5000.0f);
    REQUIRE(v.GetRaw(6) == 6000.0f);
    REQUIRE_FALSE(v.GetRaw(7));
    REQUIRE_FALSE(v.GetRaw(8));
    REQUIRE(v.GetRaw(9) == 9000.0f);

    REQUIRE(v.GetClosestRaw(4) == 5000.0f);
    REQUIRE(v.GetClosestRaw(5) == 5000.0f);
    REQUIRE(v.GetClosestRaw(6) == 6000.0f);
    REQUIRE(v.GetClosestRaw(7) == 6000.0f);
    REQUIRE(v.GetClosestRaw(8) == 6000.0f);
    REQUIRE(v.GetClosestRaw(9) == 9000.0f);
    REQUIRE(v.GetClosestRaw(10) == 9000.0f);

    REQUIRE(v.SampleValid(NetworkTime{4, 0.5f}) == 5000.0f);
    REQUIRE(v.SampleValid(NetworkTime{5, 0.0f}) == 5000.0f);
    REQUIRE(v.SampleValid(NetworkTime{5, 0.5f}) == 5500.0f);
    REQUIRE(v.SampleValid(NetworkTime{6, 0.0f}) == 6000.0f);
    REQUIRE(v.SampleValid(NetworkTime{6, 0.5f}) == 6500.0f);
}

TEST_CASE("NetworkValue is repaired on demand")
{
    const unsigned maxExtrapolation = 10;
    const float smoothing = 5.0f;

    NetworkValue<DynamicFloat> v;
    v.Resize(10);
    NetworkValueSampler<DynamicFloat> s;
    s.Setup(maxExtrapolation, smoothing);

    // Interpolation is smooth when past frames are added
    v.Set(5, {5000.0f, 1000.0f});
    v.Set(7, {7000.0f, 1000.0f});

    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(4.0f), 0.5f) == 5000.0f);
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(4.5f), 0.5f) == 5000.0f);
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(5.0f), 0.5f) == 5000.0f);
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(5.5f), 0.5f) == 5500.0f);

    v.Set(6, {6000.0f, 1000.0f});

    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(5.5f), 0.0f) == 5500.0f);
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(6.0f), 0.5f) == 6000.0f);
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(6.5f), 0.5f) == 6500.0f);

    // Extrapolation is smooth when past frames are added
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(7.0f), 0.5f) == 7000.0f);
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(7.5f), 0.5f) == 7500.0f);
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(8.0f), 0.5f) == 8000.0f);
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(8.5f), 0.5f) == 8500.0f);

    v.Set(8, {8000.0f, 1000.0f});

    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(8.5f), 0.0f) == 8500.0f);
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(9.0f), 0.5f) == 9000.0f);

    // Extrapolation is smooth when unexpected past frames are added
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(11.0f), 2.0f) == 11000.0f);

    v.Set(10, {10000.0f, 2000.0f});

    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(11.0f), 0.0f) == 11000.0f);
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(11.5f), 0.5f).value_or(0.0f) == Catch::Approx(13000.0f).margin(200.0f));
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(12.0f), 0.5f).value_or(0.0f) == Catch::Approx(14000.0f).margin(40.0f));
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(12.5f), 0.5f).value_or(0.0f) == Catch::Approx(15000.0f).margin(6.0f));
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(13.0f), 0.5f).value_or(0.0f) == Catch::Approx(16000.0f).margin(1.0f));

    // Transition from extrapolation to interpolation is smooth
    v.Set(15, {15000.0f, 1000.0f});

    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(13.0f), 0.0f).value_or(0.0f) == Catch::Approx(16000.0f).margin(1.0f));
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(13.5f), 0.5f).value_or(0.0f) == Catch::Approx(13500.0f).margin(600.0f));
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(14.0f), 0.5f).value_or(0.0f) == Catch::Approx(14000.0f).margin(100.0f));
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(14.5f), 0.5f).value_or(0.0f) == Catch::Approx(14500.0f).margin(20.0f));
    REQUIRE(s.UpdateAndSample(v, NetworkTime::FromDouble(15.0f), 0.5f).value_or(0.0f) == Catch::Approx(15000.0f).margin(3.0f));
}

TEST_CASE("NetworkValueVector is updated and sampled")
{
    const unsigned size = 2;

    NetworkValueVector<float> v;
    v.Resize(size, 5);

    REQUIRE_FALSE(v.GetRaw(1));
    REQUIRE_FALSE(v.GetRaw(2));
    REQUIRE_FALSE(v.GetRaw(3));
    REQUIRE_FALSE(v.GetRaw(4));
    REQUIRE_FALSE(v.GetRaw(5));

    Set(v, 2, {1000.0f, 10000.0f});

    REQUIRE_FALSE(v.GetRaw(1));
    REQUIRE(IsSame(v.GetRaw(2), {1000.0f, 10000.0f}));
    REQUIRE_FALSE(v.GetRaw(3));
    REQUIRE_FALSE(v.GetRaw(4));
    REQUIRE_FALSE(v.GetRaw(5));

    REQUIRE(IsSame(v.GetClosestRaw(1), {1000.0f, 10000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(2), {1000.0f, 10000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(5), {1000.0f, 10000.0f}));

    REQUIRE(IsSame(v.SampleValid(NetworkTime{1, 0.5f}), {1000.0f, 10000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{2, 0.0f}), {1000.0f, 10000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{2, 0.5f}), {1000.0f, 10000.0f}));

    Set(v, 2, {2000.0f, 20000.0f});

    REQUIRE_FALSE(v.GetRaw(1));
    REQUIRE(IsSame(v.GetRaw(2), {2000.0f, 20000.0f}));
    REQUIRE_FALSE(v.GetRaw(3));
    REQUIRE_FALSE(v.GetRaw(4));
    REQUIRE_FALSE(v.GetRaw(5));

    REQUIRE(IsSame(v.GetClosestRaw(1), {2000.0f, 20000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(2), {2000.0f, 20000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(5), {2000.0f, 20000.0f}));

    REQUIRE(IsSame(v.SampleValid(NetworkTime{1, 0.5f}), {2000.0f, 20000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{2, 0.0f}), {2000.0f, 20000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{2, 0.5f}), {2000.0f, 20000.0f}));

    Set(v, 4, {4000.0f, 40000.0f});

    REQUIRE_FALSE(v.GetRaw(1));
    REQUIRE(IsSame(v.GetRaw(2), {2000.0f, 20000.0f}));
    REQUIRE_FALSE(v.GetRaw(3));
    REQUIRE(IsSame(v.GetRaw(4), {4000.0f, 40000.0f}));
    REQUIRE_FALSE(v.GetRaw(5));

    REQUIRE(IsSame(v.GetClosestRaw(1), {2000.0f, 20000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(2), {2000.0f, 20000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(3), {2000.0f, 20000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(4), {4000.0f, 40000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(5), {4000.0f, 40000.0f}));

    REQUIRE(IsSame(v.SampleValid(NetworkTime{1, 0.5f}), {2000.0f, 20000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{2, 0.0f}), {2000.0f, 20000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{2, 0.5f}), {2500.0f, 25000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{3, 0.0f}), {3000.0f, 30000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{3, 0.5f}), {3500.0f, 35000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{4, 0.0f}), {4000.0f, 40000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{4, 0.5f}), {4000.0f, 40000.0f}));

    Set(v, 3, {3000.0f, 30000.0f});
    Set(v, 5, {5000.0f, 50000.0f});
    Set(v, 6, {6000.0f, 60000.0f});

    REQUIRE_FALSE(v.GetRaw(1));
    REQUIRE(IsSame(v.GetRaw(2), {2000.0f, 20000.0f}));
    REQUIRE(IsSame(v.GetRaw(3), {3000.0f, 30000.0f}));
    REQUIRE(IsSame(v.GetRaw(4), {4000.0f, 40000.0f}));
    REQUIRE(IsSame(v.GetRaw(5), {5000.0f, 50000.0f}));
    REQUIRE(IsSame(v.GetRaw(6), {6000.0f, 60000.0f}));

    REQUIRE(IsSame(v.GetClosestRaw(5), {5000.0f, 50000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(6), {6000.0f, 60000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(7), {6000.0f, 60000.0f}));

    REQUIRE(IsSame(v.SampleValid(NetworkTime{1, 0.5f}), {2000.0f, 20000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{2, 0.0f}), {2000.0f, 20000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{2, 0.5f}), {2500.0f, 25000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{3, 0.0f}), {3000.0f, 30000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{3, 0.5f}), {3500.0f, 35000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{4, 0.0f}), {4000.0f, 40000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{4, 0.5f}), {4500.0f, 45000.0f}));

    REQUIRE(IsSame(v.SampleValid(NetworkTime{5, 0.75f}), {5750.0f, 57500.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{6, 0.0f}), {6000.0f, 60000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{6, 0.5f}), {6000.0f, 60000.0f}));

    Set(v, 9, {9000.0f, 90000.0f});

    REQUIRE_FALSE(v.GetRaw(1));
    REQUIRE_FALSE(v.GetRaw(2));
    REQUIRE_FALSE(v.GetRaw(3));
    REQUIRE_FALSE(v.GetRaw(4));
    REQUIRE(IsSame(v.GetRaw(5), {5000.0f, 50000.0f}));
    REQUIRE(IsSame(v.GetRaw(6), {6000.0f, 60000.0f}));
    REQUIRE_FALSE(v.GetRaw(7));
    REQUIRE_FALSE(v.GetRaw(8));
    REQUIRE(IsSame(v.GetRaw(9), {9000.0f, 90000.0f}));

    REQUIRE(IsSame(v.GetClosestRaw(4), {5000.0f, 50000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(5), {5000.0f, 50000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(6), {6000.0f, 60000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(7), {6000.0f, 60000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(8), {6000.0f, 60000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(9), {9000.0f, 90000.0f}));
    REQUIRE(IsSame(v.GetClosestRaw(10), {9000.0f, 90000.0f}));

    REQUIRE(IsSame(v.SampleValid(NetworkTime{4, 0.5f}), {5000.0f, 50000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{5, 0.0f}), {5000.0f, 50000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{5, 0.5f}), {5500.0f, 55000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{6, 0.0f}), {6000.0f, 60000.0f}));
    REQUIRE(IsSame(v.SampleValid(NetworkTime{6, 0.5f}), {6500.0f, 65000.0f}));
}
