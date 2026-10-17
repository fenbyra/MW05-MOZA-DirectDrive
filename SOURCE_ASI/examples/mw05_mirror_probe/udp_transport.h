#ifndef MW05_UDP_TRANSPORT_H
#define MW05_UDP_TRANSPORT_H
// Include BEFORE windows.h to exclude legacy winsock.h. No bind/listen or external address.
#include <winsock2.h>
namespace mw05_udp {
class Transport {
    SOCKET socket_=INVALID_SOCKET;bool started_=false;sockaddr_in to_{};
public:
    int error=0;
    Transport()=default;
    Transport(const Transport&)=delete;Transport& operator=(const Transport&)=delete;
    bool open(unsigned port){
        close();error=0;
        if(port==0 || port>65535){error=WSAEINVAL;return false;}
        WSADATA w{};error=WSAStartup(MAKEWORD(2,2),&w);if(error)return false;
        started_=true;
        if(w.wVersion!=MAKEWORD(2,2)){error=WSAVERNOTSUPPORTED;close();return false;}
        socket_=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(socket_==INVALID_SOCKET){error=WSAGetLastError();close();return false;}
        u_long nonblock=1;
        if(ioctlsocket(socket_,FIONBIO,&nonblock)==SOCKET_ERROR){error=WSAGetLastError();close();return false;}
        to_.sin_family=AF_INET;to_.sin_port=htons(static_cast<unsigned short>(port));
        to_.sin_addr.s_addr=htonl(0x7F000001u); // only 127.0.0.1, never INADDR_ANY
        return true;
    }
    bool send(const unsigned char* bytes,unsigned count){
        if(socket_==INVALID_SOCKET){error=WSAENOTSOCK;return false;}
        const int n=sendto(socket_,reinterpret_cast<const char*>(bytes),static_cast<int>(count),0,
                           reinterpret_cast<const sockaddr*>(&to_),sizeof(to_));
        if(n!=static_cast<int>(count)){error=n==SOCKET_ERROR?WSAGetLastError():WSAEMSGSIZE;return false;}
        error=0;return true;
    }
    void close(){if(socket_!=INVALID_SOCKET){closesocket(socket_);socket_=INVALID_SOCKET;}
        if(started_){WSACleanup();started_=false;}}
    ~Transport(){close();}
};
}
#endif
