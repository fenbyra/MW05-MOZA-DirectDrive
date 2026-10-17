#ifndef MW05_MOZA_CORE_H
#define MW05_MOZA_CORE_H
#include "moza_bridge_api.h"
#include <cmath>
#include <cstdint>

namespace mw05_moza {
constexpr std::uint32_t kDurationMs = 60000;
constexpr std::uint32_t kMaxAgeMs = 150;
constexpr std::uint32_t kMaxReadMs = 100;
constexpr std::uint32_t kNeutralDwellMs = 500;
constexpr std::uint32_t kNeutralMaxGapMs = 100;
constexpr float kPedalDeadzone = 0.01f;
constexpr float kNeutralPedalMax = kPedalDeadzone; // Arming must command zero pedals.
constexpr float kNeutralSteerMax = 0.05f;

struct Config {
    int wheelRangeDegrees = 900;
    bool invertSteering = false;
    bool enableSteering = true;
};
struct Axes { float gas = 0, brake = 0, steer = 0; };
enum class PedalMode : std::uint8_t { Off = 0, NativeNeutral = 1, Moza = 2 };
inline PedalMode pedalMode(const Axes& a) {
    // Applied to already normalized/deadzoned SDK axes. Never force a brake
    // value just because pedals are released; let the game's own logic run.
    return a.gas > 0.0f || a.brake > 0.0f ? PedalMode::Moza : PedalMode::NativeNeutral;
}

inline float clamp(float v, float low, float high) {
    return v < low ? low : (v > high ? high : v);
}
inline float pedalRaw(std::int16_t v) {
    return (static_cast<int>(v) + 32768) / 65535.0f;
}
inline float pedal(std::int16_t v) {
    // A rescaled 1% deadzone, NOT an on/off threshold.
    return clamp((pedalRaw(v) - kPedalDeadzone) / (1 - kPedalDeadzone), 0, 1);
}
inline bool validSample(const MW05MozaSample& s) {
    return s.abi == MW05_MOZA_ABI && s.bytes == sizeof(s) &&
           s.sdkError == 0 && std::isfinite(s.angleDegrees) &&
           std::fabs(s.angleDegrees) <= 10000.0f;
}
inline bool fresh(std::uint32_t now, std::uint32_t tick) {
    return static_cast<std::uint32_t>(now - tick) < kMaxAgeMs;
}
inline bool liveWindow(std::uint32_t now, std::uint32_t started) {
    return static_cast<std::uint32_t>(now - started) < kDurationMs;
}
inline Axes axes(const MW05MozaSample& s, const Config& cfg) {
    const float halfRange = static_cast<float>(cfg.wheelRangeDegrees) * 0.5f;
    const float sign = cfg.invertSteering ? -1.0f : 1.0f;
    return {pedal(s.throttle), pedal(s.brake),
            clamp(s.angleDegrees / halfRange, -1, 1) * sign};
}
inline bool neutral(const MW05MozaSample& s, const Config& cfg) {
    return validSample(s) &&
           pedalRaw(s.throttle) <= kNeutralPedalMax &&
           pedalRaw(s.brake) <= kNeutralPedalMax &&
           (!cfg.enableSteering || std::fabs(axes(s, cfg).steer) <= kNeutralSteerMax);
}

struct NeutralTracker {
    bool tracking = false;
    std::uint32_t since = 0, last = 0;
    void observe(const MW05MozaSample& s, bool good, const Config& cfg,
                 std::uint32_t now) {
        if (!good || !fresh(now, s.readStartedTick) || !neutral(s, cfg)) {
            tracking = false;
            return;
        }
        if (!tracking ||
            static_cast<std::uint32_t>(now - last) >= kNeutralMaxGapMs) {
            since = now;
            tracking = true;
        }
        last = now;
    }
    bool ready(std::uint32_t now) const {
        return tracking &&
               static_cast<std::uint32_t>(now - last) < kNeutralMaxGapMs &&
               static_cast<std::uint32_t>(now - since) >= kNeutralDwellMs;
    }
};

// Keep BOTH the last successful and latest attempted values. A failed write
// can leave either behind. Never restore a stale pre-test sample.
struct OwnedAxis {
    bool attempted = false, applied = false;
    float attemptedValue = 0, appliedValue = 0;
    void attempt(float value) { attempted = true; attemptedValue = value; }
    void success(float value) { applied = true; appliedValue = value; }
    bool owns(float current) const {
        return (attempted && current == attemptedValue) ||
               (applied && current == appliedValue);
    }
    void clear() { *this = OwnedAxis{}; }
};
} // namespace mw05_moza
#endif
