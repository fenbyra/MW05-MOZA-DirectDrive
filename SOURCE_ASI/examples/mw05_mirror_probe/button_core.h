#ifndef MW05_BUTTON_CORE_H
#define MW05_BUTTON_CORE_H
#include <cstdint>
namespace mw05_buttons {
// A release must first be OBSERVED after startup/reconnect/focus loss.
// No queued key emulation or repeat while held; failed/stale reads reset.
struct PressGate {
    bool released = false;
    bool wasDown = false;
    bool haveTime = false;
    std::uint32_t lastAt = 0;
    void reset() { released=false; wasDown=false; haveTime=false; lastAt=0; }
    bool observe(bool valid, bool down, std::uint32_t now) {
        if (!valid) { reset(); return false; }
        if (haveTime && static_cast<std::uint32_t>(now-lastAt) > 150u) reset();
        haveTime=true; lastAt=now;
        if (!down) { released=true; wasDown=false; return false; }
        const bool edge = released && !wasDown;
        wasDown=true;
        return edge;
    }
};
}
#endif
