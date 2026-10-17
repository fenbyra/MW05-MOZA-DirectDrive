#ifndef MW05_BUTTON_FAKE_DI
#define MW05_BUTTON_FAKE_DI
#include <windows.h>
#define DIENUM_CONTINUE 1
#define DI8DEVCLASS_GAMECTRL 4
#define DIEDFL_ATTACHEDONLY 1
#define DIDFT_OPTIONAL 0x80000000u
#define DIDFT_BUTTON 0x0000000C
#define DIDFT_MAKEINSTANCE(n) ((n)<<8)
#define DIDF_ABSAXIS 1
#define DISCL_NONEXCLUSIVE 2
#define DISCL_FOREGROUND 4
struct DIDEVICEINSTANCEA {GUID guidInstance; char tszProductName[260],tszInstanceName[260];};
struct DIOBJECTDATAFORMAT {const GUID* pguid; DWORD dwOfs,dwType,dwFlags;};
struct DIDATAFORMAT {DWORD dwSize,dwObjSize,dwFlags,dwDataSize,dwNumObjs; DIOBJECTDATAFORMAT* rgodf;};
using DIEnumCallback=BOOL (CALLBACK *)(const DIDEVICEINSTANCEA*,VOID*);
struct IDirectInputDevice8A {
    HRESULT SetDataFormat(const DIDATAFORMAT*);
    HRESULT SetCooperativeLevel(HWND,DWORD);
    HRESULT Acquire(); HRESULT Unacquire(); unsigned long Release();
    HRESULT Poll(); HRESULT GetDeviceState(DWORD,LPVOID);
};
struct IDirectInput8A {
    HRESULT EnumDevices(DWORD,DIEnumCallback,VOID*,DWORD);
    HRESULT CreateDevice(const GUID&,IDirectInputDevice8A**,LPUNKNOWN);
    unsigned long Release();
};
#endif
