#ifndef QA_NFSMW_HOOKS_H
#define QA_NFSMW_HOOKS_H
// HOST mocks only. No Windows trampoline or thread-suspension implementation.
typedef int MH_STATUS;
constexpr MH_STATUS MH_OK = 0;
extern "C" {
int nfsmw__mh_ensure_init(void);
MH_STATUS MH_CreateHook(void*, void*, void**);
MH_STATUS MH_EnableHook(void*);
MH_STATUS MH_RemoveHook(void*);
}
#endif