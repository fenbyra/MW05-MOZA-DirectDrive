#define MW05_PROBE_HOST_QA
#include "../examples/mw05_mirror_probe/main.cpp"
#include <cassert>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <limits>
#include <type_traits>

namespace qa {
std::map<std::uintptr_t,unsigned char> memory;
std::mutex lock,hudLock;
std::vector<std::string> messages;
std::vector<std::pair<p::Address,SIZE_T>> attemptedWrites;
DWORD clock = 1000;
bool focused = true, key = false, esc = false, pause = false, wrongGetter = false;
bool upKey = false, downKey = false;
bool mhInitOk = true, mhNullTrampoline = false;
bool emulateDirection = false, reenterGetter = false;
int mhCreateStatus = MH_OK, mhEnableStatus = MH_OK;
unsigned nativeCalls = 0, mhCreates = 0, mhEnables = 0, mhRemoves = 0;
float nativeGasArg = 0, nativeBrakeArg = 0;
void (*duringNative)() = nullptr;
void (*onFinalGate)() = nullptr;
void (*duringEnable)() = nullptr;
bool failWrites = false, failPedals = false, partialPedalsOnce = false;
p::Address failFloatAddress = 0;
DWORD advanceOnControlsRead = 0, advanceOnSteerWrite = 0;
unsigned memoryWrites = 0;
int cfgUdp=0,cfgPort=20066;
int cfgRange = 900, cfgInvert = 0, cfgSteer = 1, cfgDiagnostics = 0, cfgButton = 37, cfgMenu = 0;
constexpr p::Address car = 0x03203870, input = 0x0320F5A8, controls = input+8;
constexpr p::Address transmission = 0x0321FB3C;
std::uint32_t menuNativeValue=0;unsigned menuNativeCalls=0;
std::uint32_t nativeMenu(void*,unsigned char) {++menuNativeCalls;return menuNativeValue;}
void driveMemory(std::uint32_t gear, float speed);
void nativeDirection(void*,float,float);
template<class T> void put(p::Address a,const T& value) {
    const auto* b = reinterpret_cast<const unsigned char*>(&value);
    for (std::size_t i=0; i<sizeof(value); ++i) memory[a+i]=b[i];
    if constexpr(std::is_same<T,p::VehicleEntry>::value) {
        const auto first=p::kImageBase+p::kVehicleListRva;
        if(a>=first && a<first+p::kMaxVehicles*8 && (a-first)%8==0) {
            std::uint32_t count=0;
            for(unsigned j=0;j<4;++j) {
                const auto it=memory.find(p::kImageBase+p::kVehicleContainerRva+12+j);
                if(it!=memory.end())count|=std::uint32_t(it->second)<<(8*j);
            }
            const auto i=(a-first)/8;
            count=value.vehicle ? (count>i?count:i+1) : i;
            put(p::kImageBase+p::kVehicleContainerRva,p::VehicleContainer{
                p::kImageBase+p::kVehicleContainerVtableRva,first,p::kMaxVehicles,count});
        }
    }
}
template<class T> T get(p::Address a) {
    T v{}; assert(readValue(a,v)); return v;
}
constexpr p::Address hudWidget=0x11243880u;
unsigned hudNativeCalls=0;void* hudSelf=nullptr;std::uint32_t hudArg=0;
bool hudChangeValue=false,hudPause=false;
void nativeHud(void* self,std::uint32_t arg) {
    assert(lock.try_lock());lock.unlock();assert(hudLock.try_lock());hudLock.unlock();
    ++hudNativeCalls;hudSelf=self;hudArg=arg;
    if(hudChangeValue)put(address(self)+0x54,4800.0f);
    if(hudPause)put(p::kImageBase+session::kSimFlagsRva,std::uint32_t{2});
}
void hudFixture(p::Address w=hudWidget) {
    put(w,p::kImageBase+hud::kVtableRva);put(w+0x30,p::kImageBase+hud::kSecondaryRva);
    put(w+0x54,800.0f);put(w+0x58,6400.0f);put(w+0x5C,7400.0f);put(w+0x60,2);
    put(w+0x18,std::uint32_t{2});put(w+0x1C,std::uint32_t{0});put(w+0x20,std::uint32_t{2});put(w+0x24,std::uint32_t{0});
    put(p::kImageBase+hud::kTickRva,hud::kEntry);
    put(p::kImageBase+0x17A8B7,hud::kReturn);put(p::kImageBase+0x17A8F2,hud::kReturn);
    put(p::kImageBase+hud::kVtableRva,hud::kVtable);put(p::kImageBase+hud::kSecondaryRva,hud::kSecondary);
    put(p::kImageBase+0x17A708,hud::kRatio);put(p::kImageBase+0x17A75B,hud::kThreshold);
    put(p::kImageBase+0x19EA1E,hud::kLayout);put(p::kImageBase+0x19EA41,hud::kCtor);
}
void addCar(unsigned i,p::Address object,p::Address in) {
    put(p::kImageBase+p::kVehicleListRva+i*8,p::VehicleEntry{object,1,{0,0,0}});
    put(object,p::kImageBase+p::kVehicleVtableRva);
    put(object+p::kVehicleInputOffset,in);
    put(in,p::kImageBase+p::kPlayerInputVtableRva);
    p::InputControls c{};
    c.banking=0.12f; c.steering=-0.2f; c.steeringVertical=-0.3f;
    c.strafeVertical=0.4f; c.strafeHorizontal=-0.5f; c.brake=1;
    c.handBrake=0; c.actionButton=1; c.nos=1;
    put(in+8,c);
}
void* original(void* self) { return ptr(address(self)+(wrongGetter ? 12u : 8u)); }
MW05MozaSample raw(std::int16_t gas=-32768,std::int16_t brake=-32768,float angle=0) {
    MW05MozaSample s{};
    s.abi=MW05_MOZA_ABI; s.bytes=sizeof(s); s.readStartedTick=static_cast<uint32_t>(clock);
    s.throttle=gas; s.brake=brake; s.angleDegrees=angle; return s;
}
void publish(std::int16_t gas=-32768,std::int16_t brake=-32768,float angle=0) {
    publishMoza(0,raw(gas,brake,angle),clock);
}
void heartbeat() {
    // Explicit fixture for an otherwise healthy game's recent helper call.
    // The real publisher above does NOT update this and remains publish-only.
    g.directionSeen=true; g.directionInput=g.player.input; g.directionSeenAt=clock;
}
void directionMemory() {
    put(input-direction::kOwnerOffset,p::kImageBase+direction::kOwnerVtableRva);
    put(p::kImageBase+direction::kHelperRva,direction::kHelperEntry);
    put(p::kImageBase+direction::kHelperReturnRva,direction::kHelperReturn);
    put(p::kImageBase+direction::kFetchCallRva,direction::kFetchCall);
    put(p::kImageBase+direction::kReverseSwapRva,direction::kReverseSwap);
    put(p::kImageBase+direction::kSetGasRva,direction::kSetGasCode);
    put(p::kImageBase+direction::kSetBrakeRva,direction::kSetBrakeCode);
    put(p::kImageBase+p::kPlayerInputVtableRva+6*4,p::kImageBase+direction::kSetGasRva);
    put(p::kImageBase+p::kPlayerInputVtableRva+7*4,p::kImageBase+direction::kSetBrakeRva);
}
void reset() {
    g_hudOriginal=nullptr;g_hudInstalled=false;g_hudCache=hud::Cache{};
    g_hudCalls=g_hudCaptures=g_hudAttemptAt=0;g_hudAttempted=false;g_hudAttemptWidget=0;
    hudNativeCalls=0;hudSelf=nullptr;hudArg=0;hudChangeValue=hudPause=false;
    memory.clear(); messages.clear(); attemptedWrites.clear(); clock=1000;
    focused=true; key=esc=pause=wrongGetter=false;
    upKey=downKey=false;
    mhInitOk=true; mhNullTrampoline=false; emulateDirection=reenterGetter=false;
    mhCreateStatus=mhEnableStatus=MH_OK;
    nativeCalls=mhCreates=mhEnables=mhRemoves=0;
    nativeGasArg=nativeBrakeArg=0;
    duringNative=onFinalGate=duringEnable=nullptr;
    failWrites=failPedals=partialPedalsOnce=false; failFloatAddress=0;
    advanceOnControlsRead=advanceOnSteerWrite=0; memoryWrites=0;
    cfgUdp=0;cfgPort=20066;g_udpEnabled=false;g_udpPort=20066;
    cfgRange=900; cfgInvert=0; cfgSteer=1; cfgDiagnostics=0; cfgButton=37; cfgMenu=0; g_reportIntervalMs=5000;
    g_menuEnabled=false; g_menuOriginal=nullptr; g_menuRelease=menu::ReleaseGate{}; g_menuSample=menu::Sample{}; g_menuCalls=g_menuSupplied=0; g_menuLastAction=-1; g_toggleButton=37; g_buttonEventTick=0; g=State{}; g_config=m::Config{}; g_base=p::kImageBase; g_original=&original;
    g.hookAttempted=g.hookInstalled=true;
    g.directionInstalled=true; g_directionOriginal=&nativeDirection;
    g_started=0; g_ready=1; g_liveGate=0; g_log=nullptr;
    std::wmemset(g_gameRoot,0,MAX_PATH);
    addCar(0,car,input);
    driveMemory(2,0);
    directionMemory();
    put(p::kImageBase+session::kFlowRva,std::uint32_t{6});
    put(p::kImageBase+session::kSimFlagsRva,std::uint32_t{0});
    put(p::kImageBase+p::kVehicleListRva+8,p::VehicleEntry{});
    onPoll(); assert(g.player.input==input);
    heartbeat();
}
void neutralReady() {
    const DWORD begin=clock;
    for (unsigned dt=0; dt<=500; dt+=10) { clock=begin+dt; publish(); }
    assert(g.neutral.ready(clock));
    heartbeat();
}
void press() { heartbeat(); key=true; onPoll(); key=false; onPoll(); }
void arm() { neutralReady(); press(); assert(g.armed); }
void writeLive() {
    publish(0,-16384,225); heartbeat(); getControlsHook(ptr(input),nullptr);
    assert(g.armed);
}
void assertZeros() {
    assert(get<float>(controls+0x14)==0);
    assert(get<float>(controls+0x18)==0);
    assert(get<float>(controls+0x04)==0);
}
bool logged(const char* text) {
    for (const auto& s:messages) if (s.find(text)!=std::string::npos) return true;
    return false;
}
unsigned pedalAttempts() {
    unsigned n=0;
    for (const auto& w:attemptedWrites)
        if (w.first==controls+0x14 || w.first==controls+0x18) ++n;
    return n;
}
void driveMemory(std::uint32_t gear, float speed) {
    const p::Address iv=car+drive::kVehicleInterfaceOffset;
    put(input+drive::kInputVehicleOffset,iv);
    put(iv,p::kImageBase+drive::kVehicleInterfaceVtableRva);
    put(car+drive::kTransmissionOffset,transmission);
    put(transmission,p::kImageBase+drive::kTransmissionVtableRva);
    put(p::kImageBase+drive::kTransmissionVtableRva+drive::kGetGearSlot*4,
        p::kImageBase+drive::kGetGearRva);
    put(p::kImageBase+drive::kVehicleInterfaceVtableRva+drive::kGetSpeedSlot*4,
        p::kImageBase+drive::kGetSpeedRva);
    put(p::kImageBase+drive::kGetGearRva,drive::kGetGearCode);
    put(p::kImageBase+drive::kGetSpeedRva,drive::kGetSpeedCode);
    put(transmission+drive::kGearOffset,gear);
    put(iv+drive::kSpeedOffset,speed);
}
void nativeDirection(void* owner,float gas,float brake) {
    ++nativeCalls; nativeGasArg=gas; nativeBrakeArg=brake;
    if (duringNative) {
        const auto action=duringNative; duringNative=nullptr; action();
    }
    if (reenterGetter) getControlsHook(ptr(input),nullptr);
    if (!emulateDirection || address(owner)!=input-direction::kOwnerOffset) return;
    // A HOST MODEL of the statically verified native helper, not execution of
    // the game, transmission implementation or physics.
    auto gear=get<std::uint32_t>(transmission+drive::kGearOffset);
    const float speed=get<float>(car+drive::kVehicleInterfaceOffset+drive::kSpeedOffset);
    if (gear!=0 && speed<2.5f && brake>0 && gas==0) gear=0;
    else if (gear==0 && speed>-5.0f && (brake==0 || gas>0)) gear=2;
    put(transmission+drive::kGearOffset,gear);
    if (gear==0) {
        put(controls+p::kGasOffset,brake);
        put(controls+p::kBrakeOffset,gas);
    }
}
void fetch(float nativeGas=0,float nativeBrake=0) {
    // Simulate the game's initial setters that precede its helper call.
    put(controls+p::kGasOffset,nativeGas);
    put(controls+p::kBrakeOffset,nativeBrake);
    directionHook(ptr(input-direction::kOwnerOffset),nullptr,nativeGas,nativeBrake);
}
}
extern "C" {
int nfsmw__mh_ensure_init() { return qa::mhInitOk ? 1:0; }
MH_STATUS MH_CreateHook(void* target,void*,void** original) {
    ++qa::mhCreates;
    if (qa::mhCreateStatus==MH_OK)
        *original=qa::mhNullTrampoline ? nullptr:(target==ptr(p::kImageBase+hud::kTickRva) ? reinterpret_cast<void*>(&qa::nativeHud):target==ptr(p::kImageBase+menu::kReaderRva) ? reinterpret_cast<void*>(&qa::nativeMenu):reinterpret_cast<void*>(&qa::nativeDirection));
    return qa::mhCreateStatus;
}
MH_STATUS MH_EnableHook(void*) {
    ++qa::mhEnables;
    assert(g_directionOriginal==&qa::nativeDirection); // Published before enable.
    if(g_hudOriginal)assert(g_hudOriginal==&qa::nativeHud);
    if (qa::duringEnable) qa::duringEnable();
    return qa::mhEnableStatus;
}
MH_STATUS MH_RemoveHook(void*) { ++qa::mhRemoves; return MH_OK; }
void AcquireSRWLockExclusive(SRWLOCK* p) { (p==&g_hudLock?qa::hudLock:qa::lock).lock(); }
void ReleaseSRWLockExclusive(SRWLOCK* p) { (p==&g_hudLock?qa::hudLock:qa::lock).unlock(); }
HANDLE GetCurrentProcess() { return reinterpret_cast<void*>(1); }
BOOL ReadProcessMemory(HANDLE,const void* src,void* out,SIZE_T count,SIZE_T* done) {
    const auto a=reinterpret_cast<std::uintptr_t>(src);
    auto* b=static_cast<unsigned char*>(out); *done=0;
    for (SIZE_T i=0; i<count; ++i) {
        auto it=qa::memory.find(a+i);
        if (it==qa::memory.end()) return 0;
        b[i]=it->second;
    }
    *done=count;
    if (a==qa::controls && count==sizeof(p::InputControls)) {
        qa::clock+=qa::advanceOnControlsRead; qa::advanceOnControlsRead=0;
    }
    return 1;
}
BOOL WriteProcessMemory(HANDLE,void* dst,const void* src,SIZE_T count,SIZE_T* done) {
    const auto a=reinterpret_cast<std::uintptr_t>(dst); *done=0;
    qa::attemptedWrites.emplace_back(static_cast<p::Address>(a),count);
    if (qa::failWrites || (qa::failPedals && count==8) ||
        (a==qa::failFloatAddress && count==4)) return 0;
    for (SIZE_T i=0; i<count; ++i) if (qa::memory.find(a+i)==qa::memory.end()) return 0;
    const auto* b=static_cast<const unsigned char*>(src);
    const bool partial=qa::partialPedalsOnce && count==8;
    const SIZE_T n=partial ? 4:count;
    for (SIZE_T i=0; i<n; ++i) qa::memory[a+i]=b[i];
    *done=n; ++qa::memoryWrites;
    if (partial) { qa::partialPedalsOnce=false; return 0; }
    if (a==qa::controls+4 && count==4) {
        qa::clock+=qa::advanceOnSteerWrite; qa::advanceOnSteerWrite=0;
    }
    return 1;
}
DWORD GetTickCount() { return qa::clock; }
void OutputDebugStringA(const char* s) { qa::messages.emplace_back(s); }
HWND GetForegroundWindow() { return qa::focused ? reinterpret_cast<void*>(2):nullptr; }
DWORD GetWindowThreadProcessId(HWND,DWORD* pid) { *pid=10; return 20; }
DWORD GetCurrentProcessId() { return 10; }
BOOL VirtualProtect(void*,SIZE_T,DWORD,DWORD* old) { *old=0x20; return 1; }
DWORD GetLastError() { return 126; }
void* InterlockedCompareExchangePointer(void* volatile* where,void* desired,void* expected) {
    const auto a=address(const_cast<void**>(where));
    p::Address old=0; assert(readValue(a,old));
    if (old==address(expected)) qa::put(a,address(desired));
    return ptr(old);
}
LONG InterlockedCompareExchange(volatile LONG* a,LONG desired,LONG expected) {
    if (a==&g_liveGate && qa::onFinalGate) {
        const auto action=qa::onFinalGate; qa::onFinalGate=nullptr; action();
    }
    LONG old=*a; if (old==expected) *a=desired; return old;
}
LONG InterlockedExchange(volatile LONG* a,LONG desired) { LONG old=*a; *a=desired; return old; }
short GetAsyncKeyState(int k) {
    if (k==VK_UP) return qa::upKey ? static_cast<short>(0x8000):0;
    if (k==VK_DOWN) return qa::downKey ? static_cast<short>(0x8000):0;
    const bool down=k==VK_F8 ? qa::key:(k==VK_ESCAPE ? qa::esc:(k==VK_PAUSE && qa::pause));
    return down ? static_cast<short>(0x8000):0;
}
DWORD GetModuleFileNameW(HMODULE,wchar_t* dst,DWORD cap) {
    const wchar_t path[]=L"D:\\Need for Speed Most Wanted\\speed.exe";
    if (cap<=std::wcslen(path)) return cap;
    std::wcscpy(dst,path); return std::wcslen(path);
}
UINT GetPrivateProfileIntW(const wchar_t*,const wchar_t* key,int def,const wchar_t*) {
    if (std::wcscmp(key,L"Enabled")==0) return qa::cfgUdp;
    if (std::wcscmp(key,L"Port")==0) return qa::cfgPort;
    if (std::wcscmp(key,L"WheelRangeDegrees")==0) return qa::cfgRange;
    if (std::wcscmp(key,L"InvertSteering")==0) return qa::cfgInvert;
    if (std::wcscmp(key,L"EnableSteering")==0) return qa::cfgSteer;
    if (std::wcscmp(key,L"MenuNavigation")==0) return qa::cfgMenu;
    if (std::wcscmp(key,L"ToggleButton")==0) return qa::cfgButton;
    if (std::wcscmp(key,L"Diagnostics")==0) return qa::cfgDiagnostics;
    return static_cast<UINT>(def);
}
HMODULE GetModuleHandleA(const char*) { return reinterpret_cast<void*>(p::kImageBase); }
HMODULE LoadLibraryExW(const wchar_t*,HANDLE,DWORD) { return nullptr; }
FARPROC GetProcAddress(HMODULE,const char*) { return nullptr; }
HANDLE CreateThread(void*,SIZE_T,LPTHREAD_START_ROUTINE,void*,DWORD,DWORD*) {
    return reinterpret_cast<void*>(3);
}
BOOL CloseHandle(HANDLE) { return 1; }
void Sleep(DWORD) {}
}

