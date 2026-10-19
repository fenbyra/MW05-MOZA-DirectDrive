// MENU TEST; user request persists through transient context loss; default OFF.
// Uses the verified player/GetControls path from v0.5.3.
// v0.7.16 DRAG TELEMETRY TEST 1: preserves the user-tested v0.6.3 analog/native reverse path.
// No direct Shift/gear writes, FFB effects, key emulation or native axis bindings.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include "udp_transport.h"
#include <windows.h>
// Some MinGW headers hide these documented flags for a Win7 header target.
// Runtime use still requires Windows 8+ or the corresponding Win7 loader update.
#ifndef LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
#define LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR 0x00000100
#endif
#ifndef LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x00001000
#endif
#include <nfsmw_sdk/nfsmw_sdk.h>
#include <nfsmw_sdk/input.h>
#include <nfsmw_sdk/hooks.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include "probe_core.h"
#include "moza_core.h"
#include "drive_probe.h"
#include "hud_telemetry_core.h"
#include "udp_telemetry_core.h"
#include "direction_core.h"
#include "session_gate.h"
#include "button_core.h"
#include "menu_core.h"
#if !defined(MW05_PROBE_HOST_QA)
#include "di_button_reader.h"
#endif

#if !defined(NFSMW_HOOKS_BACKEND_MINHOOK) && !defined(MW05_PROBE_HOST_QA)
#error v0.7.6 requires the existing SDK MinHook backend; no raw 5-byte detour fallback.
#endif

#if defined(MW05_PROBE_HOST_QA)
#define MW05_THISCALL
#define MW05_FASTCALL
#elif defined(__GNUC__) && defined(__i386__)
#define MW05_THISCALL __attribute__((thiscall))
#define MW05_FASTCALL __attribute__((fastcall))
#elif defined(_MSC_VER) && defined(_M_IX86)
#define MW05_THISCALL __thiscall
#define MW05_FASTCALL __fastcall
#else
#error This ASI must be built for Win32/x86.
#endif
#if !defined(MW05_PROBE_HOST_QA)
static_assert(sizeof(void*) == 4, "Use the existing MINGW32 build");
#endif

NFSMW_PLUGIN_DECLARE("MW05 MOZA DirectDrive", "0.7.15-drag-input-test2", "local")

