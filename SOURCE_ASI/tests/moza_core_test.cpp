#include "../examples/mw05_mirror_probe/moza_core.h"
#include <cassert>
#include <cstdio>
#include <limits>

namespace m = mw05_moza;
static MW05MozaSample sample(std::uint32_t tick = 1000) {
    MW05MozaSample s{};
    s.abi = MW05_MOZA_ABI; s.bytes = sizeof(s); s.readStartedTick = tick;
    s.throttle = s.brake = -32768;
    return s;
}
static bool near(float a, float b) { return std::fabs(a-b) < 0.00001f; }

int main() {
    unsigned tests = 0;
    assert(m::pedalRaw(-32768) == 0 && m::pedalRaw(32767) == 1);
    assert(m::pedal(-32768) == 0 && m::pedal(32767) == 1);
    assert(near(m::pedal(0), (m::pedalRaw(0)-0.01f)/0.99f)); ++tests;
    float last = 0;
    for (int raw = -32768; raw <= 32767; ++raw) {
        const float v = m::pedal(static_cast<std::int16_t>(raw));
        assert(v >= 0 && v <= 1 && v >= last);
        if (raw > -32000 && raw < 32000) assert(v > 0 && v < 1);
        last = v;
    } ++tests;
    assert(m::pedal(-32500) == 0 && m::pedal(-32000) > 0); ++tests;

    auto s = sample();
    m::Config cfg{};
    s.angleDegrees = 225;
    assert(m::axes(s,cfg).steer == 0.5f);
    s.angleDegrees = -450;
    assert(m::axes(s,cfg).steer == -1);
    s.angleDegrees = 900;
    assert(m::axes(s,cfg).steer == 1);
    cfg.invertSteering = true;
    assert(m::axes(s,cfg).steer == -1);
    cfg.wheelRangeDegrees = 360; cfg.invertSteering = false; s.angleDegrees = 90;
    assert(m::axes(s,cfg).steer == 0.5f); ++tests;

    s = sample();
    assert(m::validSample(s));
    s.abi = 0; assert(!m::validSample(s)); s = sample();
    s.bytes = 24; assert(!m::validSample(s)); s = sample();
    s.sdkError = 2; assert(!m::validSample(s)); s = sample();
    s.angleDegrees = std::numeric_limits<float>::quiet_NaN();
    assert(!m::validSample(s));
    s.angleDegrees = std::numeric_limits<float>::infinity(); assert(!m::validSample(s));
    s.angleDegrees = 10001; assert(!m::validSample(s)); ++tests;
    assert(m::fresh(1000,1000) && m::fresh(1149,1000) && !m::fresh(1150,1000));
    assert(m::fresh(0x20,0xFFFFFFF0u) && !m::fresh(0x90,0xFFFFFFF0u));
    assert(!m::fresh(1000,1001)); ++tests;
    assert(m::liveWindow(60099,100) && !m::liveWindow(60100,100));
    assert(m::liveWindow(0x20,0xFFFFFFF0u)); ++tests;

    cfg = m::Config{}; s = sample();
    assert(m::neutral(s,cfg));
    s.throttle = -32000; assert(!m::neutral(s,cfg)); // Just outside the 1% zero zone.
    s.throttle = -32500; assert(m::neutral(s,cfg));
    s.throttle = -30000; assert(!m::neutral(s,cfg));
    s = sample(); s.brake = -30000; assert(!m::neutral(s,cfg));
    s = sample(); s.angleDegrees = 23; assert(!m::neutral(s,cfg));
    cfg.enableSteering = false; assert(m::neutral(s,cfg)); ++tests;
    cfg = m::Config{};
    m::NeutralTracker n;
    for (std::uint32_t t = 1000; t < 1500; t += 10) {
        n.observe(sample(t),true,cfg,t); assert(!n.ready(t));
    }
    n.observe(sample(1500),true,cfg,1500); assert(n.ready(1500));
    assert(!n.ready(1600)); // A stalled reader is not neutral-ready forever.
    n.observe(sample(1600),true,cfg,1600); assert(!n.ready(1600)); ++tests;
    n = m::NeutralTracker{};
    for (std::uint32_t t = 1000; t <= 1500; t += 10) n.observe(sample(t),true,cfg,t);
    assert(n.ready(1500));
    n.observe(sample(1510),false,cfg,1510); assert(!n.ready(1510));
    n.observe(sample(1520),true,cfg,1520); assert(!n.ready(1520)); ++tests;
    n = m::NeutralTracker{};
    n.observe(sample(1000),true,cfg,1000);
    n.observe(sample(1000),true,cfg,1150); assert(!n.tracking);
    s = sample(1160); s.throttle = 0;
    n.observe(s,true,cfg,1160); assert(!n.tracking); ++tests;
    n = m::NeutralTracker{};
    const std::uint32_t begin = 0xFFFFFF00u;
    for (unsigned dt = 0; dt <= 500; dt += 10) {
        const std::uint32_t now = begin+dt;
        n.observe(sample(now),true,cfg,now);
    }
    assert(n.ready(begin+500)); ++tests;

    m::OwnedAxis own;
    assert(!own.owns(0));
    own.attempt(0.5f); assert(own.owns(0.5f) && !own.owns(0.7f));
    own.success(0.5f); own.attempt(0.25f);
    assert(own.owns(0.5f) && own.owns(0.25f) && !own.owns(0.4f));
    own.success(0.25f); assert(!own.owns(0.5f));
    own.clear(); assert(!own.owns(0.25f)); ++tests;
    assert(m::pedalMode({0,0,0.5f}) == m::PedalMode::NativeNeutral);
    assert(m::pedalMode({0.000001f,0,0}) == m::PedalMode::Moza);
    assert(m::pedalMode({0,0.2f,0}) == m::PedalMode::Moza);
    assert(m::pedalMode(m::axes(sample(),m::Config{})) == m::PedalMode::NativeNeutral);
    ++tests;
    std::printf("PASS: %u MOZA core groups; all 65,536 pedal raw values checked\n", tests);
}
