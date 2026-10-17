#ifndef MW05_BUTTON_FAKE_WINDOWS
#define MW05_BUTTON_FAKE_WINDOWS
#include "../host_stubs/windows.h"
#include <cstdint>
#define CALLBACK
using VOID=void;
using HINSTANCE=HMODULE;
using HRESULT=std::int32_t;
using LPVOID=void*;
using LPUNKNOWN=void*;
struct GUID {std::uint32_t Data1; std::uint16_t Data2,Data3; unsigned char Data4[8];};
using REFIID=const GUID&;
#define SUCCEEDED(hr) ((hr)>=0)
#define FAILED(hr) ((hr)<0)
extern "C" UINT GetSystemDirectoryW(wchar_t*,UINT);
extern "C" BOOL FreeLibrary(HMODULE);
#endif