namespace {
namespace p = mw05_probe;
namespace m = mw05_moza;
namespace drive = mw05_drive_probe;
namespace hud = mw05_hud;
namespace direction = mw05_direction;
namespace session = mw05_session_gate;
namespace menu = mw05_menu;
using MenuReader = std::uint32_t (MW05_THISCALL *)(void*,unsigned char);
MenuReader g_menuOriginal = nullptr;
bool g_menuEnabled = false;
SRWLOCK g_menuLock = SRWLOCK_INIT;
menu::Sample g_menuSample{};
DWORD g_menuCalls=0, g_menuSupplied=0;
int g_menuLastAction=-1;
DWORD g_menuLastNative=0, g_menuLastMerged=0;
menu::ReleaseGate g_menuRelease{}; // Dedicated button thread only.
struct MenuGuard {
    MenuGuard() { AcquireSRWLockExclusive(&g_menuLock); }
    ~MenuGuard() { ReleaseSRWLockExclusive(&g_menuLock); }
};
// One opaque stack DWORD is forwarded unchanged. Native HUD tick returns with ret 4.
using HudTick = void (MW05_THISCALL *)(void*,std::uint32_t);
HudTick g_hudOriginal=nullptr;
bool g_hudInstalled=false;
SRWLOCK g_hudLock=SRWLOCK_INIT;
hud::Cache g_hudCache{};
DWORD g_hudCalls=0,g_hudCaptures=0,g_hudAttemptAt=0;
bool g_hudAttempted=false;
p::Address g_hudAttemptWidget=0;
HudTick g_dragHudOriginal=nullptr;
bool g_dragHudInstalled=false;
hud::Cache g_dragHudCache{};
DWORD g_dragHudCalls=0,g_dragHudCaptures=0,g_dragHudAttemptAt=0;
bool g_dragHudAttempted=false;
p::Address g_dragHudAttemptWidget=0;
struct HudGuard {
    HudGuard(){AcquireSRWLockExclusive(&g_hudLock);}
    ~HudGuard(){ReleaseSRWLockExclusive(&g_hudLock);}
};
using Getter = void* (MW05_THISCALL *)(void*);
using DirectionHelper = void (MW05_THISCALL *)(void*, float, float);
SRWLOCK g_lock = SRWLOCK_INIT;
struct Guard {
    Guard() { AcquireSRWLockExclusive(&g_lock); }
    ~Guard() { ReleaseSRWLockExclusive(&g_lock); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
};
enum class StopReason {
    None, User, Escape, FocusLost, PlayerChanged, Layout, WriteFailed,
    MozaInvalid, MozaStale, DriveInvalid, DirectionUnavailable, NotDriving, ContextInvalid
};
struct State {
    p::Player player{};
    bool resolverDiagSeen = false;
    DWORD resolverDiagAt = 0;
    bool driveDiagSeen = false;
    DWORD driveDiagAt = 0;
    bool hookAttempted = false, hookInstalled = false, armed = false;
    bool previousKeyDown = false, playerReportPending = false;
    bool cleanupPending = false;
    bool requested = false, pendingSuspension = false;
    bool startQueued = false; // Explicit neutral-approved menu intent; no menu axis writes.
    bool resumeImmediate = false; // Previously armed session; new player must pass all fresh rebind checks.
    m::NeutralTracker resumeNeutral{};
    p::Address lastWrittenInput = 0;
    m::OwnedAxis gasOwned{}, brakeOwned{}, steerOwned{};
    m::PedalMode pedalMode = m::PedalMode::Off;
    DWORD pedalWrites = 0, pedalHandoffs = 0, reportPedalWrites = 0;
    float beforeGameGas = 0, beforeGameBrake = 0;
    int sdkStatus = MW05_MOZA_NOT_LOADED;
    MW05MozaSample sdkSample{};
    m::NeutralTracker neutral{};
    DWORD sampleSequence = 0, testStarted = 0, reportAt = 0;
    DWORD totalCalls = 0, playerCalls = 0, writes = 0, writeErrors = 0;
    DWORD writesAtStart = 0, reportCalls = 0, reportWrites = 0;
    StopReason pendingStop = StopReason::None;
    bool directionInstalled = false, directionSeen = false;
    bool directionInFlight = false, directionLeasePoisoned = false;
    p::Address directionInput = 0;
    DWORD directionSeenAt = 0;
    DWORD directionCalls = 0, directionMozaCalls = 0, reverseMaps = 0;
    DWORD reportDirectionCalls = 0, reportDirectionMozaCalls = 0, reportReverseMaps = 0;
} g;

m::Config g_config{}; // Immutable after initialization.
int g_toggleButton = 37; // joy.cpl/game number, one-based; 0 disables.
volatile LONG g_buttonEventTick = 0; // single-slot, fresh press only; watchdog/poll consume once.
bool g_udpEnabled=false; // Immutable after startup; independent of S2/F8 axes switch.
unsigned g_udpPort=20066; // Immutable loopback-only destination.
DWORD g_reportIntervalMs = 5000u; // [DirectDrive] Diagnostics=1 selects 200ms logging only.
p::Address g_base = 0;
Getter g_original = nullptr; // Initialized before patching.
DirectionHelper g_directionOriginal = nullptr; // Published BEFORE enabling detour.
FILE* g_log = nullptr;
DWORD g_started = 0;
volatile LONG g_ready = 0;
volatile LONG g_liveGate = 0; // Final cancellation check before an original helper call.
wchar_t g_gameRoot[MAX_PATH]{};
constexpr p::Address kSteerOffset = 0x04;

void* ptr(p::Address a) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(a)); }
p::Address address(const void* a) {
    return static_cast<p::Address>(reinterpret_cast<std::uintptr_t>(a));
}
bool readMemory(p::Address a, void* out, std::size_t count) {
    SIZE_T done = 0;
    return a != 0 && ReadProcessMemory(GetCurrentProcess(), ptr(a), out, count, &done) &&
           done == count;
}
template<class T> bool readValue(p::Address a, T& out) {
    return readMemory(a, &out, sizeof(out));
}
bool writeFloat(p::Address a, float value) {
    SIZE_T done = 0;
    return WriteProcessMemory(GetCurrentProcess(), ptr(a), &value, sizeof(value), &done) &&
           done == sizeof(value);
}
bool writePedals(p::Address controls, float gas, float brake) {
    const float values[2] = {gas, brake};
    SIZE_T done = 0;
    return WriteProcessMemory(GetCurrentProcess(), ptr(controls + p::kGasOffset),
                              values, sizeof(values), &done) && done == sizeof(values);
}
void logLine(const char* fmt, ...) {
    char body[1500]{};
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);
    char line[1600]{};
    std::snprintf(line, sizeof(line), "[%8lu ms] %s\n",
                  static_cast<unsigned long>(GetTickCount() - g_started), body);
    if (g_log) { std::fputs(line, g_log); std::fflush(g_log); }
    OutputDebugStringA(line);
}
bool gameFocused() {
    const HWND window = GetForegroundWindow();
    DWORD pid = 0;
    return window && GetWindowThreadProcessId(window, &pid) != 0 &&
           pid == GetCurrentProcessId();
}
void publishMenu(bool valid, unsigned char rawDirections, DWORD now) {
    const bool eligible=g_menuEnabled && valid && gameFocused() &&
        menu::allowed(session::sample(g_base,&readMemory));
    const unsigned char directions=g_menuRelease.observe(eligible,rawDirections);
    MenuGuard guard;
    g_menuSample={eligible,static_cast<std::uint32_t>(now),directions};
}
std::uint32_t MW05_FASTCALL menuReaderHook(void* self, void* /*unusedEdx*/, unsigned char player) {
    // Original exactly once, no plugin lock held. This returns UI bits, not axes.
    const std::uint32_t native=g_menuOriginal ? g_menuOriginal(self,player):0u;
    if (!g_menuEnabled || InterlockedCompareExchange(&g_ready,0,0)==0) return native;
    menu::Sample sample{};
    { MenuGuard guard; sample=g_menuSample; ++g_menuCalls; g_menuLastAction=player;
      g_menuLastNative=native; g_menuLastMerged=native; }
    if (!gameFocused()) return native;
    const auto context=session::sample(g_base,&readMemory);
    const auto merged=menu::merge(native,player,sample,static_cast<std::uint32_t>(GetTickCount()),context);
    { MenuGuard guard; g_menuLastMerged=merged; if (merged!=native) ++g_menuSupplied; }
    return merged;
}
bool installMenuHook() {
    if (!g_menuEnabled) return false;
    if (!menu::fingerprint(g_base,&readMemory) || !nfsmw__mh_ensure_init()) {
        logLine("MENU_HOOK refused: fingerprint/init mismatch; axes and S2 unchanged");
        return false;
    }
    void* const target=ptr(g_base+menu::kReaderRva);
    void* trampoline=nullptr;
    const MH_STATUS created=MH_CreateHook(target,reinterpret_cast<void*>(&menuReaderHook),&trampoline);
    if (created!=MH_OK || !trampoline) {
        if (created==MH_OK) MH_RemoveHook(target);
        logLine("MENU_HOOK refused: create status=%d",static_cast<int>(created));
        return false;
    }
    g_menuOriginal=reinterpret_cast<MenuReader>(trampoline);
    if (MH_EnableHook(target)!=MH_OK) {
        MH_RemoveHook(target); g_menuOriginal=nullptr;
        logLine("MENU_HOOK refused: enable failed; axes and S2 unchanged");
        return false;
    }
    logLine("MENU_HOOK installed=1 target=%08lX; UI mask getter; player=0; UP=1 DOWN=2 LEFT=4 RIGHT=8; no key emulation; release D-pad first",
            static_cast<unsigned long>(g_base+menu::kReaderRva));
    return true;
}
void MW05_FASTCALL hudTickHook(void* self,void* /*unusedEdx*/,std::uint32_t nativeArg) {
    // Native exactly once FIRST, unchanged this/argument and no plugin lock held.
    if (g_hudOriginal) g_hudOriginal(self,nativeArg);
    if (InterlockedCompareExchange(&g_ready,0,0)==0) return;
    const DWORD now=GetTickCount();
    { HudGuard guard;
      ++g_hudCalls;
      if (g_hudAttempted && g_hudAttemptWidget==address(self) && static_cast<DWORD>(now-g_hudAttemptAt)<100u) return;
      g_hudAttempted=true;g_hudAttemptAt=now;g_hudAttemptWidget=address(self); }
    const auto player=p::resolvePlayer(g_base,&readMemory);
    const auto value=gameFocused() ? hud::sample(g_base,player,address(self),&readMemory):
                                  hud::failure(hud::Status::ContextBlocked);
    { HudGuard guard;hud::publish(g_hudCache,value,now);++g_hudCaptures; }
}
bool installHudHook() {
    if (!hud::fingerprint(g_base,&readMemory) || !nfsmw__mh_ensure_init()) {
        logLine("HUD_HOOK refused: fingerprint/init mismatch; controls unchanged");return false;
    }
    void* const target=ptr(g_base+hud::kTickRva);
    void* trampoline=nullptr;
    const MH_STATUS made=MH_CreateHook(target,reinterpret_cast<void*>(&hudTickHook),&trampoline);
    if (made!=MH_OK || !trampoline) {
        if (made==MH_OK) MH_RemoveHook(target);
        logLine("HUD_HOOK refused: create status=%d; controls unchanged",static_cast<int>(made));return false;
    }
    g_hudOriginal=reinterpret_cast<HudTick>(trampoline); // BEFORE enable.
    if (MH_EnableHook(target)!=MH_OK) {
        MH_RemoveHook(target);g_hudOriginal=nullptr;
        logLine("HUD_HOOK refused: enable failed; controls unchanged");return false;
    }
    logLine("HUD_HOOK installed=1 target=%08lX; native tick called once; read after tick; no memory scan; optional separate UDP worker",
            static_cast<unsigned long>(g_base+hud::kTickRva));
    return true;
}
void MW05_FASTCALL dragHudTickHook(void* self,void* /*unusedEdx*/,std::uint32_t nativeArg) {
    // Native exactly once FIRST, unchanged this/argument and no plugin lock held.
    if (g_dragHudOriginal) g_dragHudOriginal(self,nativeArg);
    if (InterlockedCompareExchange(&g_ready,0,0)==0) return;
    const DWORD now=GetTickCount();
    { HudGuard guard;
      ++g_dragHudCalls;
      if (g_dragHudAttempted && g_dragHudAttemptWidget==address(self) && static_cast<DWORD>(now-g_dragHudAttemptAt)<100u) return;
      g_dragHudAttempted=true;g_dragHudAttemptAt=now;g_dragHudAttemptWidget=address(self); }
    const auto player=p::resolvePlayer(g_base,&readMemory);
    const auto value=gameFocused() ? hud::sample(g_base,player,address(self),&readMemory,hud::Source::Drag):
                                  hud::failure(hud::Status::ContextBlocked);
    { HudGuard guard;hud::publish(g_dragHudCache,value,now);++g_dragHudCaptures; }
}
bool installDragHudHook() {
    if (!hud::fingerprint(g_base,&readMemory,hud::Source::Drag) || !nfsmw__mh_ensure_init()) {
        logLine("DRAG_HUD_HOOK refused: fingerprint/init mismatch; controls unchanged");return false;
    }
    void* const target=ptr(g_base+hud::kDragTickRva);
    void* trampoline=nullptr;
    const MH_STATUS made=MH_CreateHook(target,reinterpret_cast<void*>(&dragHudTickHook),&trampoline);
    if (made!=MH_OK || !trampoline) {
        if (made==MH_OK) MH_RemoveHook(target);
        logLine("DRAG_HUD_HOOK refused: create status=%d; controls unchanged",static_cast<int>(made));return false;
    }
    g_dragHudOriginal=reinterpret_cast<HudTick>(trampoline); // BEFORE enable.
    if (MH_EnableHook(target)!=MH_OK) {
        MH_RemoveHook(target);g_dragHudOriginal=nullptr;
        logLine("DRAG_HUD_HOOK refused: enable failed; controls unchanged");return false;
    }
    logLine("DRAG_HUD_HOOK installed=1 target=%08lX; native tick called once; read after tick; no memory scan; optional separate UDP worker",
            static_cast<unsigned long>(g_base+hud::kDragTickRva));
    return true;
}
bool escapeDown() {
    return ((GetAsyncKeyState(VK_ESCAPE) | GetAsyncKeyState(VK_PAUSE)) & 0x8000) != 0;
}
bool sampleGood(DWORD now) {
    return g.sdkStatus == MW05_MOZA_OK && m::validSample(g.sdkSample) &&
           m::fresh(now, g.sdkSample.readStartedTick);
}
bool directionFresh(DWORD now) {
    return g.directionInstalled && g_directionOriginal && g.directionSeen &&
           g.directionInput == g.player.input &&
           m::fresh(now, g.directionSeenAt);
}
const char* reasonName(StopReason r) {
    switch (r) {
    case StopReason::User: return "user toggle (F8/S2)";
    case StopReason::Escape: return "Esc/Pause";
    case StopReason::FocusLost: return "game lost foreground focus";
    case StopReason::PlayerChanged: return "player input changed/disappeared";
    case StopReason::Layout: return "unexpected GetControls return";
    case StopReason::WriteFailed: return "memory read/write failed";
    case StopReason::MozaInvalid: return "MOZA sample invalid or SDK error";
    case StopReason::MozaStale: return "MOZA sample older than 150ms";
    case StopReason::NotDriving: return "simulation blocked/menu/not in race";
    case StopReason::ContextInvalid: return "session state unreadable";
    case StopReason::DriveInvalid: return "drivetrain/owner validation failed";
    case StopReason::DirectionUnavailable: return "native direction helper missing/stale";
    default: return "none";
    }
}
void forgetOwnership() {
    g.lastWrittenInput = 0;
    g.cleanupPending = false;
    g.gasOwned.clear(); g.brakeOwned.clear(); g.steerOwned.clear();
}
bool cleanupAxis(p::Address a, m::OwnedAxis& owned) {
    if (!owned.attempted) return true;
    float current = 0;
    if (!readValue(a, current)) return false;
    if (!owned.owns(current) || current == 0 || writeFloat(a, 0)) {
        owned.clear();
        return true;
    }
    return false;
}
void cleanupAxes() {
    if (!g.cleanupPending) return;
    // The original helper may still be finishing its own setter calls. A
    // cancellation is recorded immediately, but ownership cleanup waits for
    // this wrapper to return; never hold g_lock across a game function call.
    if (g.directionInFlight) return;
    // Cleanup can also be triggered inside the getter, before the next
    // onPoll/watchdog identity update. Re-resolve HERE instead of trusting
    // a cached player, then never write an old/disappeared/ambiguous object.
    const p::Player fresh = p::resolvePlayer(g_base, &readMemory);
    if (fresh.status != g.player.status || fresh.input != g.player.input ||
        fresh.inputVtable != g.player.inputVtable || fresh.vehicle != g.player.vehicle) g.playerReportPending = true;
    if(fresh.inputVtable!=g.player.inputVtable)forgetOwnership();
    g.player = fresh;
    if (g.lastWrittenInput == 0 || g.player.status != p::ResolveStatus::Unique ||
        g.player.input != g.lastWrittenInput) {
        forgetOwnership();
        return;
    }
    const bool gas = cleanupAxis(g.player.controls + p::kGasOffset, g.gasOwned);
    const bool brake = cleanupAxis(g.player.controls + p::kBrakeOffset, g.brakeOwned);
    const bool steer = cleanupAxis(g.player.controls + kSteerOffset, g.steerOwned);
    g.cleanupPending = !(gas && brake && steer);
    if (!g.cleanupPending) g.lastWrittenInput = 0;
}
bool transientStop(StopReason reason) {
    return reason==StopReason::Escape || reason==StopReason::FocusLost ||
           reason==StopReason::PlayerChanged || reason==StopReason::NotDriving ||
           reason==StopReason::DirectionUnavailable;
}
void stopTest(StopReason reason) {
    const bool suspended=g.requested && transientStop(reason);
    if (!suspended) g.startQueued=false;
    // Preserve an already authorized driving session through ordinary player
    // loss/replacement too. This is NOT authority to write to the old/new object:
    // tryResume still requires a unique freshly resolved identity, its real native
    // helper heartbeat, owner/drive fingerprints and every runtime gate.
    // Fatal faults and explicit OFF clear this permission below; initial manual
    // ON still requires neutral. Never synthesize permission from requested alone.
    if (!suspended) g.resumeImmediate=false;
    else if (g.armed) g.resumeImmediate=true;
    if (!suspended) {
        if (g.requested && !g.armed)
            logLine("OFF reason=%s; suspended request cleared",reasonName(reason));
        g.requested=false;
    }
    g.resumeNeutral.tracking=false;
    if (!g.armed) return;
    g.pendingSuspension=suspended;
    g.armed = false;
    InterlockedExchange(&g_liveGate, 0);
    g.pedalMode = m::PedalMode::Off;
    g.pendingStop = reason;
    g.cleanupPending = g.lastWrittenInput != 0;
    g.neutral.tracking = false; // Manual re-enable still requires fresh neutral dwell.
    cleanupAxes();
}
void flushStopLog() {
    if (g.pendingStop == StopReason::None) return;
    logLine("%s reason=%s test_writes=%lu cleanup_pending=%d",
            g.pendingSuspension ? "SUSPEND" : "STOP", reasonName(g.pendingStop), static_cast<unsigned long>(g.writes-g.writesAtStart),
            g.cleanupPending ? 1 : 0);
    g.pendingStop = StopReason::None;
}
void enforceSafety(DWORD now) {
    // Queued menu intent may survive ordinary loading/identity changes, not faults.
    if (g.startQueued) {
        if (!sampleGood(now)) {
            stopTest(g.sdkStatus!=MW05_MOZA_OK || !m::validSample(g.sdkSample) ?
                     StopReason::MozaInvalid : StopReason::MozaStale);
            return;
        }
        if (!session::sample(g_base,&readMemory).readable) {
            stopTest(StopReason::ContextInvalid); return;
        }
        if (!g.directionInstalled || !g_directionOriginal ||
            (g.hookAttempted && !g.hookInstalled)) {
            stopTest(StopReason::DirectionUnavailable);
            // This is an installation fault, not transient helper freshness.
            g.startQueued=false; g.requested=false; g.resumeImmediate=false;
            logLine("OFF queued start cancelled: required hook unavailable");
            return;
        }
    }
    if (!g.armed) return;
    if (!gameFocused()) { stopTest(StopReason::FocusLost); return; }
    if (escapeDown()) { stopTest(StopReason::Escape); return; }
    const session::Snapshot gate=session::sample(g_base,&readMemory);
    if (!gate.readable) stopTest(StopReason::ContextInvalid);
    else if (!gate.ready()) stopTest(StopReason::NotDriving);
    else if (g.sdkStatus != MW05_MOZA_OK || !m::validSample(g.sdkSample))
        stopTest(StopReason::MozaInvalid);
    else if (!m::fresh(now, g.sdkSample.readStartedTick)) stopTest(StopReason::MozaStale);
    else if (!directionFresh(now)) stopTest(StopReason::DirectionUnavailable);
}

