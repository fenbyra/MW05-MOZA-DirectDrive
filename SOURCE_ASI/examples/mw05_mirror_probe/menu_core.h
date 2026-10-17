#ifndef MW05_MENU_CORE_H
#define MW05_MENU_CORE_H
#include "session_gate.h"
#include <cstdint>
#include <cstring>
namespace mw05_menu {
// cFEngJoyInput mask getter. Ret4, byte player argument; EAX mask, not ST0.
constexpr std::uint32_t kReaderRva=0x00114B90u;
constexpr std::uint32_t kReturnRva=0x00114BD7u;
constexpr std::uint32_t kThunkRva=0x00116860u;
constexpr unsigned char kReaderEntry[]={0x0F,0xB6,0x54,0x24,0x04,0x33,0xC0,0xB9,0x48,0x34,0x8F,0x00,0x8D,0x14,0x95,0x50,0x34,0x8F,0x00};
constexpr unsigned char kReaderReturn[]={0x5E,0xC2,0x04,0x00};
constexpr unsigned char kThunk[]={0x8B,0x0D,0xD0,0xCA,0x91,0x00,0xE9,0x25,0xE3,0xFF,0xFF};
template<class Read> bool fingerprint(std::uint32_t base,Read read) {
    unsigned char entry[sizeof(kReaderEntry)]{},ret[sizeof(kReaderReturn)]{},thunk[sizeof(kThunk)]{};
    if (!read(base+kReaderRva,entry,sizeof(entry)) ||
        !read(base+kReturnRva,ret,sizeof(ret)) ||
        !read(base+kThunkRva,thunk,sizeof(thunk)) ||
        std::memcmp(entry,kReaderEntry,sizeof(entry)) ||
        std::memcmp(ret,kReaderReturn,sizeof(ret)) ||
        std::memcmp(thunk,kThunk,sizeof(thunk))) return false;
    // Read-only validation of UP,DOWN,LEFT,RIGHT masks and action IDs.
    for (unsigned row=0;row<4;++row) {
        std::uint32_t action=0,mask=0;
        if (!read(base+0x004F3428u+row*28u,&action,4) ||
            !read(base+0x004F342Cu+row*28u,&mask,4) ||
            action!=25u+row || mask!=(1u<<row)) return false;
    }
    return true;
}
inline bool allowed(const mw05_session_gate::Snapshot& context) {
    return context.readable && (context.flow==3u ||
           // Exact observed UI states only; NOT a general paused/cutscene mask.
           // 0x10 was observed in the race-entry accept/decline dialog.
           // This permits FEPad direction bits only, never gameplay axes.
           (context.flow==6u && (context.flags==2u || context.flags==0x10u)));
}
struct Sample {bool valid=false;std::uint32_t at=0;unsigned char directions=0;};
struct ReleaseGate {
    bool released=false;
    unsigned char observe(bool eligible,unsigned char raw) {
        if (!eligible) {released=false;return 0;}
        if (raw==0) {released=true;return 0;}
        if (!released || (raw & (raw-1u))!=0 || raw>8) return 0;
        return raw;
    }
};
inline std::uint32_t directionMask(unsigned char raw) {
    // DI bits UP,RIGHT,DOWN,LEFT -> FEPad masks UP,DOWN,LEFT,RIGHT.
    switch(raw) {case 1:return 1;case 2:return 8;case 4:return 2;case 8:return 4;default:return 0;}
}
inline std::uint32_t merge(std::uint32_t native,unsigned char player,const Sample& sample,
                           std::uint32_t now,const mw05_session_gate::Snapshot& context) {
    if (player!=0 || !sample.valid || static_cast<std::uint32_t>(now-sample.at)>150u ||
        !allowed(context)) return native;
    return native | directionMask(sample.directions); // Never remove native bits.
}
}
#endif
