#ifndef QA_NFSMW_INPUT_H
#define QA_NFSMW_INPUT_H
namespace nfsmw { namespace input {
template<class F> inline bool on_poll(F&& fn) { (void)fn; return true; }
}}
#endif