struct DirectionLease {
    bool prepared = false;
    p::Address input = 0, vehicle = 0, inputVtable = 0, previousWrittenInput = 0;
    DWORD sampleTick = 0;
    m::Axes command{};
    m::OwnedAxis previousGas{}, previousBrake{};
};
void updatePlayer(bool report);

// ABI: game supplies ECX=InputPlayer base, plus gas/brake floats on stack.
// FASTCALL supplies an unused EDX argument and also returns with ret 8.
// No new native call is synthesized: exactly ONE original call per callback.
void MW05_FASTCALL directionHook(void* self, void* /*unusedEdx*/,
                                float nativeGas, float nativeBrake) {
    DirectionHelper original = g_directionOriginal;
    if (!original) return; // Not reachable after a successful hook installation.
    DirectionLease lease{};
    if (InterlockedCompareExchange(&g_ready, 0, 0) != 0) {
        Guard guard;
        if (g.player.status == p::ResolveStatus::Unique &&
            address(self) <= 0xFFFFFE00u &&
            address(self) + direction::kOwnerOffset == g.player.input) {
            ++g.directionCalls;
            // A late callback cannot revive a timed-out active lease.
            enforceSafety(GetTickCount());
            g.directionSeen = true;
            g.directionInput = g.player.input;
            g.directionSeenAt = GetTickCount();
            if (g.armed && !g.directionInFlight) {
                const drive::Snapshot snapshot = drive::sample(g_base, g.player, &readMemory);
                const bool ownerOk = direction::ownerMatches(
                    g_base, g.player, address(self), &readMemory);
                const bool settersOk = direction::settersMatch(g_base, &readMemory, g.player.inputVtable);
                enforceSafety(GetTickCount()); // AFTER validation I/O.
                if (g.armed && (snapshot.status != drive::Status::Ok ||
                                !ownerOk || !settersOk)) {
                    updatePlayer(false);
                    if (g.armed) stopTest(StopReason::DriveInvalid);
                }
                if (g.armed) {
                    const m::Axes command = m::axes(g.sdkSample, g_config);
                    if (m::pedalMode(command) == m::PedalMode::Moza) {
                        lease.prepared = true;
                        lease.input = g.player.input;
                        lease.vehicle = g.player.vehicle;
                        lease.inputVtable = g.player.inputVtable;
                        lease.sampleTick = g.sdkSample.readStartedTick;
                        lease.command = command;
                        lease.previousWrittenInput = g.lastWrittenInput;
                        lease.previousGas = g.gasOwned;
                        lease.previousBrake = g.brakeOwned;
                        g.directionInFlight = true;
                        g.directionLeasePoisoned = false;
                        g.lastWrittenInput = lease.input;
                        // Only POSSIBLE new helper writes: in R it calls
                        // SetGas(logical brake), SetBrake(logical gas).
                        // Keep the last successful getter values as well.
                        g.gasOwned.attempt(command.brake);
                        g.brakeOwned.attempt(command.gas);
                    }
                }
            }
        }
    }
    bool useMoza = lease.prepared &&
        InterlockedCompareExchange(&g_liveGate, 0, 0) != 0 &&
        m::fresh(GetTickCount(), lease.sampleTick) &&
        gameFocused() && !escapeDown() && session::sample(g_base,&readMemory).ready();
    if (lease.prepared && !useMoza) {
        Guard guard;
        g.directionInFlight = false;
        const p::Player fresh = p::resolvePlayer(g_base, &readMemory);
        if (!g.directionLeasePoisoned && fresh.status == p::ResolveStatus::Unique &&
            fresh.input == lease.input && fresh.vehicle == lease.vehicle &&
            fresh.inputVtable == lease.inputVtable &&
            g.player.input == lease.input && g.player.vehicle == lease.vehicle &&
            g.player.inputVtable == lease.inputVtable) {
            // No helper call with MOZA arguments was made: undo its staged
            // ownership before the untouched native call below.
            g.lastWrittenInput = lease.previousWrittenInput;
            g.gasOwned = lease.previousGas;
            g.brakeOwned = lease.previousBrake;
        } else {
            forgetOwnership();
        }
        enforceSafety(GetTickCount());
        if (!g.armed) {
            g.cleanupPending = g.lastWrittenInput != 0;
            cleanupAxes();
        }
    }
    // Deliberately OUTSIDE g_lock: Shift/other native code may re-enter our
    // getter, watchdog or input callbacks. No SDK call is made here.
    original(self, useMoza ? lease.command.gas : nativeGas,
                   useMoza ? lease.command.brake : nativeBrake);
    if (lease.prepared && useMoza) {
        Guard guard;
        ++g.directionMozaCalls;
        g.directionInFlight = false;
        if (g.directionLeasePoisoned) {
            forgetOwnership(); // Unexpected getter layout forbids cleanup too.
        } else {
            const p::Player fresh = p::resolvePlayer(g_base, &readMemory);
            if (fresh.status != p::ResolveStatus::Unique ||
                fresh.input != lease.input || fresh.vehicle != lease.vehicle ||
                fresh.inputVtable != lease.inputVtable) {
                if(fresh.inputVtable!=lease.inputVtable)forgetOwnership();
                g.player = fresh;
                g.playerReportPending = true;
                stopTest(StopReason::PlayerChanged);
                forgetOwnership();
            } else {
                enforceSafety(GetTickCount());
                if (!g.armed) {
                    g.cleanupPending = g.lastWrittenInput != 0;
                    cleanupAxes();
                }
            }
        }
    }
}

bool installDirectionHook() {
    if (!direction::fingerprint(g_base, &readMemory)) {
        logLine("DIRECTION HOOK REFUSED: code/setter fingerprint mismatch; F8 disabled");
        return false;
    }
    if (!nfsmw__mh_ensure_init()) {
        logLine("DIRECTION HOOK REFUSED: MinHook initialization failed; F8 disabled");
        return false;
    }
    void* const target = ptr(g_base + direction::kHelperRva);
    void* trampoline = nullptr;
    const MH_STATUS created = MH_CreateHook(target,
        reinterpret_cast<void*>(&directionHook), &trampoline);
    if (created != MH_OK || !trampoline) {
        if (created == MH_OK) MH_RemoveHook(target);
        logLine("DIRECTION HOOK REFUSED: MH_CreateHook status=%d; F8 disabled",
                static_cast<int>(created));
        g_directionOriginal = nullptr;
        return false;
    }
    g_directionOriginal = reinterpret_cast<DirectionHelper>(trampoline);
    const MH_STATUS enabled = MH_EnableHook(target);
    if (enabled != MH_OK) {
        MH_RemoveHook(target);
        g_directionOriginal = nullptr;
        logLine("DIRECTION HOOK REFUSED: MH_EnableHook status=%d; F8 disabled",
                static_cast<int>(enabled));
        return false;
    }
    logLine("DIRECTION HOOK installed=1 target=%08lX; native calls only; no direct Shift/gear writes",
            static_cast<unsigned long>(g_base + direction::kHelperRva));
    return true;
}

bool releasePedalsToNative(p::Address input) {
    // Called ONLY on a transition from owned pedal values to released pedals.
    // Re-resolve before cleanup: never release values through a cached object.
    const p::Player fresh = p::resolvePlayer(g_base, &readMemory);
    if (fresh.status != p::ResolveStatus::Unique ||
        fresh.status != g.player.status || fresh.input != input ||
        fresh.input != g.player.input || fresh.inputVtable != g.player.inputVtable || fresh.vehicle != g.player.vehicle ||
        g.lastWrittenInput != input) {
        g.player = fresh;
        g.playerReportPending = true;
        forgetOwnership();
        stopTest(StopReason::PlayerChanged);
        return false;
    }
    const bool gas = cleanupAxis(fresh.controls + p::kGasOffset, g.gasOwned);
    const bool brake = cleanupAxis(fresh.controls + p::kBrakeOffset, g.brakeOwned);
    if (!gas || !brake) return false;
    if (!g.steerOwned.attempted) g.lastWrittenInput = 0;
    ++g.pedalHandoffs;
    return true;
}

