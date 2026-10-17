#ifndef QA_WINSOCK2_H
#define QA_WINSOCK2_H
#include <cstdint>
#include <cstring>
#include <vector>
using SOCKET=std::uintptr_t;using u_long=unsigned long;
constexpr SOCKET INVALID_SOCKET=static_cast<SOCKET>(-1);
constexpr int SOCKET_ERROR=-1,AF_INET=2,SOCK_DGRAM=2,IPPROTO_UDP=17;
constexpr int WSAEINVAL=10022,WSAVERNOTSUPPORTED=10092,WSAENOTSOCK=10038,WSAEMSGSIZE=10040;
constexpr unsigned long FIONBIO=0x8004667e;
#define MAKEWORD(a,b) (static_cast<unsigned short>(((b)<<8)|(a)))
struct WSADATA {unsigned short wVersion=0;};
struct in_addr {std::uint32_t s_addr=0;};
struct sockaddr {unsigned short sa_family;char sa_data[14];};
struct sockaddr_in {short sin_family=0;unsigned short sin_port=0;in_addr sin_addr{};char pad[8]{};};
namespace qa_udp {
inline int startupResult=0,lastError=10035,ioctlResult=0,sendResult=324;
inline bool badVersion=false,badSocket=false;
inline unsigned startups=0,cleanups=0,closes=0,sends=0,ioctls=0;
inline sockaddr_in destination{};inline std::vector<unsigned char> payload;
inline void reset(){startupResult=ioctlResult=0;lastError=10035;sendResult=324;badVersion=badSocket=false;
    startups=cleanups=closes=sends=ioctls=0;destination={};payload.clear();}
}
inline std::uint32_t htonl(std::uint32_t n){return ((n&255)<<24)|((n&0xff00)<<8)|((n>>8)&0xff00)|(n>>24);}
inline unsigned short htons(unsigned short n){return static_cast<unsigned short>((n<<8)|(n>>8));}
inline int WSAStartup(unsigned short v,WSADATA* w){++qa_udp::startups;w->wVersion=qa_udp::badVersion?0:v;return qa_udp::startupResult;}
inline int WSACleanup(){++qa_udp::cleanups;return 0;}
inline int WSAGetLastError(){return qa_udp::lastError;}
inline SOCKET socket(int,int,int){return qa_udp::badSocket?INVALID_SOCKET:42;}
inline int closesocket(SOCKET){++qa_udp::closes;return 0;}
inline int ioctlsocket(SOCKET,unsigned long command,u_long* n){++qa_udp::ioctls;
    return command==FIONBIO && *n==1?qa_udp::ioctlResult:-1;}
inline int sendto(SOCKET,const char* b,int n,int,const sockaddr* to,int){++qa_udp::sends;
    qa_udp::destination=*reinterpret_cast<const sockaddr_in*>(to);
    qa_udp::payload.assign(b,b+n);return qa_udp::sendResult;}
#endif
