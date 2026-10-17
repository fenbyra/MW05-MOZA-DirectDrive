static_assert(sizeof(void*) == 4, "x86 ABI check");
using Getter = void* (__attribute__((thiscall)) *)(void*);
extern Getter original;
extern void record(void* self, void* controls);
extern "C" void* __attribute__((fastcall)) detour(void* self, void*) {
    void* controls = original(self);
    record(self, controls);
    return controls;
}
using DirectionHelper = void (__attribute__((thiscall)) *)(void*,float,float);
extern DirectionHelper nativeOriginal;
extern void before(void* self,float& gas,float& brake);
extern void after(void* self);
extern "C" void __attribute__((fastcall)) directionDetour(
    void* self,void*,float gas,float brake) {
    before(self,gas,brake);
    nativeOriginal(self,gas,brake);
    after(self);
}

using MenuMaskGetter = unsigned (__attribute__((thiscall)) *)(void*,unsigned char);
extern MenuMaskGetter menuOriginal;
extern unsigned mergeMenu(unsigned,unsigned char);
extern "C" unsigned __attribute__((fastcall)) menuMaskDetour(void* self,void*,unsigned char player) {
    return mergeMenu(menuOriginal(self,player),player);
}

// HUD native tick: one opaque DWORD stack parameter, unchanged bit-for-bit.
using HudTick = void (__attribute__((thiscall)) *)(void*,unsigned);
extern HudTick hudOriginal;
extern void captureHud(void*);
extern "C" void __attribute__((fastcall)) hudTickDetour(void* self,void*,unsigned rawArg) {
    hudOriginal(self,rawArg);
    captureHud(self);
}