// Verified ABI: ECX=this, EAX=the original InputControls*. Never inline-patch
// the four-byte getter. Native keyboard/buttons remain intact while OFF.
void* MW05_FASTCALL getControlsHook(void* self, void* /*unusedEdx*/) {
    void* result = g_original(self);
    Guard guard;
    ++g.totalCalls;
    if (address(self) != g.player.input || g.player.status != p::ResolveStatus::Unique)
        return result;
    ++g.playerCalls;
    if (!p::validGetterResult(address(self), address(result))) {
        if (g.directionInFlight) g.directionLeasePoisoned = true;
        forgetOwnership(); // Layout is uncertain: even cleanup writes are forbidden.
        stopTest(StopReason::Layout);
        return result;
    }
    enforceSafety(GetTickCount());
    if (!g.armed || g.directionInFlight) return result;
    p::InputControls before{};
    if (!readValue(address(result), before)) {
        ++g.writeErrors;
        stopTest(StopReason::WriteFailed);
        return result;
    }
    // No SDK calls in the game's getter. Recheck age/deadline AFTER memory I/O.
    enforceSafety(GetTickCount());
    if (!g.armed) return result;
    const m::Axes command = m::axes(g.sdkSample, g_config);
    // Read the ACTUAL current gear. A reverse request or speed sign is not
    // a gear. Validation failure cancels rather than guessing forward mapping.
    const drive::Snapshot drivetrain = drive::sample(g_base, g.player, &readMemory);
    const direction::EnginePedals engine = direction::map(command, drivetrain.gear);
    enforceSafety(GetTickCount());
    if (!g.armed) return result;
    if (drivetrain.status != drive::Status::Ok || !engine.valid) {
        updatePlayer(false);
        if (g.armed) stopTest(StopReason::DriveInvalid);
        return result;
    }
    const p::Address controls = address(result);
    // A getter can read our own previous write. These are game-memory
    // diagnostics, NOT independent raw native/device samples.
    g.beforeGameGas = before.gas;
    g.beforeGameBrake = before.brake;
    const m::PedalMode nextMode = m::pedalMode(command);
    if (nextMode == m::PedalMode::NativeNeutral &&
        (g.gasOwned.attempted || g.brakeOwned.attempted)) {
        // Release recognized pedal values ONCE, before any new override write.
        // Then native input may restore its own current idle/coasting behavior.
        if (!releasePedalsToNative(address(self))) {
            if (g.armed) {
                ++g.writeErrors;
                stopTest(StopReason::WriteFailed);
            }
            return result;
        }
        enforceSafety(GetTickCount());
        if (!g.armed) return result;
    }
    bool wroteOverride = false;
    if (g_config.enableSteering) {
        g.lastWrittenInput = address(self);
        g.steerOwned.attempt(command.steer);
        if (!writeFloat(controls + kSteerOffset, command.steer)) {
            ++g.writeErrors;
            stopTest(StopReason::WriteFailed);
            return result;
        }
        g.steerOwned.success(command.steer);
        wroteOverride = true;
    }
    // Recheck again: a slow steering write must not be followed by stale pedals.
    enforceSafety(GetTickCount());
    if (!g.armed) return result;
    if (nextMode == m::PedalMode::Moza) {
        g.lastWrittenInput = address(self);
        g.gasOwned.attempt(engine.gas);
        g.brakeOwned.attempt(engine.brake);
        if (!writePedals(controls, engine.gas, engine.brake)) {
            ++g.writeErrors;
            stopTest(StopReason::WriteFailed);
            return result;
        }
        g.gasOwned.success(engine.gas);
        g.brakeOwned.success(engine.brake);
        if (engine.reverse) ++g.reverseMaps;
        ++g.pedalWrites;
        g.pedalMode = m::PedalMode::Moza;
        wroteOverride = true;
    } else {
        // No pedal write in neutral. In particular, neither force brake=0
        // every getter nor replay/synthesize brake=1.
        g.pedalMode = m::PedalMode::NativeNeutral;
    }
    if (wroteOverride) ++g.writes; // Successful override cycle, not individual fields.
    return result;
}

bool installGetterSlot(p::Address vtable) {
    const p::Address slot = vtable + p::kGetterSlot * sizeof(p::Address);
    p::Address current = 0;
    const p::Address expected = g_base + p::kGetControlsRva;
    if (!readValue(slot, current) || current != expected) {
        logLine("HOOK REFUSED: slot=%08lX current=%08lX expected=%08lX; possible mod conflict",
                static_cast<unsigned long>(slot), static_cast<unsigned long>(current),
                static_cast<unsigned long>(expected));
        return false;
    }
    DWORD oldProtection = 0;
    if (!VirtualProtect(ptr(slot), sizeof(p::Address), PAGE_EXECUTE_READWRITE, &oldProtection)) {
        logLine("HOOK REFUSED: VirtualProtect error=%lu", GetLastError());
        return false;
    }
    auto* cell = reinterpret_cast<PVOID volatile*>(ptr(slot));
    void* const replacement = reinterpret_cast<void*>(&getControlsHook);
    void* const prior = InterlockedCompareExchangePointer(cell, replacement, ptr(expected));
    DWORD ignored = 0;
    const BOOL restored = VirtualProtect(ptr(slot), sizeof(p::Address), oldProtection, &ignored);
    if (!restored) {
        InterlockedCompareExchangePointer(cell, ptr(expected), replacement);
        VirtualProtect(ptr(slot), sizeof(p::Address), oldProtection, &ignored);
        logLine("HOOK REFUSED: protection restore failed; rollback attempted");
        return false;
    }
    if (prior != ptr(expected)) {
        logLine("HOOK REFUSED: slot changed concurrently");
        return false;
    }
    logLine("HOOK installed=1 vtable=%08lX slot=2 original=%08lX; getter code UNCHANGED",
            static_cast<unsigned long>(vtable),
            static_cast<unsigned long>(expected));
    return true;
}

bool rollbackGetterSlot(p::Address vtable) {
    const p::Address slot=vtable+p::kGetterSlot*sizeof(p::Address);
    DWORD oldProtection=0,ignored=0;
    if(!VirtualProtect(ptr(slot),sizeof(p::Address),PAGE_EXECUTE_READWRITE,&oldProtection))return false;
    void* const replacement=reinterpret_cast<void*>(&getControlsHook);
    const auto prior=InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID volatile*>(ptr(slot)),ptr(g_base+p::kGetControlsRva),replacement);
    const bool restored=VirtualProtect(ptr(slot),sizeof(p::Address),oldProtection,&ignored)!=0;
    return (prior==replacement || prior==ptr(g_base+p::kGetControlsRva)) && restored;
}
bool installGetterHook() {
    g.hookAttempted=true;
    const p::Address normal=g_base+p::kPlayerInputVtableRva;
    const p::Address drag=g_base+p::kDragInputVtableRva;
    p::Address normalGetter=0;
    // Preflight BOTH slots before changing either one. Unknown/modded layouts fail closed.
    if(!direction::dragFingerprint(g_base,&readMemory) ||
       !readValue(normal+p::kGetterSlot*4u,normalGetter) || normalGetter!=g_base+p::kGetControlsRva) {
        logLine("HOOK REFUSED: normal/drag preflight mismatch; no getter slot changed");return false;
    }
    if(!installGetterSlot(normal))return false;
    if(!installGetterSlot(drag)) {
        const bool rolledBack=rollbackGetterSlot(normal);
        logLine("HOOK REFUSED: drag install failed; normal rollback=%d",rolledBack?1:0);
        return false;
    }
    logLine("DRAG_INPUT supported=1 normal_vt=008AC670 drag_vt=008AC708; paired owners; native helper unchanged; specialized transmission allowed only with drag input; RPM source still normal HUD");
    return true;
}

// Diagnostic only: bounded, read-only observations, never a fallback resolver.
// Called only from onPoll under the existing state lock AFTER safety handling.
void diagnosePlayerRows(DWORD now) {
    if (g_reportIntervalMs != 200u || !g.requested ||
        g.player.status == p::ResolveStatus::Unique) return;
    const auto context = session::sample(g_base, &readMemory);
    if (!context.readable || context.flow != 6) return;
    if (g.resolverDiagSeen && static_cast<DWORD>(now-g.resolverDiagAt) < 1000u) return;
    g.resolverDiagSeen=true; g.resolverDiagAt=now;
    p::VehicleContainer header{};
    const bool readable=readValue(g_base+p::kVehicleContainerRva,header);
    logLine("RESOLVER_DIAG status=%d flow=%lu flags=%08lX header_read=%d "
            "vtable=%08lX data=%08lX capacity=%lu count=%lu; read-only, not a fallback",
            static_cast<int>(g.player.status),static_cast<unsigned long>(context.flow),
            static_cast<unsigned long>(context.flags),readable?1:0,
            static_cast<unsigned long>(header.vtable),static_cast<unsigned long>(header.data),
            static_cast<unsigned long>(header.capacity),static_cast<unsigned long>(header.count));
    if (!readable || header.vtable!=g_base+p::kVehicleContainerVtableRva ||
        header.data!=g_base+p::kVehicleListRva || header.capacity!=p::kMaxVehicles ||
        header.count>header.capacity) return;
    for (unsigned i=0;i<header.count;++i) {
        p::VehicleEntry row{};
        p::Address cv=0,input=0,iv=0,back=0,getter=0;
        const bool rowOk=readValue(header.data+i*sizeof(row),row);
        // Keep every read bounded within the 32-bit address space.
        const bool carOk=rowOk && p::pointerLike(row.vehicle) && row.vehicle<=0xFFFFFE00u;
        const bool cvOk=carOk && readValue(row.vehicle,cv);
        const bool inputOk=carOk && readValue(row.vehicle+p::kVehicleInputOffset,input);
        const bool inputSafe=inputOk && p::pointerLike(input) && input<=0xFFFFFE00u;
        const bool ivOk=inputSafe && readValue(input,iv);
        const bool backOk=inputSafe && readValue(input+drive::kInputVehicleOffset,back);
        const bool getterOk=ivOk && p::pointerLike(iv) && iv<=0xFFFFFF00u &&
            readValue(iv+p::kGetterSlot*4u,getter);
        const char* reason=!rowOk?"row-read-failed":row.vehicle==0?"null-row":
            row.enabled!=1?"disabled":!carOk?"car-pointer":!cvOk?"car-read-failed":
            cv!=g_base+p::kVehicleVtableRva?"car-vtable":!inputOk?"input-read-failed":
            !inputSafe?"input-pointer":!ivOk?"input-vtable-read-failed":
            !p::supportedInputVtable(g_base,iv)?"input-vtable":"matches-resolver-filters";
        logLine("RESOLVER_ROW index=%u row_read=%d enabled=%u car=%08lX car_vt=%08lX "
                "input_read=%d input=%08lX input_vt_read=%d input_vt=%08lX "
                "back_read=%d back_iv=%08lX getter_read=%d getter=%08lX reject=%s",
                i,rowOk?1:0,static_cast<unsigned>(row.enabled),
                static_cast<unsigned long>(row.vehicle),static_cast<unsigned long>(cv),
                inputOk?1:0,static_cast<unsigned long>(input),ivOk?1:0,
                static_cast<unsigned long>(iv),backOk?1:0,static_cast<unsigned long>(back),
                getterOk?1:0,static_cast<unsigned long>(getter),reason);
    }
    p::VehicleContainer after{};
    const bool stable=readValue(g_base+p::kVehicleContainerRva,after) &&
                      p::sameContainer(header,after);
    logLine("RESOLVER_DIAG_END header_stable=%d; rows are observations, not identity proof",stable?1:0);
}

