#ifndef QA_WINDOWS_H
#define QA_WINDOWS_H
#include <cstddef>
using BOOL = int;
using LONG = long;
using DWORD = unsigned long;
using UINT = unsigned int;
using SIZE_T = std::size_t;
using HANDLE = void*;
using HWND = void*;
using HMODULE = void*;
using PVOID = void*;
using FARPROC = void (*)();
struct SRWLOCK { void* value; };
#define SRWLOCK_INIT {nullptr}
#define VK_F8 0x77
#define VK_ESCAPE 0x1B
#define VK_PAUSE 0x13
#define VK_UP 0x26
#define VK_DOWN 0x28
#define MAX_PATH 260
#define PAGE_EXECUTE_READWRITE 0x40
#define LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR 0x100
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x1000
#define WINAPI
using LPTHREAD_START_ROUTINE = DWORD (*)(void*);
extern "C" {
void AcquireSRWLockExclusive(SRWLOCK*);
void ReleaseSRWLockExclusive(SRWLOCK*);
HANDLE GetCurrentProcess();
BOOL ReadProcessMemory(HANDLE, const void*, void*, SIZE_T, SIZE_T*);
BOOL WriteProcessMemory(HANDLE, void*, const void*, SIZE_T, SIZE_T*);
DWORD GetTickCount();
void OutputDebugStringA(const char*);
HWND GetForegroundWindow();
DWORD GetWindowThreadProcessId(HWND, DWORD*);
DWORD GetCurrentProcessId();
BOOL VirtualProtect(void*, SIZE_T, DWORD, DWORD*);
DWORD GetLastError();
void* InterlockedCompareExchangePointer(void* volatile*, void*, void*);
LONG InterlockedCompareExchange(volatile LONG*, LONG, LONG);
LONG InterlockedExchange(volatile LONG*, LONG);
short GetAsyncKeyState(int);
DWORD GetModuleFileNameW(HMODULE, wchar_t*, DWORD);
UINT GetPrivateProfileIntW(const wchar_t*, const wchar_t*, int, const wchar_t*);
HMODULE GetModuleHandleA(const char*);
HMODULE LoadLibraryExW(const wchar_t*, HANDLE, DWORD);
FARPROC GetProcAddress(HMODULE, const char*);
HANDLE CreateThread(void*, SIZE_T, LPTHREAD_START_ROUTINE, void*, DWORD, DWORD*);
BOOL CloseHandle(HANDLE);
void Sleep(DWORD);
}
#endif