int main() {
    unsigned tests=0;
    qa::reset();
    const auto original=qa::get<p::InputControls>(qa::controls);
    void* returned=getControlsHook(ptr(qa::input),nullptr);
    assert(address(returned)==qa::controls && qa::memoryWrites==0);
    qa::neutralReady(); getControlsHook(ptr(qa::input),nullptr);
    assert(!g.armed && qa::memoryWrites==0);
    ++tests;

    qa::reset(); qa::press();
    assert(!g.armed && qa::logged("TOGGLE IGNORED") && qa::memoryWrites==0); ++tests;
    qa::reset(); qa::neutralReady(); qa::publish(0); qa::press();
    assert(!g.armed && !g.neutral.ready(qa::clock)); ++tests;
    qa::reset(); qa::neutralReady(); qa::publish(-32768,-32768,45); qa::press();
    assert(!g.armed); ++tests;

    qa::reset(); qa::arm(); qa::writeLive();
    auto after=qa::get<p::InputControls>(qa::controls);
    auto expected=original;
    const auto command=m::axes(g.sdkSample,g_config);
    expected.gas=command.gas; expected.brake=command.brake; expected.steering=0.5f;
    assert(std::memcmp(&after,&expected,sizeof(after))==0 && g.writes==1);
    assert(after.gas>0.49f && after.gas<0.5f && after.brake>0.24f && after.brake<0.25f);
    for (unsigned i=0; i<100; ++i) getControlsHook(ptr(qa::input),nullptr);
    after=qa::get<p::InputControls>(qa::controls);
    assert(g.writes==101 && std::memcmp(&after,&expected,sizeof(after))==0); ++tests;

    qa::press(); assert(!g.armed && !g.cleanupPending); qa::assertZeros();
    qa::put(qa::controls+0x14,1.0f);
    const auto writes=qa::memoryWrites; getControlsHook(ptr(qa::input),nullptr);
    assert(qa::memoryWrites==writes && qa::get<float>(qa::controls+0x14)==1.0f); ++tests;
    qa::reset(); qa::arm(); qa::writeLive(); qa::focused=false; watchdogTick();
    assert(!g.armed); qa::assertZeros(); ++tests;
    qa::reset(); qa::arm(); qa::writeLive(); qa::clock+=150; watchdogTick();
    assert(!g.armed && qa::logged("older than 150ms")); qa::assertZeros(); ++tests;
    qa::reset(); qa::arm(); qa::writeLive();
    auto bad=qa::raw(); bad.sdkError=2;
    const auto beforeInvalidPublish=qa::memoryWrites;
    publishMoza(MW05_MOZA_SDK_ERROR,bad,qa::clock);
    assert(g.armed && qa::memoryWrites==beforeInvalidPublish); // Reader is publish-only.
    watchdogTick();
    assert(!g.armed); qa::assertZeros(); ++tests;
    qa::reset(); qa::arm(); qa::writeLive();
    const auto beforeCachedCleanup=qa::memoryWrites;
    qa::addCar(0,0x04100000,0x04120000); qa::focused=false;
    getControlsHook(ptr(qa::input),nullptr); // Cached identity has not had an onPoll update.
    assert(!g.armed && g.player.input==0x04120000 &&
           qa::memoryWrites==beforeCachedCleanup); ++tests;

    qa::reset(); qa::arm(); qa::writeLive(); qa::wrongGetter=true;
    const auto beforeWrong=qa::memoryWrites;
    getControlsHook(ptr(qa::input),nullptr);
    assert(!g.armed && qa::memoryWrites==beforeWrong && !g.cleanupPending); ++tests;
    qa::reset(); qa::arm(); qa::writeLive();
    const auto beforePlayer=qa::memoryWrites;
    qa::addCar(0,0x04100000,0x04120000); watchdogTick();
    assert(!g.armed && g.player.input==0x04120000 && qa::memoryWrites==beforePlayer); ++tests;
    qa::reset(); qa::arm(); qa::writeLive();
    const auto beforeAmbig=qa::memoryWrites;
    qa::addCar(1,0x04100000,0x04120000);
    qa::put(p::kImageBase+p::kVehicleListRva+16,p::VehicleEntry{});
    onPoll();
    assert(!g.armed && g.player.status==p::ResolveStatus::Ambiguous &&
           qa::memoryWrites==beforeAmbig); ++tests;
    qa::reset(); qa::arm(); qa::writeLive();
    const auto beforeGone=qa::memoryWrites;
    qa::put(p::kImageBase+p::kVehicleListRva,p::VehicleEntry{}); onPoll();
    assert(!g.armed && qa::memoryWrites==beforeGone); ++tests;

    qa::reset(); qa::arm(); qa::publish(0,-16384,225);
    qa::failFloatAddress=qa::controls+4;
    getControlsHook(ptr(qa::input),nullptr);
    assert(!g.armed && g.writeErrors==1 && g.writes==0 &&
           qa::get<float>(qa::controls+0x14)==0); ++tests;
    qa::reset(); qa::arm(); qa::publish(0,-16384,225); qa::failPedals=true;
    getControlsHook(ptr(qa::input),nullptr);
    assert(!g.armed && g.writeErrors==1 && g.writes==0 &&
           qa::get<float>(qa::controls+4)==0); ++tests;
    qa::reset(); qa::arm(); qa::writeLive(); qa::publish(16000,16000,-90);
    qa::failPedals=true; getControlsHook(ptr(qa::input),nullptr);
    assert(!g.armed && g.writeErrors==1); qa::assertZeros(); ++tests;
    qa::reset(); qa::arm(); qa::writeLive(); qa::publish(16000,16000,-90);
    qa::partialPedalsOnce=true; getControlsHook(ptr(qa::input),nullptr);
    assert(!g.armed && g.writeErrors==1); qa::assertZeros(); ++tests;

    qa::reset(); qa::arm(); qa::writeLive();
    qa::failFloatAddress=qa::controls+0x14; qa::press();
    assert(!g.armed && g.cleanupPending);
    qa::press(); assert(!g.armed && g.cleanupPending);
    qa::failFloatAddress=0; watchdogTick();
    assert(!g.cleanupPending); qa::assertZeros();
    qa::press(); assert(!g.armed); // Must first observe neutral again.
    ++tests;
    qa::reset(); qa::arm(); qa::writeLive();
    qa::put(qa::controls+0x14,0.8f); qa::put(qa::controls+0x18,0.9f);
    qa::put(qa::controls+4,-0.9f); qa::press();
    assert(!g.armed && qa::get<float>(qa::controls+0x14)==0.8f &&
           qa::get<float>(qa::controls+0x18)==0.9f && qa::get<float>(qa::controls+4)==-0.9f);
    ++tests;

    qa::reset(); g_config.enableSteering=false; qa::arm(); qa::writeLive();
    assert(qa::get<float>(qa::controls+4)==-0.2f && !g.steerOwned.attempted);
    qa::press(); assert(qa::get<float>(qa::controls+4)==-0.2f); ++tests;
    qa::reset(); g_config.invertSteering=true; qa::arm(); qa::writeLive();
    assert(qa::get<float>(qa::controls+4)==-0.5f); ++tests;
    qa::reset(); qa::arm(); qa::clock=g.testStarted+59999;
    qa::publish(0); qa::heartbeat(); getControlsHook(ptr(qa::input),nullptr); assert(g.armed);
    ++qa::clock; watchdogTick();
    assert(g.armed);
    for (DWORD elapsed : {60001u, 300000u, 3600000u, 86400000u}) {
        qa::clock=g.testStarted+elapsed; qa::publish(0); qa::heartbeat();
        watchdogTick(); getControlsHook(ptr(qa::input),nullptr); assert(g.armed);
    }
    qa::press(); assert(!g.armed); qa::assertZeros(); ++tests;
    qa::reset(); qa::arm(); qa::writeLive();
    qa::advanceOnControlsRead=200;
    const auto beforeDelayed=g.writes; getControlsHook(ptr(qa::input),nullptr);
    assert(!g.armed && g.writes==beforeDelayed); qa::assertZeros(); ++tests;
    qa::reset(); qa::arm(); qa::writeLive();
    qa::clock=g.testStarted+59950; qa::publish(0,-16384,225); qa::heartbeat();
    qa::advanceOnControlsRead=75;
    const auto beforeDeadlineRead=g.writes; getControlsHook(ptr(qa::input),nullptr);
    assert(g.armed && g.writes==beforeDeadlineRead+1 && g.pendingStop==StopReason::None);
    qa::press(); qa::assertZeros(); ++tests;
    qa::reset(); qa::arm(); qa::publish(0,-16384,225);
    qa::advanceOnSteerWrite=200; getControlsHook(ptr(qa::input),nullptr);
    assert(!g.armed && g.writes==0 && qa::get<float>(qa::controls+0x14)==0 &&
           qa::get<float>(qa::controls+0x18)==1); ++tests;
    qa::reset(); qa::arm(); qa::clock=g.testStarted+59950;
    qa::publish(0,-16384,225); qa::heartbeat(); qa::advanceOnSteerWrite=75;
    getControlsHook(ptr(qa::input),nullptr);
    assert(g.armed && g.writes==1 && g.pendingStop==StopReason::None &&
           qa::get<float>(qa::controls+0x14)>0 && qa::get<float>(qa::controls+0x18)>0);
    ++tests;
    qa::reset(); qa::arm(); qa::writeLive();
    qa::memory.erase(qa::controls+0x23); // Full controls read fails, axis cells still readable.
    getControlsHook(ptr(qa::input),nullptr);
    assert(!g.armed && g.writeErrors==1); qa::assertZeros(); ++tests;

    qa::reset(); qa::arm(); qa::writeLive(); qa::esc=true; watchdogTick();
    assert(!g.armed); qa::assertZeros();
    qa::esc=false; qa::press(); assert(!g.requested);
    qa::neutralReady(); qa::press(); assert(g.armed);
    qa::publish(0); getControlsHook(ptr(qa::input),nullptr);
    qa::pause=true; watchdogTick(); assert(!g.armed); ++tests;
    qa::reset(); qa::neutralReady(); qa::key=true; onPoll(); assert(g.armed);
    onPoll(); assert(g.armed);
    qa::key=false; onPoll(); qa::key=true; onPoll(); assert(!g.armed); ++tests;
    qa::reset(); qa::arm(); qa::writeLive(); qa::clock+=150; watchdogTick();
    qa::publish(); qa::press(); assert(!g.armed);
    qa::neutralReady(); qa::press(); assert(g.armed); ++tests;
    qa::reset(); qa::arm(); qa::writeLive();
    bad=qa::raw(); bad.angleDegrees=std::numeric_limits<float>::quiet_NaN();
    publishMoza(0,bad,qa::clock);
    watchdogTick();
    assert(!g.armed); qa::assertZeros(); ++tests;
    qa::reset(); qa::arm(); qa::writeLive();
    bad=qa::raw(); bad.bytes=24; publishMoza(0,bad,qa::clock);
    watchdogTick();
    assert(!g.armed); qa::assertZeros(); ++tests;

    // Two minutes of modelled callbacks, fresh samples and watchdog checks.
    // This is NOT a Windows thread/physics/performance test.
    qa::reset(); qa::arm();
    for (unsigned i=0; i<6001; ++i) {
        qa::clock+=20;
        qa::publish((i/100)%2 ? 0 : -32768);
        qa::heartbeat(); watchdogTick();
        getControlsHook(ptr(qa::input),nullptr); onPoll();
        assert(g.armed && g.writeErrors==0);
    }
    qa::press(); assert(!g.armed && !g.cleanupPending); qa::assertZeros(); ++tests;

    // Release logging defaults and diagnostics have NO input-path effects.
    qa::reset(); assert(openLogAndConfig() && g_reportIntervalMs==5000);
    qa::messages.clear(); g.reportAt=qa::clock;
    qa::clock+=4999; onPoll(); assert(!qa::logged("STAT "));
    ++qa::clock; onPoll(); assert(qa::logged("STAT ")); ++tests;
    qa::reset(); qa::cfgDiagnostics=1;
    assert(openLogAndConfig() && g_reportIntervalMs==200);
    qa::messages.clear(); g.reportAt=qa::clock;
    qa::clock+=199; onPoll(); assert(!qa::logged("STAT "));
    ++qa::clock; onPoll(); assert(qa::logged("STAT ")); ++tests;
    qa::reset(); qa::cfgDiagnostics=2; assert(!openLogAndConfig());
    qa::cfgDiagnostics=-1; assert(!openLogAndConfig()); ++tests;
    qa::reset(); qa::clock=0xFFFFFF00u; qa::arm();
    qa::clock+=70000u; qa::publish(0); qa::heartbeat(); watchdogTick();
    getControlsHook(ptr(qa::input),nullptr); assert(g.armed);
    qa::clock+=150; watchdogTick(); assert(!g.armed); qa::assertZeros(); ++tests;
    qa::reset(); qa::cfgDiagnostics=1; assert(openLogAndConfig());
    qa::arm(); qa::writeLive(); qa::clock+=150; watchdogTick();
    assert(!g.armed); qa::assertZeros(); ++tests;

    qa::reset(); assert(openLogAndConfig());
    assert(g_config.wheelRangeDegrees==900);
    wchar_t path[MAX_PATH]{}; assert(rootPath(L"x.dll",path));
    assert(std::wcscmp(path,L"D:\\Need for Speed Most Wanted\\x.dll")==0); ++tests;
    qa::cfgRange=90; assert(!openLogAndConfig());
    qa::cfgRange=900; qa::cfgInvert=2; assert(!openLogAndConfig());
    qa::cfgInvert=0; qa::cfgSteer=2; assert(!openLogAndConfig()); ++tests;
    qa::reset(); assert(openLogAndConfig());
    assert(mozaThread(nullptr)==0 && qa::logged("LoadLibraryExW") && !g.armed &&
           qa::memoryWrites==0); ++tests;

    // v0.6.1 neutral handoff: never overwrite the native idle brake with zero.
    qa::reset(); qa::arm();
    const auto idleBefore=qa::get<p::InputControls>(qa::controls);
    for (unsigned i=0; i<20; ++i) getControlsHook(ptr(qa::input),nullptr);
    auto idleExpected=idleBefore; idleExpected.steering=0;
    const auto idleAfter=qa::get<p::InputControls>(qa::controls);
    assert(std::memcmp(&idleAfter,&idleExpected,sizeof(idleAfter))==0);
    assert(g.pedalMode==m::PedalMode::NativeNeutral && g.pedalWrites==0 &&
           g.pedalHandoffs==0 && qa::pedalAttempts()==0);
    ++tests;
    // Native brake is not assumed to be 1: preserve current rolling/coasting state.
    qa::reset(); qa::arm(); qa::put(qa::controls+0x18,0.0f);
    getControlsHook(ptr(qa::input),nullptr);
    assert(qa::get<float>(qa::controls+0x18)==0 && qa::pedalAttempts()==0); ++tests;
    qa::reset(); qa::arm();
    qa::put(qa::controls+0x14,0.3f); qa::put(qa::controls+0x18,0.7f);
    getControlsHook(ptr(qa::input),nullptr);
    assert(qa::get<float>(qa::controls+0x14)==0.3f &&
           qa::get<float>(qa::controls+0x18)==0.7f && qa::pedalAttempts()==0); ++tests;
    // Steering is still direct in native pedal neutral.
    qa::reset(); qa::arm(); qa::publish(-32768,-32768,225);
    getControlsHook(ptr(qa::input),nullptr);
    assert(qa::get<float>(qa::controls+4)==0.5f &&
           qa::get<float>(qa::controls+0x18)==1 && qa::pedalAttempts()==0); ++tests;
    // No steering override + neutral means no game-memory writes at all.
    qa::reset(); g_config.enableSteering=false; qa::arm();
    for (unsigned i=0; i<10; ++i) getControlsHook(ptr(qa::input),nullptr);
    assert(qa::memoryWrites==0 && g.writes==0 && g.lastWrittenInput==0 &&
           g.pedalMode==m::PedalMode::NativeNeutral); ++tests;
    // Gas lifts native idle braking and stays analog, with unchanged mapping.
    qa::reset(); qa::arm(); getControlsHook(ptr(qa::input),nullptr);
    qa::publish(0); getControlsHook(ptr(qa::input),nullptr);
    assert(g.pedalMode==m::PedalMode::Moza && g.pedalWrites==1 &&
           qa::get<float>(qa::controls+0x14)>0.49f &&
           qa::get<float>(qa::controls+0x18)==0); ++tests;
    // Release old gas ONCE, then never suppress a later native idle brake.
    qa::publish(); getControlsHook(ptr(qa::input),nullptr);
    assert(g.pedalMode==m::PedalMode::NativeNeutral && g.pedalHandoffs==1 &&
           !g.gasOwned.attempted && !g.brakeOwned.attempted);
    assert(qa::get<float>(qa::controls+0x14)==0 && qa::get<float>(qa::controls+0x18)==0);
    const auto releaseAttempts=qa::pedalAttempts();
    qa::put(qa::controls+0x18,1.0f); // Emulated next native update, not done by the mod.
    for (unsigned i=0; i<20; ++i) getControlsHook(ptr(qa::input),nullptr);
    assert(qa::get<float>(qa::controls+0x18)==1 &&
           qa::pedalAttempts()==releaseAttempts && g.pedalHandoffs==1);
    qa::press(); assert(!g.armed && qa::get<float>(qa::controls+0x18)==1);
    ++tests;
    // Brake release is also a one-time handoff; do not replay the old idle 1.
    qa::reset(); qa::arm(); qa::publish(-32768,0);
    getControlsHook(ptr(qa::input),nullptr); assert(qa::get<float>(qa::controls+0x18)>0.49f);
    qa::publish(); getControlsHook(ptr(qa::input),nullptr);
    assert(qa::get<float>(qa::controls+0x18)==0 && g.pedalHandoffs==1);
    const auto brakeReleaseAttempts=qa::pedalAttempts();
    for (unsigned i=0; i<20; ++i) getControlsHook(ptr(qa::input),nullptr);
    assert(qa::get<float>(qa::controls+0x18)==0 &&
           qa::pedalAttempts()==brakeReleaseAttempts); ++tests;
    // Preserve a newer native value even during the handoff.
    qa::reset(); qa::arm(); qa::writeLive();
    qa::put(qa::controls+0x14,0.8f); qa::put(qa::controls+0x18,0.9f);
    qa::publish(); getControlsHook(ptr(qa::input),nullptr);
    assert(g.pedalMode==m::PedalMode::NativeNeutral &&
           qa::get<float>(qa::controls+0x14)==0.8f &&
           qa::get<float>(qa::controls+0x18)==0.9f); ++tests;
    // Multiple transitions are idempotent, not an accumulating brake or throttle.
    qa::reset(); qa::arm();
    for (unsigned i=0; i<4; ++i) {
        qa::publish(0); getControlsHook(ptr(qa::input),nullptr);
        qa::publish(); getControlsHook(ptr(qa::input),nullptr);
        getControlsHook(ptr(qa::input),nullptr);
        assert(g.pedalHandoffs==i+1 && g.pedalWrites==i+1);
    }
    assert(qa::get<float>(qa::controls+0x14)==0); ++tests;
    // The observed released-pedal noise remains inside the EXISTING 1% zone.
    qa::reset(); qa::arm(); qa::publish(-32309);
    getControlsHook(ptr(qa::input),nullptr);
    assert(g.pedalMode==m::PedalMode::NativeNeutral && qa::pedalAttempts()==0); ++tests;
    qa::reset(); qa::arm(); qa::publish(-32112);
    getControlsHook(ptr(qa::input),nullptr);
    assert(g.pedalMode==m::PedalMode::Moza && qa::get<float>(qa::controls+0x14)>0);
    ++tests;
    // Failed handoff stops and retains retry state; never keep driving.
    qa::reset(); qa::arm(); qa::writeLive(); qa::publish();
    qa::failFloatAddress=qa::controls+0x14; getControlsHook(ptr(qa::input),nullptr);
    assert(!g.armed && g.pedalMode==m::PedalMode::Off &&
           g.writeErrors==1 && g.cleanupPending);
    qa::failFloatAddress=0; watchdogTick();
    assert(!g.cleanupPending && qa::get<float>(qa::controls+0x14)==0); ++tests;
    qa::reset(); qa::arm(); qa::writeLive(); qa::publish();
    qa::failFloatAddress=qa::controls+0x18; getControlsHook(ptr(qa::input),nullptr);
    assert(!g.armed && g.writeErrors==1 && g.cleanupPending);
    qa::failFloatAddress=0; watchdogTick(); assert(!g.cleanupPending); ++tests;
    // Resolve before handing off: no release or new steer write to an old player.
    qa::reset(); qa::arm(); qa::writeLive(); qa::publish();
    const auto beforeHandoffChange=qa::memoryWrites;
    qa::addCar(0,0x04100000,0x04120000);
    getControlsHook(ptr(qa::input),nullptr);
    assert(!g.armed && g.pendingStop==StopReason::PlayerChanged &&
           g.player.input==0x04120000 && qa::memoryWrites==beforeHandoffChange &&
           !g.cleanupPending); ++tests;
    // Without steering, successful pedal release drops the last owned object.
    qa::reset(); g_config.enableSteering=false; qa::arm(); qa::writeLive();
    qa::publish(); getControlsHook(ptr(qa::input),nullptr);
    assert(g.lastWrittenInput==0 && !g.steerOwned.attempted &&
           g.pedalMode==m::PedalMode::NativeNeutral && g.pedalHandoffs==1);
    qa::put(qa::controls+0x18,1.0f); qa::press();
    assert(!g.cleanupPending && qa::get<float>(qa::controls+0x18)==1); ++tests;
    // New diagnostic fields report successful handoff/native neutrality.
    qa::reset(); qa::arm(); getControlsHook(ptr(qa::input),nullptr);
    qa::clock+=5000; qa::publish(); qa::heartbeat(); onPoll();
    assert(qa::logged("pedal_mode=1") && qa::logged("pedal_writes=0") &&
           qa::logged("pedal_handoffs_total=0"));
    ++tests;
    // v0.6.2 DIAG: valid native telemetry never writes or changes controls.
    qa::reset(); qa::driveMemory(0,-3.5f); qa::downKey=true;
    const auto beforeDiagnostic=qa::get<p::InputControls>(qa::controls);
    qa::clock+=5000; onPoll();
    const auto afterDiagnostic=qa::get<p::InputControls>(qa::controls);
    assert(!g.armed && qa::memoryWrites==0);
    assert(std::memcmp(&beforeDiagnostic,&afterDiagnostic,sizeof(beforeDiagnostic))==0);
    assert(qa::logged("drive_status=1 gear_id=0 speed_game=-3.50000 key_up=0 key_down=1"));
    ++tests;
    // v0.6.3 must NOT guess pedal mapping if the actual gear cannot be read.
    qa::reset(); qa::arm(); qa::writeLive();
    qa::memory.erase(qa::transmission+drive::kGearOffset);
    getControlsHook(ptr(qa::input),nullptr);
    assert(!g.armed && g_liveGate==0 && g.pendingStop==StopReason::DriveInvalid);
    ++tests;
    // Actual R swaps the two engine pedal fields; forward mapping is unchanged.
    qa::reset(); qa::driveMemory(0,-1.5f); qa::arm(); qa::writeLive();
    const auto expectedDiagnostic=m::axes(g.sdkSample,g_config);
    assert(qa::get<float>(qa::controls+0x14)==expectedDiagnostic.brake);
    assert(qa::get<float>(qa::controls+0x18)==expectedDiagnostic.gas);
    qa::upKey=true; qa::clock+=5000; qa::publish(0,-16384,225); qa::heartbeat(); onPoll();
    assert(g.armed);
    assert(qa::logged("drive_status=1 gear_id=0 speed_game=-1.50000 key_up=1 key_down=0"));
    ++tests;

    // OFF: arguments (including signed zero/NaN bits) and native writes pass
    // through. No MOZA request and exactly one original call.
    qa::reset(); qa::emulateDirection=true;
    qa::fetch(0,1);
    assert(qa::nativeCalls==1 && qa::nativeGasArg==0 && qa::nativeBrakeArg==1);
    assert(g.directionMozaCalls==0 && qa::memoryWrites==0 && !g.armed);
    assert(qa::get<std::uint32_t>(qa::transmission+drive::kGearOffset)==0);
    ++tests;
    qa::reset(); float payload=0;
    const std::uint32_t nanBits=0x7FC01234;
    std::memcpy(&payload,&nanBits,4);
    directionHook(ptr(qa::input-direction::kOwnerOffset),nullptr,-0.0f,payload);
    std::uint32_t actualNan=0,actualZero=0;
    std::memcpy(&actualNan,&qa::nativeBrakeArg,4);
    std::memcpy(&actualZero,&qa::nativeGasArg,4);
    assert(actualNan==nanBits && actualZero==0x80000000u && qa::nativeCalls==1);
    ++tests;
    // Startup and unrelated objects always chain untouched.
    qa::reset(); g_ready=0;
    directionHook(ptr(qa::input-direction::kOwnerOffset),nullptr,0.2f,0.7f);
    assert(qa::nativeCalls==1 && qa::nativeGasArg==0.2f && qa::nativeBrakeArg==0.7f);
    assert(qa::memoryWrites==0 && g.directionCalls==0); ++tests;
    qa::reset(); qa::arm(); qa::publish(0,-16000);
    directionHook(ptr(0x04000000),nullptr,0.2f,0.7f);
    assert(qa::nativeCalls==1 && qa::nativeGasArg==0.2f && qa::nativeBrakeArg==0.7f);
    assert(g.directionMozaCalls==0 && qa::memoryWrites==0); ++tests;
    // No new game-function call is synthesized by the getter or onPoll.
    qa::reset(); qa::arm(); qa::writeLive(); onPoll();
    assert(qa::nativeCalls==0); ++tests;
    // Pressed brake at rest: native model selects R, final getter keeps
    // analog reverse throttle. Releasing pedals does not force idle zero/one.
    qa::reset(); qa::arm(); qa::emulateDirection=true; qa::publish(-32768,0);
    const auto logicalReverse=m::axes(g.sdkSample,g_config);
    qa::fetch();
    assert(qa::nativeCalls==1 && qa::nativeGasArg==0 &&
           qa::nativeBrakeArg==logicalReverse.brake);
    assert(qa::get<std::uint32_t>(qa::transmission+drive::kGearOffset)==0);
    getControlsHook(ptr(qa::input),nullptr);
    assert(qa::get<float>(qa::controls+0x14)==logicalReverse.brake);
    assert(qa::get<float>(qa::controls+0x18)==0 && g.reverseMaps==1);
    qa::publish(); qa::fetch(); getControlsHook(ptr(qa::input),nullptr);
    const auto attemptsAfterRelease=qa::pedalAttempts();
    qa::put(qa::controls+0x18,0.43f);
    for (unsigned i=0;i<10;++i) getControlsHook(ptr(qa::input),nullptr);
    assert(qa::get<float>(qa::controls+0x18)==0.43f &&
           qa::pedalAttempts()==attemptsAfterRelease &&
           g.pedalMode==m::PedalMode::NativeNeutral);
    ++tests;
    // Forward braking above the native threshold does NOT preselect R.
    qa::reset(); qa::driveMemory(3,15); qa::arm();
    qa::emulateDirection=true; qa::publish(-32768,0); qa::fetch();
    getControlsHook(ptr(qa::input),nullptr);
    assert(qa::get<std::uint32_t>(qa::transmission+drive::kGearOffset)==3);
    assert(qa::get<float>(qa::controls+0x14)==0);
    assert(qa::get<float>(qa::controls+0x18)>0.49f && g.reverseMaps==0); ++tests;
    // Fast reverse + gas first brakes in R; when native model selects D,
    // the very next getter uses forward throttle without remembered mapping.
    qa::reset(); qa::driveMemory(0,-10); qa::arm();
    qa::emulateDirection=true; qa::publish(0); qa::fetch();
    getControlsHook(ptr(qa::input),nullptr);
    assert(qa::get<std::uint32_t>(qa::transmission+drive::kGearOffset)==0);
    assert(qa::get<float>(qa::controls+0x14)==0 &&
           qa::get<float>(qa::controls+0x18)>0.49f);
    qa::driveMemory(0,-1); qa::fetch(); getControlsHook(ptr(qa::input),nullptr);
    assert(qa::get<std::uint32_t>(qa::transmission+drive::kGearOffset)==2);
    assert(qa::get<float>(qa::controls+0x14)>0.49f &&
           qa::get<float>(qa::controls+0x18)==0); ++tests;
    // Both pedals stay analog and use ACTUAL gear, not speed sign.
    qa::reset(); qa::driveMemory(2,-0.5f); qa::arm(); qa::publish(0,-16384);
    qa::emulateDirection=true; qa::fetch(); getControlsHook(ptr(qa::input),nullptr);
    auto both=m::axes(g.sdkSample,g_config);
    assert(qa::get<std::uint32_t>(qa::transmission+drive::kGearOffset)==2);
    assert(qa::get<float>(qa::controls+0x14)==both.gas &&
           qa::get<float>(qa::controls+0x18)==both.brake); ++tests;
    // Original helper can re-enter our getter: no lock recursion or override
    // midway through native direction/gear calculations.
    qa::reset(); qa::arm(); qa::publish(-32768,0);
    qa::emulateDirection=qa::reenterGetter=true; qa::fetch();
    assert(qa::nativeCalls==1 && qa::memoryWrites==0 && !g.directionInFlight);
    assert(g.armed && g.directionMozaCalls==1);
    getControlsHook(ptr(qa::input),nullptr);
    assert(g.pedalWrites==1); ++tests;
    // Cancellation DURING native call: watchdog does not deadlock and does
    // not clean too early. Late helper writes are cleaned after its return.
    qa::reset(); qa::arm(); qa::publish(-32768,0);
    qa::emulateDirection=true;
    qa::duringNative=[] {
        assert(g.directionInFlight);
        qa::focused=false; watchdogTick();
        assert(!g.armed && g.cleanupPending && qa::memoryWrites==0);
    };
    qa::fetch();
    assert(!g.armed && g_liveGate==0 && !g.cleanupPending && !g.directionInFlight);
    assert(qa::get<float>(qa::controls+0x14)==0 &&
           qa::get<float>(qa::controls+0x18)==0); ++tests;
    // User cancellation BEFORE the original call uses the untouched native
    // arguments, rolls back staged ownership, and makes no MOZA native call.
    qa::reset(); qa::arm(); qa::publish(-32768,0);
    qa::onFinalGate=[] { qa::focused=false; watchdogTick(); };
    qa::fetch(0.12f,0.34f);
    assert(qa::nativeCalls==1 && qa::nativeGasArg==0.12f && qa::nativeBrakeArg==0.34f);
    assert(g.directionMozaCalls==0 && !g.armed && !g.directionInFlight);
    assert(qa::get<float>(qa::controls+0x14)==0.12f &&
           qa::get<float>(qa::controls+0x18)==0.34f); ++tests;
    // A sample that becomes stale in the final call window is not forwarded.
    qa::reset(); qa::arm(); qa::publish(-32768,0);
    qa::onFinalGate=[] { qa::clock+=150; };
    qa::fetch(0.15f,0.35f);
    assert(qa::nativeGasArg==0.15f && qa::nativeBrakeArg==0.35f &&
           !g.armed && g.directionMozaCalls==0); ++tests;
    // Missing fresh helper observations are a NEW fail-closed requirement.
    qa::reset(); qa::neutralReady(); g.directionSeen=false;
    qa::key=true; onPoll(); assert(!g.armed); ++tests;
    qa::reset(); qa::arm(); qa::clock+=150; qa::publish();
    watchdogTick();
    assert(!g.armed && qa::logged("native direction helper missing/stale")); ++tests;
    // No arming if the drivetrain or owner fingerprint is invalid.
    qa::reset(); qa::neutralReady();
    qa::put(qa::input-direction::kOwnerOffset,std::uint32_t{0});
    qa::press(); assert(!g.armed); ++tests;
    // Runtime fingerprint or owner mismatch cancels; original still chains.
    qa::reset(); qa::arm(); qa::publish(-32768,0);
    qa::put(p::kImageBase+direction::kSetGasRva,std::uint8_t{0x90});
    qa::fetch(0.2f,0.3f);
    assert(!g.armed && qa::nativeCalls==1 &&
           qa::nativeGasArg==0.2f && qa::nativeBrakeArg==0.3f); ++tests;
    qa::reset(); qa::arm(); qa::publish(-32768,0);
    qa::put(qa::input-direction::kOwnerOffset,std::uint32_t{0});
    qa::fetch(0.2f,0.3f);
    assert(!g.armed && qa::nativeCalls==1 && g.directionMozaCalls==0); ++tests;
    // Player replacement during native call: never write to the old object.
    qa::reset(); qa::arm(); qa::publish(-32768,0);
    qa::duringNative=[] {
        qa::put(p::kImageBase+p::kVehicleListRva,p::VehicleEntry{});
    };
    qa::fetch();
    assert(!g.armed && qa::memoryWrites==0 && !g.cleanupPending &&
           !g.directionInFlight); ++tests;
    // Unexpected getter layout during native call poisons cleanup as before.
    qa::reset(); qa::arm(); qa::publish(-32768,0); qa::reenterGetter=true;
    qa::wrongGetter=true; qa::fetch();
    assert(!g.armed && qa::memoryWrites==0 && !g.cleanupPending &&
           !g.directionInFlight && g.directionLeasePoisoned); ++tests;
    // Installation is strict, original is published before callback enable,
    // and failure never silently falls back to a split-instruction detour.
    qa::reset(); assert(installDirectionHook());
    assert(qa::mhCreates==1 && qa::mhEnables==1); ++tests;
    qa::reset(); qa::memory.erase(p::kImageBase+direction::kHelperRva);
    assert(!installDirectionHook() && qa::mhCreates==0); ++tests;
    qa::reset(); qa::mhInitOk=false;
    assert(!installDirectionHook() && qa::mhCreates==0); ++tests;
    qa::reset(); qa::mhCreateStatus=4;
    assert(!installDirectionHook() && qa::mhEnables==0 &&
           g_directionOriginal==nullptr); ++tests;
    qa::reset(); qa::mhEnableStatus=5;
    assert(!installDirectionHook() && qa::mhRemoves==1 &&
           g_directionOriginal==nullptr); ++tests;
    qa::reset(); g_ready=0;
    qa::duringEnable=[] {
        directionHook(ptr(qa::input-direction::kOwnerOffset),nullptr,0.42f,0.24f);
    };
    assert(installDirectionHook() && qa::nativeCalls==1 &&
           qa::nativeGasArg==0.42f && qa::nativeBrakeArg==0.24f); ++tests;
    qa::reset(); qa::mhNullTrampoline=true;
    assert(!installDirectionHook() && qa::mhEnables==0 && qa::mhRemoves==1 &&
           g_directionOriginal==nullptr); ++tests;
    // Invalid SDK publication while the original is in flight cancels too;
    // its late setter output must not survive the wrapper.
    qa::reset(); qa::arm(); qa::publish(-32768,0); qa::emulateDirection=true;
    qa::duringNative=[] {
        const auto sample=qa::raw();
        publishMoza(MW05_MOZA_NOT_LOADED,sample,qa::clock);
        watchdogTick();
        assert(!g.armed && g.cleanupPending);
    };
    qa::fetch();
    assert(!g.armed && !g.cleanupPending && !g.directionInFlight &&
           qa::get<float>(qa::controls+0x14)==0 &&
           qa::get<float>(qa::controls+0x18)==0); ++tests;
    // Even fresh SDK data cannot keep a helper lease alive after a slow
    // original game call. No hard-real-time completion guarantee is claimed.
    qa::reset(); qa::arm(); qa::publish(-32768,0); qa::emulateDirection=true;
    qa::duringNative=[] { qa::clock+=151; qa::publish(-32768,0); };
    qa::fetch();
    assert(!g.armed && g.pendingStop==StopReason::DirectionUnavailable &&
           !g.cleanupPending && qa::get<float>(qa::controls+0x14)==0); ++tests;
    // Nested helper callbacks are forwarded, not substituted a second time.
    qa::reset(); qa::arm(); qa::publish(-32768,0);
    qa::duringNative=[] {
        directionHook(ptr(qa::input-direction::kOwnerOffset),nullptr,0.11f,0.22f);
    };
    qa::fetch();
    assert(qa::nativeCalls==2 && g.directionMozaCalls==1 &&
           !g.directionInFlight && g.armed); ++tests;
    // Released pedals make no MOZA native call and preserve the ordinary
    // native key arguments, including native idle handling while in R.
    qa::reset(); qa::driveMemory(0,-1); qa::arm(); qa::publish();
    qa::fetch(0.2f,0.3f);
    assert(qa::nativeGasArg==0.2f && qa::nativeBrakeArg==0.3f &&
           g.directionMozaCalls==0 && qa::memoryWrites==0); ++tests;

    // v0.7.1 lifecycle. These are serialized host callbacks, not real threads.
    auto frames=[](unsigned ms, std::int16_t gas=-32768,
                   std::int16_t brake=-32768, float angle=0) {
        for(unsigned t=0;t<ms;t+=10) {
            qa::clock+=10; qa::publish(gas,brake,angle); qa::heartbeat(); onPoll();
        }
    };
    auto focusSuspend=[] {
        qa::focused=false; watchdogTick();
        assert(g.requested && !g.armed && g_liveGate==0);
    };
    auto block=[](std::uint32_t flags) {
        qa::put(p::kImageBase+session::kSimFlagsRva,flags);
    };
    // v0.7.7: already-armed same-player recovery resumes on the first valid poll.
    qa::reset(); qa::arm(); qa::writeLive(); focusSuspend();
    const auto afterSuspendWrites=qa::memoryWrites;
    frames(1000); assert(g.requested && !g.armed && qa::memoryWrites==afterSuspendWrites);
    qa::focused=true; frames(10,0,-32768,45);
    assert(g.armed && g.requested && qa::logged("neutral_dwell_ms=0 mode=validated-auto-resume")); ++tests;

    // Paused simulation with released Esc is still suspended.
    qa::reset(); qa::arm(); qa::writeLive(); block(2); watchdogTick();
    assert(g.requested && !g.armed); const auto pauseWrites=qa::memoryWrites;
    frames(1200); assert(!g.armed && qa::memoryWrites==pauseWrites);
    block(0); frames(10,0,-32768,45); assert(g.armed); ++tests;

    // Non-race flow blocks even if an old car/getter/helper are all present.
    qa::reset(); qa::arm(); qa::writeLive();
    qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{3}); watchdogTick();
    frames(1000); assert(g.requested && !g.armed);
    qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{6});
    frames(10,0,-32768,45); assert(g.armed); ++tests;

    qa::reset(); qa::arm(); qa::writeLive(); qa::esc=true; watchdogTick();
    frames(1000); assert(g.requested && !g.armed);
    qa::esc=false; block(2); frames(1000); assert(!g.armed);
    block(0); frames(10,0,-32768,45); assert(g.armed); ++tests;

    // F8 during suspension cancels intent, even with pressed pedals.
    qa::reset(); qa::arm(); focusSuspend(); qa::focused=true; qa::publish(32767);
    qa::press(); assert(!g.requested && !g.armed);
    frames(1500); assert(!g.requested && !g.armed); ++tests;
    qa::reset(); qa::arm(); qa::press(); frames(1500);
    assert(!g.requested && !g.armed); ++tests;
    qa::reset(); frames(1500); assert(!g.requested && !g.armed); ++tests;

    // Returning to the SAME player with pressed pedals/off-centre wheel must auto-arm.
    for (unsigned kind=0;kind<3;++kind) {
        qa::reset(); qa::arm(); focusSuspend(); qa::focused=true;
        frames(10,kind==0 ? 0 : -32768,kind==1 ? 0 : -32768,kind==2 ? 45.0f : 0.0f);
        assert(g.requested && g.armed && !g.resumeNeutral.tracking); ++tests;
    }
    // No neutral dwell is started for same-player focus recovery.
    qa::reset(); qa::arm(); focusSuspend(); qa::focused=true;
    frames(10,0,-32768,120); assert(g.armed && !g.resumeNeutral.tracking); ++tests;

    // Lost player and subsequent valid reappearance; no stale-object override.
    qa::reset(); qa::arm(); qa::writeLive();
    qa::put(p::kImageBase+p::kVehicleListRva,p::VehicleEntry{}); watchdogTick();
    assert(g.requested && !g.armed && !g.cleanupPending);
    const auto missingWrites=qa::memoryWrites;
    frames(1000); assert(!g.armed && qa::memoryWrites==missingWrites);
    qa::addCar(0,qa::car,qa::input); onPoll();
    frames(530); assert(g.armed); ++tests;

    // Fresh native helper must be observed for the recovered player.
    qa::reset(); qa::arm(); qa::clock+=150; qa::publish(); watchdogTick();
    assert(g.requested && !g.armed); g.directionSeen=false;
    for(unsigned i=0;i<60;++i) {qa::clock+=10; qa::publish(); onPoll();}
    assert(!g.armed && !g.resumeNeutral.tracking);
    frames(520); assert(g.armed); ++tests;

    // Serious SDK/drive/context faults clear intent, not a hidden auto retry.
    qa::reset(); qa::arm(); focusSuspend(); qa::focused=true;
    bad=qa::raw(); bad.sdkError=3; publishMoza(MW05_MOZA_SDK_ERROR,bad,qa::clock);
    qa::heartbeat(); onPoll(); assert(!g.requested && !g.armed);
    frames(1000); assert(!g.armed); ++tests;
    qa::reset(); qa::arm(); focusSuspend(); qa::focused=true;
    qa::clock+=150; qa::heartbeat(); onPoll(); assert(!g.requested && !g.armed); ++tests;
    qa::reset(); qa::arm(); focusSuspend(); qa::focused=true;
    qa::memory.erase(qa::transmission+drive::kGearOffset);
    qa::publish(); qa::heartbeat(); onPoll(); assert(!g.requested && !g.armed); ++tests;
    qa::reset(); qa::arm(); qa::writeLive();
    qa::memory.erase(p::kImageBase+session::kSimFlagsRva); watchdogTick();
    assert(!g.requested && !g.armed); ++tests;
    qa::reset(); qa::arm(); focusSuspend(); qa::focused=true;
    qa::memory.erase(qa::controls+0x23);
    qa::publish(); qa::heartbeat(); onPoll(); assert(!g.requested && !g.armed); ++tests;

    // F8 works through the watchdog if the game poll stalls in pause.
    qa::reset(); qa::arm(); block(2); watchdogTick();
    qa::key=true; watchdogTick(); assert(!g.requested && !g.armed);
    qa::key=false; watchdogTick(); block(0); frames(1000); assert(!g.armed); ++tests;
    qa::reset(); qa::neutralReady(); block(2); qa::press();
    assert(!g.requested && !g.armed); block(0); frames(1000); assert(!g.armed); ++tests;

    // Slow recovery reads cannot activate stale controls or skip dwell.
    qa::reset(); qa::arm(); focusSuspend(); qa::focused=true;
    qa::publish(); qa::heartbeat(); qa::advanceOnControlsRead=200; onPoll();
    assert(!g.armed && !g.resumeNeutral.tracking); ++tests;

    // Final helper-gate cancellation passes original arguments unchanged.
    qa::reset(); qa::arm(); qa::publish(0,-32768);
    qa::onFinalGate=[] { qa::put(p::kImageBase+session::kSimFlagsRva,std::uint32_t{2}); };
    qa::fetch(0.12f,0.34f);
    assert(!g.armed && g.requested && g.directionMozaCalls==0 &&
           qa::nativeGasArg==0.12f && qa::nativeBrakeArg==0.34f && !g.directionInFlight); ++tests;
    // A native function already running may finish; defer ownership cleanup.
    qa::reset(); qa::arm(); qa::publish(-32768,0); qa::emulateDirection=true;
    qa::duringNative=[] {
        qa::put(p::kImageBase+session::kSimFlagsRva,std::uint32_t{2});
        watchdogTick(); assert(g.requested && !g.armed && g.cleanupPending);
    };
    qa::fetch(); assert(g.requested && !g.armed && !g.cleanupPending && !g.directionInFlight);
    assert(qa::get<float>(qa::controls+0x14)==0 && qa::get<float>(qa::controls+0x18)==0 &&
           qa::get<float>(qa::controls+4)==-0.2f); ++tests;


    // Pending ownership cleanup forbids resume, even with recovered focus/gate.
    qa::reset(); qa::arm(); qa::writeLive(); qa::failFloatAddress=qa::controls+0x14;
    focusSuspend(); assert(g.cleanupPending); qa::focused=true;
    frames(1000); assert(!g.armed && g.requested && g.cleanupPending);
    qa::failFloatAddress=0; frames(520); assert(g.armed && !g.cleanupPending); ++tests;
    // Repeated same-player cycles preserve intent and resume without neutral dwell.
    qa::reset(); qa::arm();
    for(unsigned i=0;i<10;++i) {
        qa::writeLive(); focusSuspend(); frames(100); qa::focused=true;
        frames(10,0,-32768,90); assert(g.armed && g.requested);
    }
    qa::press(); assert(!g.requested && !g.armed); ++tests;
    // Out-of-focus F8 never enables a previously OFF mode.
    qa::reset(); qa::neutralReady(); qa::focused=false; qa::key=true; watchdogTick();
    qa::focused=true; watchdogTick(); assert(!g.requested && !g.armed);
    qa::key=false; watchdogTick(); frames(1000); assert(!g.armed); ++tests;
    // Invalid context blocks initial activation without creating latent intent.
    qa::reset(); qa::neutralReady();
    qa::memory.erase(p::kImageBase+session::kFlowRva); qa::press();
    assert(!g.requested && !g.armed); ++tests;
    // S2 edges use the SAME neutral/context safety checks as F8.
    auto s2=[] {g_buttonEventTick=static_cast<LONG>(qa::clock); onPoll();};
    qa::reset(); qa::neutralReady(); s2();
    assert(g.armed && g.requested && qa::logged("TOGGLE source=S2 button=37")); ++tests;
    // Mailbox is consumed exactly once even if both watchdog/poll run repeatedly.
    onPoll(); watchdogTick(); onPoll(); assert(g.armed); ++tests;
    s2(); assert(!g.armed && !g.requested); ++tests;
    qa::reset(); s2(); assert(!g.armed && !g.requested); ++tests;
    qa::reset(); qa::neutralReady(); qa::put(p::kImageBase+session::kSimFlagsRva,std::uint32_t{2});
    s2(); assert(!g.armed && !g.requested); ++tests;
    qa::reset(); qa::neutralReady(); qa::focused=false; s2();
    qa::focused=true; onPoll(); assert(!g.armed && !g.requested); ++tests;
    qa::reset(); qa::neutralReady(); g_buttonEventTick=static_cast<LONG>(qa::clock-151);
    onPoll(); assert(!g.armed && !g.requested); ++tests;
    qa::reset(); qa::neutralReady(); qa::key=true; s2();
    assert(g.armed && g.requested && qa::logged("TOGGLE source=F8+S2"));
    onPoll(); assert(g.armed); ++tests;
    qa::reset(); qa::arm(); qa::esc=true; watchdogTick();
    g_buttonEventTick=static_cast<LONG>(qa::clock); watchdogTick();
    assert(!g.armed && !g.requested); qa::esc=false; frames(1000); assert(!g.armed); ++tests;
    qa::reset(); qa::neutralReady(); g_toggleButton=0; s2(); assert(!g.armed);
    qa::press(); assert(g.armed); ++tests;
    qa::reset(); qa::cfgButton=0; assert(openLogAndConfig() && g_toggleButton==0);
    qa::cfgButton=37; assert(openLogAndConfig() && g_toggleButton==37);
    qa::cfgButton=128; assert(openLogAndConfig() && g_toggleButton==128); ++tests;
    qa::cfgButton=129; assert(!openLogAndConfig());
    qa::cfgButton=-1; assert(!openLogAndConfig()); ++tests;
    // UI mask getter: original once, preserve native bits, scope to player0/UI.
    qa::reset();g_menuEnabled=true;g_menuOriginal=&qa::nativeMenu;
    qa::menuNativeCalls=0;qa::menuNativeValue=0x10u;
    qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{3});
    publishMenu(true,0,qa::clock);publishMenu(true,1,qa::clock);
    assert(menuReaderHook(ptr(1),nullptr,0)==0x11 && qa::menuNativeCalls==1);++tests;
    assert(menuReaderHook(ptr(1),nullptr,1)==0x10 && qa::menuNativeCalls==2);++tests;
    qa::focused=false;assert(menuReaderHook(ptr(1),nullptr,0)==0x10);++tests;
    qa::focused=true;qa::clock+=151;assert(menuReaderHook(ptr(1),nullptr,0)==0x10);++tests;
    qa::reset();g_menuEnabled=true;g_menuOriginal=&qa::nativeMenu;qa::menuNativeValue=0;
    qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{3});
    publishMenu(true,1,qa::clock);assert(menuReaderHook(ptr(1),nullptr,0)==0);
    publishMenu(true,0,qa::clock);publishMenu(true,1,qa::clock);
    assert(menuReaderHook(ptr(1),nullptr,0)==1);++tests;
    qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{6});
    assert(menuReaderHook(ptr(1),nullptr,0)==0);++tests;
    publishMenu(true,1,qa::clock);qa::put(p::kImageBase+session::kSimFlagsRva,std::uint32_t{2});
    publishMenu(true,1,qa::clock);assert(menuReaderHook(ptr(1),nullptr,0)==0);
    publishMenu(true,0,qa::clock);publishMenu(true,2,qa::clock);
    assert(menuReaderHook(ptr(1),nullptr,0)==8);++tests;
    qa::menuNativeValue=0xFFFF0000u;assert(menuReaderHook(ptr(1),nullptr,0)==0xFFFF0008u);
    qa::menuNativeValue=8;assert(menuReaderHook(ptr(1),nullptr,0)==8);++tests;
    qa::reset();qa::cfgMenu=1;assert(openLogAndConfig() && g_menuEnabled);
    qa::cfgButton=5;assert(!openLogAndConfig());qa::cfgButton=8;assert(!openLogAndConfig());
    qa::cfgButton=37;qa::cfgMenu=2;assert(!openLogAndConfig());++tests;
    qa::reset();g_menuEnabled=true;
    qa::put(p::kImageBase+menu::kReaderRva,menu::kReaderEntry);
    qa::put(p::kImageBase+menu::kReturnRva,menu::kReaderReturn);
    qa::put(p::kImageBase+menu::kThunkRva,menu::kThunk);
    for(unsigned row=0;row<4;++row) {
        qa::put(p::kImageBase+0x4F3428u+row*28u,25u+row);
        qa::put(p::kImageBase+0x4F342Cu+row*28u,1u<<row);
    }
    assert(installMenuHook() && g_menuOriginal==&qa::nativeMenu);++tests;
    qa::reset();g_menuEnabled=true;assert(!installMenuHook());++tests;
    qa::put(p::kImageBase+0x4F3428u,std::uint32_t{25});
    qa::put(p::kImageBase+0x4F342Cu,std::uint32_t{1});
    // Invalid tables cannot install even when the function bytes are valid.
    qa::put(p::kImageBase+menu::kReaderRva,menu::kReaderEntry);
    qa::put(p::kImageBase+menu::kReturnRva,menu::kReaderReturn);
    qa::put(p::kImageBase+menu::kThunkRva,menu::kThunk);
    for(unsigned row=0;row<4;++row) {
        qa::put(p::kImageBase+0x4F3428u+row*28u,25u+row);
        qa::put(p::kImageBase+0x4F342Cu+row*28u,1u<<row);
    }
    assert(menu::fingerprint(g_base,&readMemory));
    qa::put(p::kImageBase+0x4F342Cu,std::uint32_t{2});
    assert(!menu::fingerprint(g_base,&readMemory));++tests;
    qa::put(p::kImageBase+0x4F342Cu,std::uint32_t{1});
    qa::put(p::kImageBase+0x4F3428u,std::uint32_t{26});
    assert(!menu::fingerprint(g_base,&readMemory));++tests;
    qa::reset();g_menuOriginal=&qa::nativeMenu;qa::menuNativeValue=0x55;qa::menuNativeCalls=0;
    assert(menuReaderHook(ptr(1),nullptr,0)==0x55 && qa::menuNativeCalls==1);++tests;
    g_menuEnabled=true;InterlockedExchange(&g_ready,0);
    assert(menuReaderHook(ptr(1),nullptr,0)==0x55 && qa::menuNativeCalls==2);++tests;
    qa::reset();g_menuEnabled=true;g_menuOriginal=&qa::nativeMenu;qa::menuNativeValue=0x10;
    qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{3});
    publishMenu(true,0,qa::clock);publishMenu(true,1,qa::clock);
    assert(menuReaderHook(ptr(1),nullptr,0)==0x11);
    publishMenu(true,0,qa::clock);
    assert(menuReaderHook(ptr(1),nullptr,0)==0x10);++tests;
    // HUD is optional, original exactly once, no lock held, this/stack bits preserved.
    qa::reset();qa::hudFixture();g_hudOriginal=&qa::nativeHud;qa::hudChangeValue=true;
    auto writesBefore=qa::memoryWrites;hudTickHook(ptr(qa::hudWidget),nullptr,0x7FC12345u);
    assert(qa::hudNativeCalls==1&&qa::hudSelf==ptr(qa::hudWidget)&&qa::hudArg==0x7FC12345u);
    assert(g_hudCache.latest.status==hud::Status::Ok&&g_hudCache.latest.current==4800&&qa::memoryWrites==writesBefore);++tests;
    qa::clock+=50;hudTickHook(ptr(qa::hudWidget),nullptr,0xDEADBEEFu);
    assert(qa::hudNativeCalls==2&&qa::hudArg==0xDEADBEEFu&&g_hudCaptures==1);++tests;
    qa::clock+=50;hudTickHook(ptr(qa::hudWidget),nullptr,1);assert(g_hudCaptures==2&&qa::hudNativeCalls==3);++tests;
    qa::reset();qa::hudFixture();g_hudOriginal=&qa::nativeHud;g_ready=0;hudTickHook(ptr(qa::hudWidget),nullptr,7);
    assert(qa::hudNativeCalls==1&&!g_hudCache.seen);++tests;
    qa::reset();qa::hudFixture();g_hudOriginal=&qa::nativeHud;qa::focused=false;hudTickHook(ptr(qa::hudWidget),nullptr,8);
    assert(qa::hudNativeCalls==1&&g_hudCache.latest.status==hud::Status::ContextBlocked);++tests;
    qa::reset();qa::hudFixture();g_hudOriginal=&qa::nativeHud;qa::hudPause=true;hudTickHook(ptr(qa::hudWidget),nullptr,8);
    assert(qa::hudNativeCalls==1&&g_hudCache.latest.status==hud::Status::ContextBlocked);++tests;
    qa::reset();qa::arm();qa::hudFixture();g_hudOriginal=&qa::nativeHud;qa::put(qa::hudWidget+0x30,std::uint32_t{0});
    writesBefore=qa::memoryWrites;hudTickHook(ptr(qa::hudWidget),nullptr,8);
    assert(g.armed&&g.requested&&g.writeErrors==0&&qa::memoryWrites==writesBefore&&g_hudCache.latest.status==hud::Status::LayoutMismatch);++tests;
    qa::reset();qa::hudFixture();g_hudOriginal=&qa::nativeHud;
    hudTickHook(ptr(qa::hudWidget),nullptr,0);qa::hudFixture(qa::hudWidget+0x1000);qa::clock+=1;
    hudTickHook(ptr(qa::hudWidget+0x1000),nullptr,0);assert(g_hudCache.latest.status==hud::Status::Ambiguous&&g_hudCaptures==2);++tests;
    qa::reset();qa::hudFixture();g_hudOriginal=&qa::nativeHud;g_hudInstalled=true;g_reportIntervalMs=0;
    hudTickHook(ptr(qa::hudWidget),nullptr,0);writesBefore=qa::memoryWrites;onPoll();
    assert(!g.armed&&qa::memoryWrites==writesBefore&&qa::logged("HUD_TELEMETRY status=1")&&qa::logged("current_hud=800.000"));++tests;
    qa::reset();qa::arm();qa::hudFixture();g_hudOriginal=&qa::nativeHud;g_hudInstalled=true;g_reportIntervalMs=0;
    hudTickHook(ptr(qa::hudWidget),nullptr,0);writesBefore=qa::memoryWrites;onPoll();
    assert(g.armed&&g.requested&&g.writeErrors==0&&qa::memoryWrites==writesBefore&&qa::logged("HUD_TELEMETRY status=1"));++tests;
    qa::reset();qa::hudFixture();g_hudOriginal=&qa::nativeHud;g_hudInstalled=true;g_reportIntervalMs=0;
    hudTickHook(ptr(qa::hudWidget),nullptr,0);qa::clock+=501;onPoll();assert(qa::logged("HUD_TELEMETRY status=9"));++tests;
    qa::reset();qa::arm();g_reportIntervalMs=0;writesBefore=qa::memoryWrites;onPoll();
    assert(g.armed&&g.requested&&g.writeErrors==0&&qa::memoryWrites==writesBefore&&qa::logged("HUD_TELEMETRY status=12"));++tests;
    qa::reset();qa::hudFixture();assert(installHudHook()&&g_hudOriginal==&qa::nativeHud&&qa::mhCreates==1&&qa::mhEnables==1);++tests;
    qa::reset();qa::hudFixture();qa::mhCreateStatus=7;assert(!installHudHook()&&qa::mhEnables==0);++tests;
    qa::reset();qa::hudFixture();qa::mhEnableStatus=7;assert(!installHudHook()&&!g_hudOriginal&&qa::mhRemoves==1);++tests;
    qa::reset();qa::hudFixture();qa::mhNullTrampoline=true;assert(!installHudHook()&&!g_hudOriginal&&qa::mhRemoves==1);++tests;
    qa::reset();qa::hudFixture();qa::put(p::kImageBase+hud::kTickRva,std::uint8_t{0xE9});
    assert(!installHudHook()&&qa::mhCreates==0);++tests;
    qa::reset();qa::hudFixture();qa::duringEnable=[](){hudTickHook(ptr(qa::hudWidget),nullptr,0xFEED1234u);};
    assert(installHudHook()&&qa::hudNativeCalls==1&&qa::hudArg==0xFEED1234u);++tests;

    // UDP snapshot selection is read-only, independent of F8 and never takes the control lock.
    qa::reset();qa::hudFixture();g_hudInstalled=true;
    hud::publish(g_hudCache,hud::sample(g_base,p::resolvePlayer(g_base,&readMemory),qa::hudWidget,&readMemory),qa::clock);
    {std::lock_guard<std::mutex> hold(qa::lock);
        assert(selectUdpSnapshot(qa::clock).status==hud::Status::Ok);}
    assert(!g.armed && qa::memoryWrites==0);++tests;
    qa::focused=false;assert(selectUdpSnapshot(qa::clock).status==hud::Status::ContextBlocked);++tests;
    qa::focused=true;qa::clock+=501;assert(selectUdpSnapshot(qa::clock).status==hud::Status::Stale);++tests;
    g_hudInstalled=false;assert(selectUdpSnapshot(qa::clock).status==hud::Status::HookUnavailable);++tests;
    assert(qa::memoryWrites==0);++tests;
    qa::reset();assert(openLogAndConfig() && !g_udpEnabled && g_udpPort==20066);++tests;
    qa::reset();qa::cfgUdp=1;assert(openLogAndConfig() && g_udpEnabled && g_udpPort==20066);++tests;
    qa::reset();qa::cfgUdp=2;assert(openLogAndConfig() && !g_udpEnabled && g_config.wheelRangeDegrees==900);++tests;
    qa::reset();qa::cfgUdp=1;qa::cfgPort=0;assert(openLogAndConfig() && !g_udpEnabled);++tests;
    qa::reset();qa::cfgUdp=1;qa::cfgPort=65536;assert(openLogAndConfig() && !g_udpEnabled);++tests;
    qa::reset();qa::cfgUdp=1;qa::cfgPort=20055;assert(openLogAndConfig() && g_udpEnabled && g_udpPort==20055);++tests;
    // v0.7.7 regression: instant resume applies CURRENT axes, not the pre-pause sample.
    qa::reset(); qa::arm(); qa::writeLive(); qa::esc=true; watchdogTick();
    assert(g.resumeImmediate && !g.armed); qa::esc=false;
    frames(10,0,-32768,-225); assert(g.armed && !g.resumeImmediate);
    qa::emulateDirection=true; qa::fetch(); getControlsHook(ptr(qa::input),nullptr);
    assert(qa::nativeGasArg>0.49f && qa::nativeGasArg<0.51f && qa::nativeBrakeArg==0);
    assert(qa::get<float>(qa::controls+4)==-0.5f); ++tests;

    // Held ESC and blocked simulation still prohibit all rearming.
    qa::reset(); qa::arm(); qa::esc=true; watchdogTick();
    frames(1000,32767,32767,180); assert(!g.armed && g.resumeImmediate);
    qa::esc=false; block(2); frames(1000,32767,32767,180); assert(!g.armed);
    block(0); frames(10,32767,32767,180); assert(g.armed); ++tests;

    // Watchdog / MOZA publisher NEVER auto-arm; only the game callback does.
    qa::reset(); qa::arm(); focusSuspend(); qa::focused=true;
    qa::publish(0,-32768,90); qa::heartbeat(); watchdogTick(); assert(!g.armed);
    onPoll(); assert(g.armed); ++tests;

    // Player loss preserves authorized intent, but never permits writes while missing.
    qa::reset(); qa::arm(); focusSuspend();
    qa::put(p::kImageBase+p::kVehicleListRva,p::VehicleEntry{}); watchdogTick();
    assert(g.resumeImmediate && !g.armed); qa::addCar(0,qa::car,qa::input); qa::focused=true; onPoll();
    frames(10,0,-32768,90);
    assert(g.armed && qa::logged("neutral_dwell_ms=0 mode=validated-auto-resume")); ++tests;

    // Explicit OFF resets fast permission; initial ON still requires neutral.
    qa::reset(); qa::arm(); focusSuspend(); qa::focused=true; qa::publish(0,-32768,90);
    qa::press(); assert(!g.requested && !g.armed && !g.resumeImmediate);
    qa::press(); assert(!g.requested && !g.armed); ++tests;

    // Pausing repeatedly while already suspended must not lose the fast permission.
    qa::reset(); qa::arm(); focusSuspend();
    for(unsigned i=0;i<5;++i) { stopTest(StopReason::NotDriving); stopTest(StopReason::FocusLost); }
    assert(g.resumeImmediate); qa::focused=true; frames(10,32767,-32768,225); assert(g.armed); ++tests;

    // Fatal error cancels both intent and the fast permission.
    qa::reset(); qa::arm(); focusSuspend(); stopTest(StopReason::WriteFailed);
    assert(!g.requested && !g.resumeImmediate); qa::focused=true; frames(600); assert(!g.armed); ++tests;
    // v0.7.8: menu queue requires no player and causes no game-memory writes.
    auto menuOnly=[] {
        qa::put(p::kImageBase+p::kVehicleListRva,p::VehicleEntry{});
        qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{3});
        g.hookAttempted=false; g.hookInstalled=false;
        qa::put(p::kImageBase+p::kPlayerInputVtableRva+p::kGetterSlot*4,p::kImageBase+p::kGetControlsRva);
        watchdogTick();
        qa::neutralReady();
    };
    auto queueMenu=[&] {
        qa::reset(); menuOnly(); qa::press();
        assert(g.requested && g.startQueued && !g.armed && g_liveGate==0);
    };
    qa::reset(); menuOnly(); const auto noMenuWrites=qa::memoryWrites; qa::press();
    assert(g.requested && g.startQueued && !g.armed && g_liveGate==0 &&
           qa::memoryWrites==noMenuWrites && qa::logged("WAIT_RACE source=MOZA")); ++tests;

    // Any position AFTER neutral-approved queue can be used on the first valid race poll.
    queueMenu(); const auto queuedWrites=qa::memoryWrites;
    frames(2000,0,-32768,225);
    assert(g.startQueued && !g.armed && qa::memoryWrites==queuedWrites);
    qa::addCar(0,qa::car,qa::input); qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{6});
    onPoll(); assert(g.startQueued && !g.armed); // No native helper heartbeat for new player yet.
    frames(10,0,-32768,225);
    assert(g.armed && !g.startQueued && qa::logged("mode=menu-queued-start"));
    qa::fetch(); getControlsHook(ptr(qa::input),nullptr);
    assert(qa::nativeGasArg>0.49f && qa::get<float>(qa::controls+4)==0.5f); ++tests;

    // S2 queues from watchdog without a game on_poll (front-end poll may not run).
    qa::reset(); menuOnly(); g_buttonEventTick=static_cast<LONG>(qa::clock); watchdogTick();
    assert(g.requested && g.startQueued && !g.armed);
    g_buttonEventTick=static_cast<LONG>(qa::clock); watchdogTick();
    assert(!g.requested && !g.startQueued && !g.armed); ++tests;

    // F8 cancels queue; entering race then does not auto-enable.
    queueMenu(); qa::press(); assert(!g.requested && !g.startQueued);
    qa::addCar(0,qa::car,qa::input); qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{6});
    frames(700); assert(!g.armed); ++tests;

    // Reject first menu activation with gas, brake, or off-centre steering.
    for(unsigned kind=0;kind<3;++kind) {
        qa::reset(); menuOnly();
        qa::publish(kind==0 ? 0 : -32768,kind==1 ? 0 : -32768,kind==2 ? 90.0f : 0.0f);
        qa::press(); assert(!g.requested && !g.startQueued); ++tests;
    }
    // Focus/ESC, unknown flow and unreadable globals cannot create intent.
    qa::reset(); menuOnly(); qa::focused=false; qa::press(); assert(!g.requested); ++tests;
    qa::reset(); menuOnly(); qa::esc=true; qa::press(); assert(!g.requested); ++tests;
    qa::reset(); menuOnly(); qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{4});
    qa::press(); assert(!g.requested); ++tests;
    qa::reset(); menuOnly(); qa::memory.erase(p::kImageBase+session::kSimFlagsRva);
    qa::press(); assert(!g.requested); ++tests;

    // Stale/error MOZA in menu cancels a queued request, no hidden reconnect retry.
    queueMenu(); qa::clock+=150; watchdogTick();
    assert(!g.requested && !g.startQueued && !g.armed); ++tests;
    queueMenu(); bad=qa::raw(); bad.sdkError=3; publishMoza(MW05_MOZA_SDK_ERROR,bad,qa::clock);
    watchdogTick(); assert(!g.requested && !g.startQueued); ++tests;
    queueMenu(); qa::memory.erase(p::kImageBase+session::kSimFlagsRva); watchdogTick();
    assert(!g.requested && !g.startQueued); ++tests;

    // Pending intent survives ordinary loading and model preview identity changes.
    queueMenu(); qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{4});
    frames(700,0,-32768,90); assert(g.startQueued && !g.armed);
    qa::addCar(0,qa::car,qa::input); watchdogTick(); assert(g.startQueued && !g.armed);
    qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{6});
    frames(10,0,-32768,90); assert(g.armed && !g.startQueued); ++tests;

    // No queue when a required hook is known to have failed or cleanup is outstanding.
    qa::reset(); menuOnly(); g.directionInstalled=false; qa::press(); assert(!g.requested); ++tests;
    qa::reset(); menuOnly(); g.hookAttempted=true; g.hookInstalled=false;
    qa::press(); assert(!g.requested); ++tests;
    qa::reset(); menuOnly(); g.cleanupPending=true; g.directionInFlight=true;
    qa::press(); assert(!g.requested); ++tests;

    // Focus loss preserves explicit menu intent, but cannot activate outside focus.
    queueMenu(); qa::focused=false; frames(700,0,-32768,90);
    assert(g.startQueued && !g.armed);
    qa::addCar(0,qa::car,qa::input); qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{6});
    frames(100,0,-32768,90); assert(!g.armed);
    qa::focused=true; frames(10,0,-32768,90); assert(g.armed); ++tests;

    // Queue cannot bypass a blocked race or an in-flight native helper.
    queueMenu(); qa::addCar(0,qa::car,qa::input);
    qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{6}); block(2);
    frames(700,0,-32768,90); assert(!g.armed && g.startQueued);
    block(0); g.directionInFlight=true; frames(100,0,-32768,90); assert(!g.armed);
    g.directionInFlight=false; frames(10,0,-32768,90); assert(g.armed); ++tests;

    // Consumed queue becomes an authorized active session; later player loss preserves it.
    queueMenu(); qa::addCar(0,qa::car,qa::input);
    qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{6}); frames(20);
    assert(g.armed && !g.startQueued);
    qa::put(p::kImageBase+p::kVehicleListRva,p::VehicleEntry{}); watchdogTick();
    qa::addCar(0,qa::car,qa::input); onPoll(); frames(1000,0,-32768,90);
    assert(g.armed && !g.startQueued); ++tests;
    // v0.7.9: repeated rows do not cancel an already active player.
    qa::reset();qa::arm();qa::writeLive();
    const auto activePlayer=g.player;
    qa::put(p::kImageBase+p::kVehicleListRva+8,p::VehicleEntry{qa::car,1,{0,0,0}});
    qa::put(p::kImageBase+p::kVehicleListRva+16,p::VehicleEntry{qa::car,1,{0,0,0}});
    qa::put(p::kImageBase+p::kVehicleListRva+24,p::VehicleEntry{});
    onPoll();assert(g.armed && g.player.status==p::ResolveStatus::Unique &&
        g.player.vehicle==activePlayer.vehicle && g.player.input==activePlayer.input);++tests;
    qa::fetch();getControlsHook(ptr(qa::input),nullptr);
    assert(g.armed && qa::get<float>(qa::controls+4)==0.5f);++tests;

    // HUD and UDP share resolver: exact duplicate rows must no longer suppress RPM.
    qa::hudFixture();g_hudOriginal=&qa::nativeHud;g_hudInstalled=true;
    hudTickHook(ptr(qa::hudWidget),nullptr,0);
    assert(g_hudCache.latest.status==hud::Status::Ok && selectUdpSnapshot(qa::clock).status==hud::Status::Ok);++tests;

    // Real distinct candidates still block controls and telemetry, without writes.
    qa::addCar(2,0x04100000,0x04120000);
    const auto ambiguityWrites=qa::memoryWrites;onPoll();
    assert(!g.armed && g.player.status==p::ResolveStatus::Ambiguous &&
        qa::memoryWrites==ambiguityWrites && selectUdpSnapshot(qa::clock).status==hud::Status::NoPlayer);++tests;

    // Recorded transition: vehicle changes while the input address is reused.
    // Revalidate the NEW car/back-reference/helper; retain intent, not an old object lease.
    qa::reset();qa::arm();qa::writeLive();
    constexpr p::Address newCar=0x033E7800;
    qa::addCar(0,newCar,qa::input);
    qa::put(p::kImageBase+p::kVehicleListRva+8,p::VehicleEntry{newCar,1,{0,0,0}});
    qa::put(p::kImageBase+p::kVehicleListRva+16,p::VehicleEntry{newCar,1,{0,0,0}});
    qa::put(p::kImageBase+p::kVehicleListRva+24,p::VehicleEntry{});
    qa::put(qa::input+drive::kInputVehicleOffset,newCar+drive::kVehicleInterfaceOffset);
    qa::put(newCar+drive::kVehicleInterfaceOffset,p::kImageBase+drive::kVehicleInterfaceVtableRva);
    qa::put(newCar+drive::kTransmissionOffset,qa::transmission);
    qa::put(newCar+drive::kVehicleInterfaceOffset+drive::kSpeedOffset,0.0f);
    const auto changeWrites=qa::memoryWrites;watchdogTick();
    assert(g.requested && !g.armed && g.resumeImmediate &&
        g.player.status==p::ResolveStatus::Unique && g.player.vehicle==newCar &&
        g.player.input==qa::input && qa::memoryWrites==changeWrites);++tests;
    // A reused input address does not preserve the old helper heartbeat.
    qa::clock+=10;qa::publish(0,-32768,90);onPoll();
    assert(!g.armed && qa::memoryWrites==changeWrites);++tests;
    qa::fetch();onPoll();assert(g.armed && g.player.vehicle==newCar &&
        qa::logged("neutral_dwell_ms=0 mode=validated-auto-resume"));++tests;
    qa::hudFixture();g_hudOriginal=&qa::nativeHud;g_hudInstalled=true;
    hudTickHook(ptr(qa::hudWidget),nullptr,0);
    assert(selectUdpSnapshot(qa::clock).status==hud::Status::Ok);++tests;

    // Temporary native-helper gap with unchanged identity: no second neutral dwell.
    qa::reset();qa::arm();qa::publish(0,-32768,120);qa::clock+=151;
    qa::publish(0,-32768,120);watchdogTick();
    assert(!g.armed && g.requested && g.resumeImmediate);
    const auto helperGapWrites=qa::memoryWrites;
    // Fresh SDK alone cannot restart anything; the native helper must return first.
    for(unsigned i=0;i<20;++i){qa::clock+=10;qa::publish(0,-32768,120);onPoll();}
    assert(!g.armed && qa::memoryWrites==helperGapWrites);++tests;
    qa::fetch();onPoll();
    assert(g.armed && qa::logged("neutral_dwell_ms=0 mode=validated-auto-resume"));
    getControlsHook(ptr(qa::input),nullptr);
    assert(qa::get<float>(qa::controls+4)>0.26f && qa::get<float>(qa::controls+0x14)>0.49f);++tests;

    // Even the helper-gap fast path cannot bypass game pause/cutscene flags.
    qa::reset();qa::arm();qa::clock+=151;qa::publish(0,-32768,120);watchdogTick();
    block(2);frames(700,0,-32768,120);assert(!g.armed && g.resumeImmediate);
    block(0);frames(10,0,-32768,120);assert(g.armed);++tests;

    // Actual player loss during helper suspension retains intent, requiring fresh gates.
    qa::reset();qa::arm();qa::clock+=151;qa::publish(0,-32768,120);watchdogTick();
    qa::put(p::kImageBase+p::kVehicleListRva,p::VehicleEntry{});watchdogTick();
    assert(g.resumeImmediate);qa::addCar(0,qa::car,qa::input);onPoll();
    frames(10,0,-32768,120);assert(g.armed);++tests;

    // SDK fault after helper suspension cancels intent, not hidden retries.
    qa::reset();qa::arm();qa::clock+=151;qa::publish(0,-32768,120);watchdogTick();
    bad=qa::raw();bad.sdkError=2;publishMoza(MW05_MOZA_SDK_ERROR,bad,qa::clock);onPoll();
    // onPoll may wait for a helper before tryResume validates SDK. enforceSafety
    // while suspended is unchanged; restore a helper to exercise fatal validation.
    qa::heartbeat();onPoll();assert(!g.requested && !g.armed && !g.resumeImmediate);++tests;

    // v0.7.10: retained intent is not a write lease; revalidate every new identity.
    auto rebind=[&](p::Address in=qa::input) {
        qa::reset();qa::arm();qa::writeLive();
        qa::addCar(0,newCar,in);
        qa::put(in-direction::kOwnerOffset,p::kImageBase+direction::kOwnerVtableRva);
        qa::put(in+drive::kInputVehicleOffset,newCar+drive::kVehicleInterfaceOffset);
        qa::put(newCar+drive::kVehicleInterfaceOffset,p::kImageBase+drive::kVehicleInterfaceVtableRva);
        qa::put(newCar+drive::kTransmissionOffset,qa::transmission);
        qa::put(newCar+drive::kVehicleInterfaceOffset+drive::kSpeedOffset,0.0f);
        watchdogTick();qa::publish(32767,-32768,120);
        assert(g.requested && !g.armed && g.resumeImmediate && !g.directionSeen);
    };
    // Watchdog cannot activate a replacement, even with all data and a heartbeat.
    rebind();qa::heartbeat();const auto rebindWrites=qa::memoryWrites;
    watchdogTick();assert(!g.armed && qa::memoryWrites==rebindWrites);
    onPoll();assert(g.armed && !g.resumeNeutral.tracking);++tests;

    // No helper calls: no activation, despite fresh SDK and a unique new player.
    rebind();const auto noHelperWrites=qa::memoryWrites;
    for(unsigned i=0;i<70;++i){qa::clock+=10;qa::publish(32767,-32768,120);onPoll();}
    assert(!g.armed && g.requested && qa::memoryWrites==noHelperWrites);++tests;

    // Every game-blocking mask bit still blocks the replacement-player path.
    for(unsigned bit=0;bit<32;++bit) {
        const std::uint32_t flag=std::uint32_t{1}<<bit;
        if(!(session::kBlockingMask&flag))continue;
        rebind();block(flag);qa::heartbeat();onPoll();assert(!g.armed && g.requested);
        block(0);qa::heartbeat();onPoll();assert(g.armed);
    }++tests;

    rebind();qa::focused=false;qa::heartbeat();onPoll();assert(!g.armed && g.requested);
    qa::focused=true;qa::heartbeat();onPoll();assert(g.armed);++tests;

    rebind();qa::esc=true;qa::heartbeat();onPoll();assert(!g.armed && g.requested);
    qa::esc=false;qa::heartbeat();onPoll();assert(g.armed);++tests;

    // A real SDK fault cancels permission. Later samples cannot silently restart it.
    rebind();bad=qa::raw();bad.sdkError=3;publishMoza(MW05_MOZA_SDK_ERROR,bad,qa::clock);
    qa::heartbeat();onPoll();assert(!g.requested && !g.resumeImmediate && !g.armed);
    frames(30,32767,-32768,120);assert(!g.armed);
    qa::press();assert(!g.requested && !g.armed);++tests;

    // Explicit OFF during rebinding must persist, including non-neutral held inputs.
    rebind();qa::press();assert(!g.requested && !g.resumeImmediate && !g.armed);
    frames(30,32767,-32768,120);assert(!g.armed);++tests;

    // An actually different candidate still blocks; duplicates alone do not.
    rebind();qa::addCar(1,0x04500000,0x04510000);
    qa::put(p::kImageBase+p::kVehicleListRva+16,p::VehicleEntry{});
    const auto ambiguousWrites=qa::memoryWrites;onPoll();
    assert(!g.armed && g.player.status==p::ResolveStatus::Ambiguous &&
           qa::memoryWrites==ambiguousWrites);++tests;

    rebind();qa::put(qa::input-direction::kOwnerOffset,std::uint32_t{0});
    qa::heartbeat();onPoll();assert(!g.armed && !g.requested);++tests;

    rebind();qa::put(qa::input+drive::kInputVehicleOffset,
        qa::car+drive::kVehicleInterfaceOffset);
    qa::heartbeat();onPoll();assert(!g.armed && !g.requested);++tests;

    rebind();qa::memory.erase(qa::controls+0x23);
    qa::heartbeat();onPoll();assert(!g.armed && !g.requested);++tests;

    rebind();qa::clock+=200;qa::heartbeat();onPoll();
    assert(!g.armed && !g.requested && !g.resumeImmediate);++tests;

    rebind();qa::heartbeat();g.directionInFlight=true;onPoll();assert(!g.armed);
    g.directionInFlight=false;onPoll();assert(g.armed);++tests;

    // Different input allocation: use current sample only on NEW controls.
    constexpr p::Address newInput=0x04410048;
    rebind(newInput);const auto oldSteer=qa::get<float>(qa::controls+4);
    const auto oldGas=qa::get<float>(qa::controls+0x14);
    qa::heartbeat();onPoll();assert(g.armed && g.player.input==newInput);
    getControlsHook(ptr(newInput),nullptr);
    assert(qa::get<float>(newInput+8+4)>0.26f &&
           qa::get<float>(newInput+8+0x14)==1.0f &&
           qa::get<float>(qa::controls+4)==oldSteer &&
           qa::get<float>(qa::controls+0x14)==oldGas);++tests;


    // v0.7.11: mimic an explosion leaving a long nonzero inactive tail.
    qa::reset();qa::arm();qa::writeLive();
    for(unsigned i=1;i<72;++i){
        qa::addCar(i,0x05000000+i*0x1000,0x06000000+i*0x1000);
        qa::put(0x06000000+i*0x1000,std::uint32_t{0x008AB5C8});
    }
    // A stale tail may even resemble a second valid player: MUST NOT inspect it.
    qa::put(0x06000000+64*0x1000,std::uint32_t{p::kImageBase+p::kPlayerInputVtableRva});
    qa::put(p::kImageBase+p::kVehicleContainerRva+12,std::uint32_t{32});
    qa::publish(0,-32768,120);qa::heartbeat();onPoll();
    assert(g.armed && g.player.vehicle==qa::car && g.player.status==p::ResolveStatus::Unique);++tests;
    getControlsHook(ptr(qa::input),nullptr);
    assert(g.armed && qa::get<float>(qa::controls+4)>0.26f && qa::get<float>(qa::controls+0x14)>0.49f);++tests;
    qa::hudFixture();g_hudOriginal=&qa::nativeHud;g_hudInstalled=true;
    hudTickHook(ptr(qa::hudWidget),nullptr,0);
    assert(selectUdpSnapshot(qa::clock).status==hud::Status::Ok);++tests;
    // If that second candidate becomes genuinely ACTIVE, ambiguity still blocks.
    qa::put(p::kImageBase+p::kVehicleContainerRva+12,std::uint32_t{65});
    const auto liveAmbiguousWrites=qa::memoryWrites;onPoll();
    assert(!g.armed && g.requested && g.player.status==p::ResolveStatus::Ambiguous &&
           qa::memoryWrites==liveAmbiguousWrites);++tests;
    qa::put(p::kImageBase+p::kVehicleContainerRva+12,std::uint32_t{32});
    qa::publish(0,-32768,120);onPoll();qa::fetch();onPoll();assert(g.armed);++tests;
    // Corrupt container size cannot become a larger unbounded scan.
    qa::put(p::kImageBase+p::kVehicleContainerRva+12,std::uint32_t{161});
    const auto badHeaderWrites=qa::memoryWrites;onPoll();
    assert(!g.armed && g.player.status==p::ResolveStatus::InvalidContainer &&
           qa::memoryWrites==badHeaderWrites);++tests;

    // v0.7.12: race confirmation dialog accepts UI directions, not axes.
    qa::reset();g_menuEnabled=true;g_menuOriginal=&qa::nativeMenu;qa::menuNativeValue=0x100;
    qa::put(p::kImageBase+session::kFlowRva,std::uint32_t{6});
    qa::put(p::kImageBase+session::kSimFlagsRva,std::uint32_t{0x10});
    const auto dialogWrites=qa::memoryWrites;
    publishMenu(true,1,qa::clock);assert(menuReaderHook(ptr(1),nullptr,0)==0x100);++tests;
    publishMenu(true,0,qa::clock);publishMenu(true,8,qa::clock);
    assert(menuReaderHook(ptr(1),nullptr,0)==0x104 && qa::memoryWrites==dialogWrites);++tests;
    assert(menuReaderHook(ptr(1),nullptr,1)==0x100);++tests;
    qa::focused=false;assert(menuReaderHook(ptr(1),nullptr,0)==0x100);qa::focused=true;++tests;
    qa::put(p::kImageBase+session::kSimFlagsRva,std::uint32_t{0});
    assert(menuReaderHook(ptr(1),nullptr,0)==0x100);++tests;
    qa::reset();qa::arm();
    qa::put(p::kImageBase+session::kSimFlagsRva,std::uint32_t{0x10});
    const auto blockedDialogWrites=qa::memoryWrites;onPoll();
    assert(!g.armed && qa::memoryWrites==blockedDialogWrites);++tests;

    std::printf("PASS: %u live-plugin mock groups (no real Windows threads/SDK/game)\n",tests);
}