// Read-only report remains available after a validation fault cleared requested.
// Actual gear/speed are ONLY exposed by drive::sample after all original checks.
void diagnoseDrivetrain(DWORD now) {
    if(g_reportIntervalMs!=200u || g.player.status!=p::ResolveStatus::Unique)return;
    const auto context=session::sample(g_base,&readMemory);
    if(!context.readable || context.flow!=6)return;
    if(g.driveDiagSeen && static_cast<DWORD>(now-g.driveDiagAt)<1000u)return;
    g.driveDiagSeen=true;g.driveDiagAt=now;
    const auto snapshot=drive::sample(g_base,g.player,&readMemory);
    if(snapshot.status==drive::Status::Ok && g.player.inputVtable!=g_base+p::kDragInputVtableRva)return;
    p::Address transmission=0,tvt=0,gearFn=0,iv=0,ivt=0,speedFn=0,ownerVt=0;
    const bool tr=readValue(g.player.vehicle+drive::kTransmissionOffset,transmission);
    const bool ts=tr && p::pointerLike(transmission) && transmission<=0xFFFFFE00u;
    const bool tvr=ts && readValue(transmission,tvt);
    const bool tSafe=tvr && p::pointerLike(tvt) && tvt<=0xFFFFFF00u;
    const bool gr=tSafe && readValue(tvt+drive::kGetGearSlot*4u,gearFn);
    const bool ir=readValue(g.player.input+drive::kInputVehicleOffset,iv);
    const bool is=ir && p::pointerLike(iv) && iv<=0xFFFFFE00u;
    const bool ivr=is && readValue(iv,ivt);
    const bool ivSafe=ivr && p::pointerLike(ivt) && ivt<=0xFFFFFF00u;
    const bool sr=ivSafe && readValue(ivt+drive::kGetSpeedSlot*4u,speedFn);
    const bool ownerRead=g.player.input>=direction::kOwnerOffset &&
        readValue(g.player.input-direction::kOwnerOffset,ownerVt);
    const bool ownerOk=direction::ownerMatches(g_base,g.player,g.player.input-direction::kOwnerOffset,&readMemory);
    const bool settersOk=direction::settersMatch(g_base,&readMemory,g.player.inputVtable);
    logLine("DRIVE_DIAG status=%u car=%08lX input=%08lX input_vt=%08lX "
            "trans_read=%d trans=%08lX trans_vt_read=%d trans_vt=%08lX "
            "gear_fn_read=%d gear_fn=%08lX iv_read=%d iv=%08lX iv_vt_read=%d iv_vt=%08lX "
            "speed_fn_read=%d speed_fn=%08lX owner_read=%d owner_vt=%08lX owner_ok=%d setters_ok=%d "
            "validated_gear=%d validated_speed=%+.5f requested=%d armed=%d; read-only",
            static_cast<unsigned>(snapshot.status),static_cast<unsigned long>(g.player.vehicle),
            static_cast<unsigned long>(g.player.input),static_cast<unsigned long>(g.player.inputVtable),
            tr?1:0,static_cast<unsigned long>(transmission),tvr?1:0,static_cast<unsigned long>(tvt),
            gr?1:0,static_cast<unsigned long>(gearFn),ir?1:0,static_cast<unsigned long>(iv),
            ivr?1:0,static_cast<unsigned long>(ivt),sr?1:0,static_cast<unsigned long>(speedFn),
            ownerRead?1:0,static_cast<unsigned long>(ownerVt),ownerOk?1:0,settersOk?1:0,
            snapshot.gear,snapshot.speed,g.requested?1:0,g.armed?1:0);
}

void updatePlayer(bool report) {
    const p::Player fresh = p::resolvePlayer(g_base, &readMemory);
    const bool changed = fresh.status != g.player.status || fresh.input != g.player.input ||
                         fresh.inputVtable != g.player.inputVtable || fresh.vehicle != g.player.vehicle;
    if(fresh.inputVtable!=g.player.inputVtable)forgetOwnership();
    g.player = fresh; // Must precede stop/cleanup.
    if (changed) {
        g.playerReportPending = true;
        g.directionSeen = false;
        g.directionInput = 0;
        stopTest(StopReason::PlayerChanged);
    }
    if (!report || !g.playerReportPending) return;
    g.playerReportPending = false;
    p::VehicleContainer listHeader{};
    const bool listReadable=readValue(g_base+p::kVehicleContainerRva,listHeader);
    logLine("PLAYER_LIST readable=%d vtable=%08lX data=%08lX capacity=%lu count=%lu; counted list, no sentinel scan",
            listReadable?1:0,static_cast<unsigned long>(listHeader.vtable),
            static_cast<unsigned long>(listHeader.data),
            static_cast<unsigned long>(listHeader.capacity),static_cast<unsigned long>(listHeader.count));
    if (fresh.status == p::ResolveStatus::Unique)
        logLine("PLAYER car=%08lX input=%08lX controls=%08lX input_vt=%08lX",
                static_cast<unsigned long>(fresh.vehicle), static_cast<unsigned long>(fresh.input),
                static_cast<unsigned long>(fresh.controls), static_cast<unsigned long>(fresh.inputVtable));
    else logLine("PLAYER unavailable status=%d; no writes allowed", static_cast<int>(fresh.status));
}
void handleToggle(DWORD now) {
    const bool keyDown = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    const bool keyPressed = keyDown && !g.previousKeyDown;
    g.previousKeyDown = keyDown;
    const std::uint32_t buttonTick = static_cast<std::uint32_t>(InterlockedExchange(&g_buttonEventTick, 0));
    const bool buttonPressed = g_toggleButton != 0 && buttonTick != 0 &&
        static_cast<std::uint32_t>(static_cast<std::uint32_t>(now) - buttonTick) <= 150u;
    if ((keyPressed || buttonPressed) && gameFocused()) {
        logLine("TOGGLE source=%s button=%d", keyPressed ? (buttonPressed ? "F8+S2" : "F8") : "S2", g_toggleButton);
        if (g.requested || g.armed) {
            stopTest(StopReason::User);
            flushStopLog();
        } else if (session::sample(g_base,&readMemory).flow==3) {
            // Known front-end state only, NOT arbitrary loading/paused-race states.
            const session::Snapshot menuGate=session::sample(g_base,&readMemory);
            const DWORD menuNow=GetTickCount();
            if (!menuGate.readable || menuGate.flow!=3 || !gameFocused() || escapeDown() ||
                g.cleanupPending || g.directionInFlight || !g.directionInstalled || !g_directionOriginal ||
                (g.hookAttempted && !g.hookInstalled) || !sampleGood(menuNow) ||
                !g.neutral.ready(menuNow)) {
                logLine("MENU ARM IGNORED: require focused front-end, valid MOZA neutral >=500ms, "
                        "installed direction hook and no pending cleanup");
            } else {
                g.requested=true; g.startQueued=true; g.resumeImmediate=false;
                g.resumeNeutral.tracking=false;
                // armed/liveGate remain OFF. No player/controls pointer needed in menu.
                logLine("WAIT_RACE source=MOZA menu_neutral_approved=1; requested=1 active=0; "
                        "S2/F8 cancels; current axes apply on first valid race poll");
            }
        } else if (escapeDown() || !session::sample(g_base,&readMemory).ready() || !g.hookInstalled || !directionFresh(now) ||
                   g.directionInFlight ||
                   g.player.status != p::ResolveStatus::Unique || g.cleanupPending ||
                   !sampleGood(now) || !g.neutral.ready(now)) {
            logLine("TOGGLE IGNORED hook=%d direction_ready=%d player_status=%d cleanup_pending=%d "
                    "sdk_status=%d sdk_err=%ld fresh=%d neutral_ready=%d; check gate_ready and "
                    "release BOTH pedals and center wheel for >=500ms",
                    g.hookInstalled ? 1 : 0, directionFresh(now) ? 1 : 0,
                    static_cast<int>(g.player.status),
                    g.cleanupPending ? 1 : 0, g.sdkStatus, static_cast<long>(g.sdkSample.sdkError),
                    sampleGood(now) ? 1 : 0, g.neutral.ready(now) ? 1 : 0);
        } else {
            p::InputControls sample{};
            const drive::Snapshot drivetrain = drive::sample(g_base, g.player, &readMemory);
            if (drivetrain.status != drive::Status::Ok ||
                !direction::ownerMatches(g_base, g.player,
                    g.player.input - direction::kOwnerOffset, &readMemory) ||
                !direction::settersMatch(g_base, &readMemory, g.player.inputVtable)) {
                logLine("TOGGLE IGNORED: drivetrain/owner/setter validation failed");
            } else if (!readValue(g.player.controls, sample)) {
                logLine("TOGGLE IGNORED: controls unreadable");
            } else if (!sampleGood(GetTickCount()) || !g.neutral.ready(GetTickCount()) ||
                       !directionFresh(GetTickCount()) || !gameFocused() || escapeDown() ||
                       !session::sample(g_base,&readMemory).ready()) {
                logLine("TOGGLE IGNORED: MOZA freshness/neutral changed during memory read");
            } else {
                forgetOwnership();
                g.pedalMode = m::PedalMode::Off;
                g.testStarted = GetTickCount();
                g.writesAtStart = g.writes;
                g.requested = true;
                g.startQueued=false;
                g.resumeImmediate=false;
                g.resumeNeutral.tracking=false;
                g.armed = true;
                InterlockedExchange(&g_liveGate, 1);
                logLine("START source=MOZA duration=unlimited steering_enabled=%d; "
                        "native direction=MOZA; actual R swaps engine pedals; "
                        "released pedals=native; F8/S2=OFF; Esc/focus/menu=suspend",
                        g_config.enableSteering ? 1 : 0);
            }
        }
    }
}

