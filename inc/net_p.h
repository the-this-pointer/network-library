#ifndef NetLib_NET_WIN_H
#define NetLib_NET_WIN_H

#if defined(WIN32) || defined(WIN64)
  #include <ws2spi.h>
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netinet/tcp.h>
  #include <netinet/ip.h>
  #include <netdb.h>
#endif

#include <map>
#include <string>

namespace thisptr {

  namespace net_p {

    typedef enum
    {
      NETE_SocketError = -1,
      NETE_InvalidSocket = -2,
      NETE_InvalidAddress = -3,
      NETE_ConnectionRefused = -4,
      NETE_Timedout = -5,
      NETE_Wouldblock = -6,
      NETE_Notconnected = -7,
      NETE_Inprogress = -8,
      NETE_Interrupted = -9,
      NETE_ConnectionAborted = -10,
      NETE_ConnectionReset = -11,
      NETE_AddressInUse = -12,
      NETE_Unknown = -13,
      NETE_Success = 0,
    } NetSocketError;
    extern std::map<int, NetSocketError> gSocketErrors;

#if defined(WIN32) || defined(WIN64)
    static unsigned int gNetSockCounter = 0;
    int initialize();
    int cleanup();
    int setBlocking(SOCKET socket, bool isBlocking);
    NetSocketError lastError();
    std::string lastErrorString();
    int close(SOCKET sock);
#else
    int initialize() { return 0; }
    int cleanup() { return 0; }

    int setBlocking(SOCKET socket, bool isBlocking) {
      int nCurFlag;
      if ((nCurFlag = fcntl(m_socket, F_GETFL)) < 0)
      {
          return nCurFlag;
      }

      if (isBlocking)
        nCurFlag &= (~O_NONBLOCK);
      else
        nCurFlag |= O_NONBLOCK;

      return fcntl(m_socket, F_SETFL, nCurFlag);
    }

    NetSocketError lastError() {
      int err = errno;

      std::map<int, NetSocketError>::iterator it;
      it = gSocketErrors.find(err);
      if (it != gSocketErrors.end())
        return it->second;
      return NETE_Unknown;
    }

    std::string lastErrorString() {
      return std::string(strerror(errno));
    }

    int close(SOCKET sock) {
      int iResult = ::close(sock);
      return iResult;
    }
#endif

    struct addrinfo* addressinfo(const char* address, const char* port);
    int connect(SOCKET& sock, const char* address, const char* port);
    int setsockopt(SOCKET sock, int opt, const void* val, int size);
    int send(SOCKET sock, const char* buffer);
    int recv(SOCKET sock, char* buffer, int len);
    int shutdown(SOCKET sock, char c);
    int listen(SOCKET& sock, const char* address, const char* port);
    SOCKET accept(SOCKET sock);
  }

}

#endif //NetLib_NET_WIN_H
