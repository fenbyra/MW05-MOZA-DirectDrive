#ifndef MW05_DI_BUTTON_READER_H
#define MW05_DI_BUTTON_READER_H
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <windows.h>
#include <dinput.h>
#include <cwchar>
#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif
namespace mw05_buttons {
// Uses system DirectInput, NOT MOZA SDK button-array indices or joyGetPosEx's
// 32-bit button mask. No driver calls on poll/getter/watchdog threads.
struct ButtonState { bool toggle=false; unsigned char directions=0; };
class DirectInputButton {
    bool menu_ = false;
    HMODULE module_ = nullptr;
    IDirectInput8A* input_ = nullptr;
    IDirectInputDevice8A* device_ = nullptr;
    HWND window_ = nullptr;
    GUID selected_{};
    unsigned matches_ = 0;
    static bool equalName(const char* a, const char* b) {
        while (*a && *b) {
            char x=*a++, y=*b++;
            if (x>='A' && x<='Z') x=static_cast<char>(x-'A'+'a');
            if (y>='A' && y<='Z') y=static_cast<char>(y-'A'+'a');
            if (x!=y) return false;
        }
        return *a==*b;
    }
    static BOOL CALLBACK enumerate(const DIDEVICEINSTANCEA* instance, VOID* context) {
        auto* self=static_cast<DirectInputButton*>(context);
        if (equalName(instance->tszProductName,"R3 Racing Wheel and Pedals") ||
            equalName(instance->tszInstanceName,"R3 Racing Wheel and Pedals")) {
            self->selected_=instance->guidInstance;
            ++self->matches_;
        }
        return DIENUM_CONTINUE;
    }
    bool initialize() {
        if (input_) return true;
        if (!module_) {
            wchar_t path[MAX_PATH]{};
            const UINT length=GetSystemDirectoryW(path,MAX_PATH);
            const wchar_t suffix[]=L"\\dinput8.dll";
            if (!length || length >= MAX_PATH ||
                length+sizeof(suffix)/sizeof(suffix[0]) > MAX_PATH) return false;
            std::wmemcpy(path+length,suffix,sizeof(suffix)/sizeof(suffix[0]));
            // Absolute OS directory; never the game's dinput8 ASI-loader proxy.
            module_=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!module_) return false;
        }
        using Factory=HRESULT (WINAPI *)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
        const auto factory=reinterpret_cast<Factory>(GetProcAddress(module_,"DirectInput8Create"));
        if (!factory) return false;
        const GUID iid={0xBF798030,0x483A,0x4DA2,{0xAA,0x99,0x5D,0x64,0xED,0x36,0x97,0x00}};
        return SUCCEEDED(factory(GetModuleHandleA(nullptr),DIRECTINPUT_VERSION,iid,
                          reinterpret_cast<LPVOID*>(&input_),nullptr)) && input_;
    }
public:
    DirectInputButton() = default;
    DirectInputButton(const DirectInputButton&) = delete;
    DirectInputButton& operator=(const DirectInputButton&) = delete;
    ~DirectInputButton() {
        closeDevice();
        if (input_) input_->Release();
        if (module_) FreeLibrary(module_);
    }
    bool connected() const { return device_ != nullptr; }
    void closeDevice() {
        if (device_) { device_->Unacquire(); device_->Release(); }
        device_=nullptr; window_=nullptr;
    }
    // 0 ready, 1 absent, 2 ambiguous, 3 driver/setup error. Never choose an
    // arbitrary controller if multiple matching R3 devices are enumerated.
    int connect(HWND window, int buttonNumber, bool menu=false) {
        closeDevice();
        if (!window || buttonNumber<1 || buttonNumber>128 || !initialize()) return 3;
        matches_=0;
        if (FAILED(input_->EnumDevices(DI8DEVCLASS_GAMECTRL,&enumerate,this,DIEDFL_ATTACHEDONLY))) return 3;
        if (matches_==0) return 1;
        if (matches_!=1) return 2;
        if (FAILED(input_->CreateDevice(selected_,&device_,nullptr)) || !device_) return 3;
        const GUID buttonGuid={0xA36D02F0,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};
        menu_=menu;
        DIOBJECTDATAFORMAT objects[5]{};
        objects[0].pguid=&buttonGuid;
        objects[0].dwOfs=0;
        objects[0].dwType=DIDFT_BUTTON|DIDFT_MAKEINSTANCE(static_cast<DWORD>(buttonNumber-1));
        for (unsigned i=0;i<4;++i) {
            objects[i+1].pguid=&buttonGuid;
            objects[i+1].dwOfs=i+1;
            objects[i+1].dwType=DIDFT_BUTTON|DIDFT_MAKEINSTANCE(i+4)|DIDFT_OPTIONAL;
        }
        DIDATAFORMAT format{};
        format.dwSize=sizeof(format); format.dwObjSize=sizeof(objects[0]);
        format.dwFlags=DIDF_ABSAXIS; format.dwDataSize=menu ? 8:4; format.dwNumObjs=menu ? 5:1; format.rgodf=objects;
        if (FAILED(device_->SetDataFormat(&format)) ||
            FAILED(device_->SetCooperativeLevel(window,DISCL_NONEXCLUSIVE|DISCL_FOREGROUND)) ||
            FAILED(device_->Acquire())) { closeDevice(); return 3; }
        window_=window;
        return 0;
    }
    bool readState(HWND window, ButtonState& result) {
        result=ButtonState{};
        if (!device_ || window!=window_) return false;
        if (FAILED(device_->Poll())) return false;
        unsigned char state[8]{};
        if (FAILED(device_->GetDeviceState(menu_ ? 8:4,state))) return false;
        result.toggle=(state[0]&0x80u)!=0;
        if (menu_) for (unsigned i=0;i<4;++i)
            if ((state[i+1]&0x80u)!=0) result.directions|=static_cast<unsigned char>(1u<<i);
        return true;
    }
    bool read(HWND window, bool& down) {
        ButtonState state{};
        const bool valid=readState(window,state);
        down=valid && state.toggle;
        return valid;
    }
};
}
#endif