// Resume ONLY from the game poll callback, never from the SDK worker/watchdog.
// Previously armed sessions skip neutral on recovery, including validated player rebind.
// Explicit menu-approved queued start skips dwell once the first valid player is ready.
// Actual lost/replaced player must acquire a NEW helper heartbeat and pass all checks.
void tryResume() {
    if (!g.requested || g.armed) return;
    const DWORD now=GetTickCount();
    const session::Snapshot gate=session::sample(g_base,&readMemory);
    if (!gameFocused() || escapeDown() || !gate.readable || !gate.ready() ||
        !g.hookInstalled || !directionFresh(now) || g.directionInFlight ||
        g.player.status!=p::ResolveStatus::Unique || g.cleanupPending) {
        g.resumeNeutral.tracking=false;
        if (gameFocused() && !gate.readable) stopTest(StopReason::ContextInvalid);
        return;
    }
    if (!sampleGood(now)) {
        stopTest(g.sdkStatus!=MW05_MOZA_OK || !m::validSample(g.sdkSample) ?
                 StopReason::MozaInvalid : StopReason::MozaStale);
        return;
    }
    const drive::Snapshot drivetrain=drive::sample(g_base,g.player,&readMemory);
    if (drivetrain.status!=drive::Status::Ok ||
        !direction::ownerMatches(g_base,g.player,
            g.player.input-direction::kOwnerOffset,&readMemory) ||
        !direction::settersMatch(g_base,&readMemory,g.player.inputVtable)) {
        // A replacement player must settle before owner/drive validation can pass.
        updatePlayer(false);
        stopTest(StopReason::DriveInvalid);
        return;
    }
    p::InputControls sample{};
    if (!readValue(g.player.controls,sample)) {
        stopTest(StopReason::WriteFailed); return;
    }
    const DWORD after=GetTickCount();
    if (!gameFocused() || escapeDown() ||
        !session::sample(g_base,&readMemory).ready() || !sampleGood(after) ||
        !directionFresh(after)) { g.resumeNeutral.tracking=false; return; }
    // Fresh identity check AFTER the I/O: never auto-arm a replaced object.
    const p::Player fresh=p::resolvePlayer(g_base,&readMemory);
    if (fresh.status!=p::ResolveStatus::Unique || fresh.input!=g.player.input ||
        fresh.inputVtable!=g.player.inputVtable || fresh.vehicle!=g.player.vehicle) {
        updatePlayer(false); g.resumeNeutral.tracking=false; return;
    }
    const DWORD finalNow=GetTickCount();
    if (!sampleGood(finalNow) || !directionFresh(finalNow) || !gameFocused() ||
        escapeDown() || !session::sample(g_base,&readMemory).ready()) {
        g.resumeNeutral.tracking=false; return;
    }
    const bool queued=g.startQueued;
    const bool immediate=queued || g.resumeImmediate;
    if (!immediate) {
        g.resumeNeutral.observe(g.sdkSample,true,g_config,finalNow);
        if (!g.resumeNeutral.ready(finalNow)) return;
    }
    flushStopLog();
    forgetOwnership();
    g.pedalMode=m::PedalMode::Off;
    g.testStarted=finalNow; g.writesAtStart=g.writes;
    g.armed=true; g.resumeNeutral.tracking=false; g.resumeImmediate=false; g.startQueued=false;
    InterlockedExchange(&g_liveGate,1);
    logLine("RESUME source=MOZA neutral_dwell_ms=%u mode=%s; requested=1; duration=unlimited",
            immediate ? 0u : 500u, queued ? "menu-queued-start" :
            (immediate ? "validated-auto-resume" : "validated-neutral"));
}

void watchdogTick() {
    Guard guard;
    if (g.requested || g.armed || g.cleanupPending) {
        updatePlayer(false);
        enforceSafety(GetTickCount());
        cleanupAxes();
    }
    flushStopLog();
    handleToggle(GetTickCount());
}
// DirectInput runs on its own thread: never calls a driver under the axes lock.
// Only a fresh edge timestamp crosses into the existing toggle path.
DWORD WINAPI buttonThread(void*) {
#if !defined(MW05_PROBE_HOST_QA)
    mw05_buttons::DirectInputButton reader;
    mw05_buttons::PressGate edge;
    DWORD retryAt = 0;
    int previousStatus = -999;
    unsigned char previousDirections = 0;
    for (;;) {
        Sleep(10);
        if (InterlockedCompareExchange(&g_ready, 0, 0) == 0) continue;
        const DWORD now = GetTickCount();
        const HWND window = gameFocused() ? GetForegroundWindow() : nullptr;
        if (!window) {
            reader.closeDevice();
            publishMenu(false,0,now);
            edge.reset();
            InterlockedExchange(&g_buttonEventTick, 0);
            retryAt = 0;
            continue;
        }
        if (!reader.connected()) {
            if (retryAt && static_cast<DWORD>(now - retryAt) < 2000u) continue;
            retryAt = now;
            const int status = reader.connect(window, g_toggleButton ? g_toggleButton:37, g_menuEnabled);
            if (status != previousStatus) {
                Guard guard;
                logLine("BUTTON_INPUT status=%d button=%d index=%d device=R3 Racing Wheel and Pedals; 0=ready 1=not-found 2=ambiguous 3=driver-error; F8 remains available",
                        status, g_toggleButton, g_toggleButton - 1);
                previousStatus = status;
            }
            edge.reset();
            if (status != 0) continue;
        }
        mw05_buttons::ButtonState buttons{};
        const DWORD readStarted = GetTickCount();
        const bool valid = reader.readState(window, buttons);
        const bool down=buttons.toggle;
        const DWORD finished = GetTickCount();
        const bool timely = static_cast<DWORD>(finished-readStarted) <= 100u;
        const bool focused = gameFocused() && GetForegroundWindow() == window;
        const bool pressed = edge.observe(valid && timely && focused, down, static_cast<std::uint32_t>(finished));
        if (!valid || !timely || !focused) {
            edge.reset();
            publishMenu(false,0,finished);
            InterlockedExchange(&g_buttonEventTick, 0);
            reader.closeDevice();
            retryAt = 0;
            continue;
        }
        publishMenu(valid && timely && focused,buttons.directions,finished);
        if (g_menuEnabled && buttons.directions!=previousDirections) {
            unsigned char accepted=0;
            { MenuGuard guard; accepted=g_menuSample.directions; }
            const auto menuContext=session::sample(g_base,&readMemory);
            { Guard guard; logLine("MENU_DPAD raw=%u accepted=%u flow=%lu flags=%08lX readable=%d focused=%d (up=1 right=2 down=4 left=8)",
                                  static_cast<unsigned>(buttons.directions),static_cast<unsigned>(accepted),
                                  static_cast<unsigned long>(menuContext.flow),static_cast<unsigned long>(menuContext.flags),
                                  menuContext.readable?1:0,gameFocused()?1:0); }
            previousDirections=buttons.directions;
        }
        if (pressed && g_toggleButton != 0) {
            // Recheck before publishing; stale presses never survive driver stalls.
            if (gameFocused() && GetForegroundWindow() == window)
                InterlockedExchange(&g_buttonEventTick, static_cast<LONG>(finished ? finished : 1u));
            else edge.reset();
        }
    }
#else
    return 0; // Host integration tests publish edge timestamps explicitly.
#endif
}
DWORD WINAPI watchdogThread(void*) {
    for (;;) {
        Sleep(25);
        if (InterlockedCompareExchange(&g_ready, 0, 0) != 0) watchdogTick();
    }
}

bool rootPath(const wchar_t* filename, wchar_t (&out)[MAX_PATH]) {
    const std::size_t rootLength = std::wcslen(g_gameRoot), nameLength = std::wcslen(filename);
    if (rootLength == 0 || rootLength + nameLength >= MAX_PATH) return false;
    std::wmemcpy(out, g_gameRoot, rootLength);
    std::wmemcpy(out + rootLength, filename, nameLength + 1);
    return true;
}
void publishMoza(int status, const MW05MozaSample& sample, DWORD now) {
    Guard guard;
    g.sdkStatus = status;
    g.sdkSample = sample;
    ++g.sampleSequence;
    g.neutral.observe(sample, status == MW05_MOZA_OK && m::validSample(sample),
                      g_config, now);
    // Reader ONLY publishes. It never writes game memory or runs cleanup.
    // Getter/onPoll/watchdog consume this status and handle cancellation.
    // The independent watchdog also handles an SDK call that never returns.
}
DWORD WINAPI mozaThread(void*) {
    while (InterlockedCompareExchange(&g_ready, 0, 0) == 0) Sleep(10);
    wchar_t path[MAX_PATH]{};
    if (!rootPath(L"MW05_MozaBridge.dll", path)) {
        Guard guard; logLine("MOZA DISABLED: cannot form bridge path"); return 0;
    }
    // Absolute path; dependencies are sought in the bridge's directory and
    // Windows trusted/default DLL directories, not an arbitrary working folder.
    HMODULE bridge = LoadLibraryExW(path, nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!bridge) {
        const DWORD error = GetLastError();
        Guard guard;
        logLine("MOZA DISABLED: LoadLibraryExW(MW05_MozaBridge.dll) error=%lu; "
                "bridge and x86 MOZA_SDK.dll must be in the game ROOT", error);
        return 0;
    }
    const auto init = reinterpret_cast<MW05MozaInitFn>(GetProcAddress(bridge, "MW05Moza_Init"));
    const auto read = reinterpret_cast<MW05MozaReadFn>(GetProcAddress(bridge, "MW05Moza_Read"));
    if (!init || !read) {
        Guard guard; logLine("MOZA DISABLED: bridge C exports missing; wrong bridge build"); return 0;
    }
    const int status = init(MW05_MOZA_ABI, sizeof(MW05MozaSample));
    {
        Guard guard;
        g.sdkStatus = status;
        logLine("MOZA bridge_init=%d abi=%08X packet_bytes=%u; no FFB effect calls",
                status, MW05_MOZA_ABI, static_cast<unsigned>(sizeof(MW05MozaSample)));
    }
    if (status != MW05_MOZA_OK) return 0;
    for (;;) {
        MW05MozaSample sample{};
        const DWORD begin = GetTickCount();
        int result = read(&sample, sizeof(sample)); // Outside all game/state locks.
        const DWORD now = GetTickCount();
        if (static_cast<DWORD>(now - begin) >= m::kMaxReadMs)
            result = MW05_MOZA_SLOW_READ;
        publishMoza(result, sample, now);
        Sleep(10); // Approx. 100 Hz; actual rate depends on SDK/system scheduling.
    }
}

