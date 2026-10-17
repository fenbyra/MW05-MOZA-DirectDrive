#ifndef MW05_MOZA_BRIDGE_API_H
#define MW05_MOZA_BRIDGE_API_H

// The ONLY interface shared by the MSVC DLL and the MinGW ASI.
// No SDK headers, C++ classes, STL objects, allocation, or exceptions cross it.
#include <stdint.h>

#if defined(_WIN32)
#define MW05_MOZA_CALL __cdecl
#else
#define MW05_MOZA_CALL
#endif

#define MW05_MOZA_ABI 0x00060000u
enum {
    MW05_MOZA_OK = 0,
    MW05_MOZA_BAD_ARGUMENT = -1,
    MW05_MOZA_NOT_INITIALIZED = -2,
    MW05_MOZA_SDK_ERROR = -3,
    MW05_MOZA_NULL_DATA = -4,
    MW05_MOZA_EXCEPTION = -5,
    MW05_MOZA_BAD_ANGLE = -6,
    MW05_MOZA_NOT_LOADED = -7,
    MW05_MOZA_SLOW_READ = -8
};

#pragma pack(push, 4)
typedef struct MW05MozaSample {
    uint32_t abi;
    uint32_t bytes;
    uint32_t readStartedTick; // GetTickCount BEFORE getHIDData, not after it.
    int32_t sdkError;
    float angleDegrees;
    int16_t wheelAxle;       // Diagnostic only; NOT used to normalize steering.
    int16_t throttle;
    int16_t brake;
    int16_t reserved;
} MW05MozaSample;
#pragma pack(pop)

typedef int32_t (MW05_MOZA_CALL *MW05MozaInitFn)(uint32_t abi, uint32_t bytes);
typedef int32_t (MW05_MOZA_CALL *MW05MozaReadFn)(MW05MozaSample*, uint32_t bytes);

#ifdef __cplusplus
#include <cstddef>
static_assert(sizeof(MW05MozaSample) == 28, "Bridge ABI size");
static_assert(offsetof(MW05MozaSample, angleDegrees) == 16, "Bridge ABI angle");
static_assert(offsetof(MW05MozaSample, throttle) == 22, "Bridge ABI throttle");
extern "C" {
#endif
int32_t MW05_MOZA_CALL MW05Moza_Init(uint32_t abi, uint32_t bytes);
int32_t MW05_MOZA_CALL MW05Moza_Read(MW05MozaSample* out, uint32_t bytes);
#ifdef __cplusplus
}
#endif
#endif
