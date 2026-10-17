#include "../examples/mw05_mirror_probe/di_button_reader.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
namespace {
IDirectInput8A input; IDirectInputDevice8A device;
unsigned matches=1, instance=0, releases=0, acquires=0;
int failStage=0; bool down=false; unsigned char mask=0; unsigned objects=0; std::wstring loaded;
HWND hwnd=reinterpret_cast<HWND>(1);
HRESULT factory(HINSTANCE,DWORD version,REFIID iid,LPVOID* out,LPUNKNOWN) {
    assert(version==0x800 && iid.Data1==0xBF798030);
    if(failStage==1) return -1;
    *out=&input; return 0;
}
}
extern "C" UINT GetSystemDirectoryW(wchar_t* out,UINT cap) {
    const wchar_t path[]=L"C:\\Windows\\System32";
    assert(cap>sizeof(path)/sizeof(path[0])); std::wcscpy(out,path); return std::wcslen(path);
}
extern "C" HMODULE LoadLibraryExW(const wchar_t* path,HANDLE,DWORD flags) {
    assert(flags==0x800); loaded=path; return reinterpret_cast<HMODULE>(2);
}
extern "C" HMODULE GetModuleHandleA(const char*) {return reinterpret_cast<HMODULE>(3);}
extern "C" FARPROC GetProcAddress(HMODULE,const char* name) {
    assert(std::strcmp(name,"DirectInput8Create")==0);
    return reinterpret_cast<FARPROC>(&factory);
}
extern "C" BOOL FreeLibrary(HMODULE) {return 1;}
HRESULT IDirectInput8A::EnumDevices(DWORD cls,DIEnumCallback cb,VOID* ctx,DWORD flags) {
    assert(cls==4 && flags==1);
    if(failStage==2) return -1;
    DIDEVICEINSTANCEA d{}; std::strcpy(d.tszProductName,"Other device"); cb(&d,ctx);
    std::strcpy(d.tszProductName,"R3 Racing Wheel and Pedals");
    for(unsigned n=0;n<matches;++n) {d.guidInstance.Data1=n+10; cb(&d,ctx);}
    return 0;
}
HRESULT IDirectInput8A::CreateDevice(const GUID&,IDirectInputDevice8A** out,LPUNKNOWN) {
    if(failStage==3) return -1;
    *out=&device; return 0;
}
unsigned long IDirectInput8A::Release(){return 0;}
HRESULT IDirectInputDevice8A::SetDataFormat(const DIDATAFORMAT* f) {
    assert(f->dwSize==sizeof(*f) && f->dwObjSize==sizeof(DIOBJECTDATAFORMAT));
    assert((f->dwNumObjs==1 && f->dwDataSize==4) || (f->dwNumObjs==5 && f->dwDataSize==8));
    objects=f->dwNumObjs;
    assert(f->rgodf->dwOfs==0);
    for (unsigned i=1;i<objects;++i) {
        assert(f->rgodf[i].dwOfs==i && ((f->rgodf[i].dwType>>8)&0xFFFF)==i+3);
        assert((f->rgodf[i].dwType&DIDFT_OPTIONAL)!=0);
    }
    assert(f->rgodf->pguid->Data1==0xA36D02F0);
    instance=(f->rgodf->dwType>>8)&0xFFFF;
    return failStage==4 ? -1:0;
}
HRESULT IDirectInputDevice8A::SetCooperativeLevel(HWND window,DWORD flags) {
    assert(window==hwnd && flags==(2|4)); return failStage==5 ? -1:0;
}
HRESULT IDirectInputDevice8A::Acquire(){++acquires;return failStage==6 ? -1:0;}
HRESULT IDirectInputDevice8A::Unacquire(){return 0;}
unsigned long IDirectInputDevice8A::Release(){++releases;return 0;}
HRESULT IDirectInputDevice8A::Poll(){return failStage==7 ? -1:0;}
HRESULT IDirectInputDevice8A::GetDeviceState(DWORD bytes,LPVOID data) {
    assert(bytes==(objects==5 ? 8:4)); if(failStage==8) return -1;
    static_cast<unsigned char*>(data)[0]=down ? 0x80:0;
    if (objects==5) for(unsigned i=0;i<4;++i)
        static_cast<unsigned char*>(data)[i+1]=(mask&(1u<<i)) ? 0x80:0;
    return 0;
}
int main() {
    unsigned tests=0;
    {mw05_buttons::DirectInputButton r; assert(r.connect(hwnd,37)==0);
     assert(instance==36 && r.connected() && loaded==L"C:\\Windows\\System32\\dinput8.dll");
     bool pressed=true; assert(r.read(hwnd,pressed) && !pressed);
     down=true; assert(r.read(hwnd,pressed) && pressed);
     assert(!r.read(nullptr,pressed) && !pressed); ++tests;
     r.closeDevice(); assert(!r.connected() && releases==1); ++tests;}
    for(unsigned b:{1u,32u,33u,37u,128u}) {
        mw05_buttons::DirectInputButton r; assert(r.connect(hwnd,b)==0 && instance==b-1); ++tests;
    }
    for(int b:{-1,0,129}) {mw05_buttons::DirectInputButton r;assert(r.connect(hwnd,b)==3);++tests;}
    matches=0; {mw05_buttons::DirectInputButton r;assert(r.connect(hwnd,37)==1);++tests;}
    matches=2; {mw05_buttons::DirectInputButton r;assert(r.connect(hwnd,37)==2);++tests;}
    matches=1;
    for(int stage=1;stage<=6;++stage) {
        failStage=stage; mw05_buttons::DirectInputButton r;
        assert(r.connect(hwnd,37)==3 && !r.connected()); ++tests;
    }
    failStage=0;
    for(int stage:{7,8}) {
        mw05_buttons::DirectInputButton r;assert(r.connect(hwnd,37)==0);
        failStage=stage;bool pressed=true;assert(!r.read(hwnd,pressed) && !pressed);
        failStage=0;++tests;
    }
    {mw05_buttons::DirectInputButton r;assert(r.connect(hwnd,37,true)==0 && instance==36);
     for (unsigned m=0;m<16;++m) {
         mask=static_cast<unsigned char>(m);mw05_buttons::ButtonState state;
         assert(r.readState(hwnd,state) && state.directions==m && state.toggle==down);
     } ++tests;}
    std::printf("PASS: %u DirectInput reader mock groups (not Windows ABI/device validation)\n",tests);
}