void onPoll() {
    if (InterlockedCompareExchange(&g_ready, 0, 0) == 0) return;
    Guard guard;
    updatePlayer(true);
    const DWORD now = GetTickCount();
    enforceSafety(now);
    cleanupAxes();
    flushStopLog();
    if (!g.hookAttempted && g.player.status == p::ResolveStatus::Unique)
        g.hookInstalled = installGetterHook();

    handleToggle(now);
    tryResume();
    diagnosePlayerRows(now);
    diagnoseDrivetrain(now);
    if (static_cast<DWORD>(now - g.reportAt) < g_reportIntervalMs) return;
    g.reportAt = now;
    p::InputControls game{};
    const bool readable = g.player.status == p::ResolveStatus::Unique &&
                          readValue(g.player.controls, game);
    const bool good = sampleGood(now);
    const m::Axes command = good ? m::axes(g.sdkSample, g_config) : m::Axes{};
    // Logging does not call native getters/Shift or write a gear.
    const drive::Snapshot drivetrain = drive::sample(g_base, g.player, &readMemory);
    const session::Snapshot gate=session::sample(g_base,&readMemory);
    // Read-only HUD diagnostic. Never controls axes/request/menu/bridge.
    const bool dragHud=hud::isDrag(g_base,g.player);
    hud::Snapshot ht{};DWORD hudCalls=0,hudCaptures=0,hudAge=0;
    { HudGuard hudGuard;
      const auto& cache=dragHud?g_dragHudCache:g_hudCache;
      ht=hud::select(cache,now,g.player,gate,gameFocused());
      hudCalls=dragHud?g_dragHudCalls:g_hudCalls;hudCaptures=dragHud?g_dragHudCaptures:g_hudCaptures;
      hudAge=cache.seen ? now-cache.at:0; }
    if (!(dragHud?g_dragHudInstalled:g_hudInstalled)) ht=hud::failure(hud::Status::HookUnavailable);
    logLine("HUD_TELEMETRY status=%u hud=%08lX vehicle_context=%08lX age_ms=%lu "
            "current_hud=%.3f threshold_hud=%.3f scale_hud=%.3f gear_hud=%d active=%d "
            "hook_calls_total=%lu captures_total=%lu source=%s; HUD source, not engine limiter; UDP is optional and independent",
            static_cast<unsigned>(ht.status),static_cast<unsigned long>(ht.widget),
            static_cast<unsigned long>(ht.vehicle),static_cast<unsigned long>(hudAge),
            static_cast<double>(ht.current),static_cast<double>(ht.threshold),
            static_cast<double>(ht.scale),ht.gear,ht.active?1:0,
            static_cast<unsigned long>(hudCalls),static_cast<unsigned long>(hudCaptures),dragHud?"drag":"normal");
    if (g_menuEnabled) {
        DWORD calls=0,supplied=0,native=0,merged=0;int action=-1;
        { MenuGuard menuGuard;calls=g_menuCalls;supplied=g_menuSupplied;action=g_menuLastAction;native=g_menuLastNative;merged=g_menuLastMerged; }
        logLine("MENU_STAT mask_calls_total=%lu supplied_total=%lu last_player=%d native=%08lX merged=%08lX",calls,supplied,action,native,merged);
    }
    logLine("STAT armed=%d test_ms=%lu sdk_status=%d sdk_err=%ld sample_seq=%lu age_ms=%lu "
            "moza_valid=%d neutral_ready=%d raw_gas=%d raw_brake=%d raw_axle=%d angle=%+.3f "
            "gas_cmd=%.4f brake_cmd=%.4f steer_cmd=%+.4f steering_enabled=%d "
            "readable=%d gas_now=%.4f brake_now=%.4f steer_now=%+.4f handbrake=%.3f "
            "getter_calls=%lu player_calls_total=%lu writes=%lu errors_total=%lu cleanup_pending=%d "
            "pedal_mode=%u pedal_writes=%lu pedal_handoffs_total=%lu "
            "before_game_gas=%.4f before_game_brake=%.4f "
            "drive_status=%u gear_id=%d speed_game=%+.5f key_up=%d key_down=%d "
            "dir_ready=%d dir_calls=%lu dir_moza=%lu reverse_maps=%lu dir_inflight=%d "
            "requested=%d suspended=%d focused=%d gate_ready=%d flow_state=%lu sim_flags=%08lX resume_neutral_ready=%d waiting_for_race=%d",
            g.armed ? 1 : 0, static_cast<unsigned long>(g.armed ? now-g.testStarted : 0),
            g.sdkStatus, static_cast<long>(g.sdkSample.sdkError),
            static_cast<unsigned long>(g.sampleSequence),
            static_cast<unsigned long>(now-g.sdkSample.readStartedTick),
            good ? 1 : 0, g.neutral.ready(now) ? 1 : 0,
            static_cast<int>(g.sdkSample.throttle), static_cast<int>(g.sdkSample.brake),
            static_cast<int>(g.sdkSample.wheelAxle), static_cast<double>(g.sdkSample.angleDegrees),
            static_cast<double>(command.gas), static_cast<double>(command.brake),
            static_cast<double>(command.steer), g_config.enableSteering ? 1 : 0,
            readable ? 1 : 0, static_cast<double>(game.gas), static_cast<double>(game.brake),
            static_cast<double>(game.steering), static_cast<double>(game.handBrake),
            static_cast<unsigned long>(g.totalCalls-g.reportCalls),
            static_cast<unsigned long>(g.playerCalls),
            static_cast<unsigned long>(g.writes-g.reportWrites),
            static_cast<unsigned long>(g.writeErrors), g.cleanupPending ? 1 : 0,
            static_cast<unsigned>(g.pedalMode),
            static_cast<unsigned long>(g.pedalWrites-g.reportPedalWrites),
            static_cast<unsigned long>(g.pedalHandoffs),
            static_cast<double>(g.beforeGameGas), static_cast<double>(g.beforeGameBrake),
            static_cast<unsigned>(drivetrain.status), drivetrain.gear,
            static_cast<double>(drivetrain.speed),
            (GetAsyncKeyState(VK_UP) & 0x8000) != 0 ? 1 : 0,
            (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0 ? 1 : 0,
            directionFresh(now) ? 1 : 0,
            static_cast<unsigned long>(g.directionCalls-g.reportDirectionCalls),
            static_cast<unsigned long>(g.directionMozaCalls-g.reportDirectionMozaCalls),
            static_cast<unsigned long>(g.reverseMaps-g.reportReverseMaps),
            g.directionInFlight ? 1 : 0, g.requested ? 1 : 0,
            g.requested && !g.armed ? 1 : 0, gameFocused() ? 1 : 0,
            gate.ready() ? 1 : 0, static_cast<unsigned long>(gate.flow),
            static_cast<unsigned long>(gate.flags), g.resumeNeutral.ready(now) ? 1 : 0, g.startQueued ? 1 : 0);
    g.reportCalls = g.totalCalls;
    g.reportWrites = g.writes;
    g.reportPedalWrites = g.pedalWrites;
    g.reportDirectionCalls = g.directionCalls;
    g.reportDirectionMozaCalls = g.directionMozaCalls;
    g.reportReverseMaps = g.reverseMaps;
}

hud::Snapshot selectUdpSnapshot(std::uint32_t now) {
    if(InterlockedCompareExchange(&g_ready,0,0)==0)
        return hud::failure(hud::Status::HookUnavailable);
    const auto player=p::resolvePlayer(g_base,&readMemory);
    if(player.status!=p::ResolveStatus::Unique)return hud::failure(hud::Status::NoPlayer);
    const bool drag=hud::isDrag(g_base,player);
    if(!(drag?g_dragHudInstalled:g_hudInstalled))return hud::failure(hud::Status::HookUnavailable);
    const auto context=session::sample(g_base,&readMemory);
    const bool focused=gameFocused();
    HudGuard guard;
    return hud::select(drag?g_dragHudCache:g_hudCache,now,player,context,focused);
}
DWORD WINAPI telemetryThread(void*) {
    while(InterlockedCompareExchange(&g_ready,0,0)==0)Sleep(10);
    mw05_udp::Transport transport;
    if(!transport.open(g_udpPort)){
        logLine("UDP_START failed wsa_error=%d; controls unchanged",transport.error);return 0;
    }
    logLine("UDP_START destination=127.0.0.1:%u packet_bytes=324; send success is NOT receiver acknowledgement",g_udpPort);
    mw05_udp::Stream stream{};
    unsigned long sent=0,cleared=0,errors=0;
    DWORD report=GetTickCount();
    for(;;){
        const DWORD now=GetTickCount();
        const auto snapshot=selectUdpSnapshot(static_cast<std::uint32_t>(now));
        const auto action=mw05_udp::step(stream,snapshot);
        if(action!=mw05_udp::Action::None){
            const auto packet=mw05_udp::encode(snapshot,static_cast<std::uint32_t>(now),action==mw05_udp::Action::Live);
            // No control/HUD lock held. Nonblocking, loopback only; no reads or game calls here.
            if(transport.send(packet.data(),static_cast<unsigned>(packet.size()))){
                if(action==mw05_udp::Action::Live)++sent;else ++cleared;
            }else ++errors;
        }
        if(static_cast<DWORD>(now-report)>=5000u){
            report=now;
            logLine("UDP_STAT live=%d hud_status=%u sent_total=%lu clear_total=%lu errors_total=%lu "
                    "wsa_error=%d rpm_hud=%.3f reference_hud=%.3f; local send only, receiver unconfirmed",
                    stream.live?1:0,static_cast<unsigned>(snapshot.status),sent,cleared,errors,
                    transport.error,stream.live?snapshot.current:0.0,stream.live?snapshot.scale:0.0);
        }
        Sleep(50);
    }
}

bool openLogAndConfig() {
    const DWORD size = GetModuleFileNameW(nullptr, g_gameRoot, MAX_PATH);
    if (size == 0 || size >= MAX_PATH) return false;
    wchar_t* slash = std::wcsrchr(g_gameRoot, L'\\');
    if (!slash) slash = std::wcsrchr(g_gameRoot, L'/');
    if (!slash) return false;
    slash[1] = L'\0';
    wchar_t path[MAX_PATH]{};
    if (!rootPath(L"mw05_directdrive.log", path)) return false;
#if defined(MW05_PROBE_HOST_QA)
    g_log = nullptr; // No test files opened by the host mock.
#else
    g_log = _wfopen(path, L"wb");
#endif
    logLine("MW05 MOZA DirectDrive v0.7.16 DRAG TELEMETRY TEST 1; default OFF; LIVE axes via MSVC C-ABI bridge; no FFB calls");
    logLine("HUD_TELEMETRY: 0=no-sample 1=ok 2=no-player 3=context-blocked 4=layout-mismatch "
            "5=read-failed 6=value-rejected 7=changed-during-read 8=inactive 9=stale "
            "10=player-context-changed 11=ambiguous-HUD 12=hook-unavailable; invalid fields zero/gear=-1; "
            "separate optional normal/drag HUD tick hooks; no game data writes; optional loopback RPM output; no FFB calls");
    logLine("COUNTED_LIST header=009352A0 data=009352B0 capacity=160; scan active count only; stale tail ignored; header rechecked");
    logLine("TRANSITION_FIX duplicate rows of the same validated vehicle/input pair count once; different identities remain ambiguous; no session mask change; automatic player rebind skips neutral only for previously-active intent");
    logLine("MENU_ARM front-end flow=3; S2/F8 queues neutral-approved intent, no axis writes; race entry requires all runtime gates; manual OFF cancels");
    logLine("RESUME_POLICY previously-active session=auto after full player/helper/context revalidation; manual enable=neutral 500ms; current axes apply on resume");
    logLine("NEUTRAL released pedals=native logic; no constant brake injection; "
            "pedal_mode: 0=OFF 1=native_neutral 2=MOZA; bridge ABI unchanged 00060000");
    logLine("NATIVE REVERSE: substitute logical pedals only in existing game helper calls; "
            "no direct Shift, no synthesized helper calls, no gear writes; "
            "gear_id 0=R 1=N 2=first 3=second ... 9=eighth; "
            "drive_status=1 valid, otherwise gear_id=-1 and speed_game=0 placeholder; "
            "speed_game=native signed GetSpeed, no HUD-unit conversion");
    if (!rootPath(L"mw05_directdrive.ini", path)) return false;
    const int udpEnabled=static_cast<int>(GetPrivateProfileIntW(L"Telemetry",L"Enabled",0,path));
    const int udpPort=static_cast<int>(GetPrivateProfileIntW(L"Telemetry",L"Port",20066,path));
    g_udpEnabled=udpEnabled==1 && udpPort>=1 && udpPort<=65535;
    if((udpEnabled!=0 && udpEnabled!=1) || udpPort<1 || udpPort>65535)
        logLine("UDP_CONFIG invalid; telemetry OFF only; controls unchanged");
    g_udpPort=(udpPort>=1 && udpPort<=65535)?static_cast<unsigned>(udpPort):20066u;
    logLine("UDP_CONFIG enabled=%d destination=127.0.0.1:%u format=HorizonDash324_RPM_ONLY rate_hz=20 "
            "source_hz<=10 max_field=HUD_scale idle/speed/gear=unavailable_zero; S2/F8 controls axes only",g_udpEnabled?1:0,g_udpPort);
    const int range = static_cast<int>(GetPrivateProfileIntW(L"MOZA", L"WheelRangeDegrees", 900, path));
    const int invert = static_cast<int>(GetPrivateProfileIntW(L"MOZA", L"InvertSteering", 0, path));
    const int steering = static_cast<int>(GetPrivateProfileIntW(L"MOZA", L"EnableSteering", 1, path));
    const int menuNavigation = static_cast<int>(GetPrivateProfileIntW(L"DirectDrive", L"MenuNavigation", 0, path));
    const int toggleButton = static_cast<int>(GetPrivateProfileIntW(L"DirectDrive", L"ToggleButton", 37, path));
    const int diagnostics = static_cast<int>(GetPrivateProfileIntW(L"DirectDrive", L"Diagnostics", 0, path));
    if (range < 180 || range > 2520 || (invert != 0 && invert != 1) ||
        (steering != 0 && steering != 1) || (diagnostics != 0 && diagnostics != 1) ||
        toggleButton < 0 || toggleButton > 128 || (menuNavigation!=0 && menuNavigation!=1) ||
        (menuNavigation && toggleButton>=5 && toggleButton<=8)) {
        logLine("DISABLED: invalid INI; WheelRangeDegrees=180..2520; MOZA flags and DirectDrive.Diagnostics must be 0 or 1; ToggleButton=0..128; MenuNavigation=0/1; menu buttons5..8 reserved");
        return false;
    }
    g_menuEnabled = menuNavigation!=0;
    g_toggleButton = toggleButton;
    g_config = {range, invert != 0, steering != 0};
    g_reportIntervalMs = diagnostics ? 200u : 5000u;
    logLine("CONFIG full_range=%d half_range=%.1f invert_steering=%d steering_enabled=%d "
            "pedal_deadzone=1%%; F8 toggles at neutral; duration=unlimited; Esc/Pause/focus/menu suspends; "
            "diagnostics=%d stat_interval_ms=%lu",
            range, range*0.5, invert, steering, diagnostics,
            static_cast<unsigned long>(g_reportIntervalMs));
    logLine("BUTTON_CONFIG ToggleButton=%d (one-based); 0=disabled; F8 preserved; no keyboard emulation", g_toggleButton);
    logLine("MENU_CONFIG MenuNavigation=%d; default OFF; buttons5/6/7/8; B/X/A unchanged; UI only: flow3 or flow6 flags exactly 2/10hex; axes gate unchanged", menuNavigation);
    if (!g_log) logLine("WARNING: cannot create game-root log; DebugView output only");
    return true;
}

bool validateImage() {
    if (g_base != p::kImageBase) {
        logLine("DISABLED: base=%08lX; SDK on_poll requires 00400000", static_cast<unsigned long>(g_base));
        return false;
    }
    std::uint16_t mz = 0, machine = 0, magic = 0;
    std::uint32_t peOffset = 0, signature = 0, imageSize = 0;
    if (!readValue(g_base, mz) || mz != 0x5A4D ||
        !readValue(g_base+0x3C, peOffset) || peOffset > 0x100000 ||
        !readValue(g_base+peOffset, signature) || signature != 0x4550 ||
        !readValue(g_base+peOffset+4, machine) || machine != 0x014C ||
        !readValue(g_base+peOffset+24, magic) || magic != 0x010B ||
        !readValue(g_base+peOffset+0x50, imageSize) ||
        imageSize < p::kVehicleListRva + p::kMaxVehicles*8) {
        logLine("DISABLED: invalid/unexpected PE32 image"); return false;
    }
    const unsigned char getterExpected[] = {0x8D,0x41,0x08,0xC3};
    const unsigned char steerExpected[] = {0x8B,0x44,0x24,0x04,0x89,0x41,0x0C,0xC2,0x04,0x00};
    unsigned char getter[sizeof(getterExpected)]{}, steer[sizeof(steerExpected)]{};
    p::Address getterSlot = 0, steerSlot = 0;
    const p::Address vt = g_base+p::kPlayerInputVtableRva;
    if (!readValue(vt+2*4, getterSlot) || getterSlot != g_base+p::kGetControlsRva ||
        !readMemory(getterSlot, getter, sizeof(getter)) ||
        std::memcmp(getter, getterExpected, sizeof(getter)) != 0 ||
        !readValue(vt+5*4, steerSlot) || steerSlot < g_base ||
        steerSlot > g_base+imageSize-sizeof(steer) ||
        !readMemory(steerSlot, steer, sizeof(steer)) ||
        std::memcmp(steer, steerExpected, sizeof(steer)) != 0) {
        logLine("DISABLED: getter/setter fingerprint mismatch or another mod changed slot");
        return false;
    }
    g_original = reinterpret_cast<Getter>(ptr(getterSlot));
    if (!session::fingerprint(g_base,&readMemory)) {
        logLine("DISABLED: session gate code fingerprints mismatch"); return false;
    }
    logLine("SESSION GATE flow=00925E90 race=6 sim_flags=00911FE8 mask=0003483B; read only; runtime test required");
    logLine("FINGERPRINT OK base=%08lX vt=%08lX getter=%08lX",
            static_cast<unsigned long>(g_base), static_cast<unsigned long>(vt),
            static_cast<unsigned long>(getterSlot));
    return true;
}
} // namespace

NFSMW_PLUGIN_MAIN() {
    g_started = GetTickCount();
    if (!openLogAndConfig()) return NFSMW_FAIL;
    g_base = address(GetModuleHandleA(nullptr));
    if (!validateImage()) return NFSMW_FAIL;
    // Install before creating our workers/holding their lock. We publish the
    // created trampoline into g_directionOriginal BEFORE enabling the callback.
    g.directionInstalled = installDirectionHook();
    const bool poll = nfsmw::input::on_poll(&onPoll);
    logLine("on_poll installed=%d", poll ? 1 : 0);
    if (!poll) return NFSMW_FAIL;
    const bool menuInstalled=installMenuHook();
    if (g_menuEnabled && !menuInstalled) g_menuEnabled=false; // immutable before workers start
    g_hudInstalled=installHudHook(); // Optional; independent from controls.
    g_dragHudInstalled=installDragHudHook(); // Separate drag widget and cache.
    HANDLE watchdog = CreateThread(nullptr, 0, &watchdogThread, nullptr, 0, nullptr);
    if (!watchdog) {
        logLine("DISABLED: watchdog creation failed error=%lu", GetLastError());
        return NFSMW_OK; // onPoll stays inert.
    }
    CloseHandle(watchdog);
    HANDLE moza = CreateThread(nullptr, 0, &mozaThread, nullptr, 0, nullptr);
    if (!moza) {
        logLine("DISABLED: MOZA worker creation failed error=%lu", GetLastError());
        return NFSMW_OK;
    }
    CloseHandle(moza);
    if (g_toggleButton != 0 || g_menuEnabled) {
        HANDLE buttons = CreateThread(nullptr, 0, &buttonThread, nullptr, 0, nullptr);
        if (buttons) CloseHandle(buttons);
        else logLine("BUTTON_INPUT unavailable: thread creation failed error=%lu; use F8", GetLastError());
    }
    g.previousKeyDown = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    g.reportAt = GetTickCount();
    if(g_udpEnabled && (g_hudInstalled || g_dragHudInstalled)){
        HANDLE telemetry=CreateThread(nullptr,0,&telemetryThread,nullptr,0,nullptr);
        if(telemetry)CloseHandle(telemetry);
        else logLine("UDP_START failed thread error=%lu; controls unchanged",GetLastError());
    }else if(g_udpEnabled)logLine("UDP_START refused: HUD hook unavailable; controls unchanged");
    InterlockedExchange(&g_ready, 1);
    return NFSMW_OK;
}
