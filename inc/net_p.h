#ifdef _WIN32
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
      NETE_Success = 0,
      NETE_InvalidSocket,
      NETE_InvalidAddress,
      NETE_ConnectionRefused,
      NETE_Timedout,
      NETE_Wouldblock,
      NETE_Notconnected,
      NETE_Inprogress,
      NETE_Interrupted,
      NETE_ConnectionAborted,
      NETE_ConnectionReset,
      NETE_AddressInUse,
      NETE_Unknown
    } NetSocketError;

#if defined(WIN32) || defined(WIN64)

    // Error mappings from https://github.com/DFHack/clsocket/blob/master/src/SimpleSocket.cpp#L948
    std::map<int, NetSocketError> gSocketErrors = {
        { EXIT_SUCCESS, NETE_Success },
        { WSAEBADF, NETE_Notconnected },
        { WSAENOTCONN, NETE_Notconnected },
        { WSAEINTR, NETE_Interrupted },
        { WSAECONNREFUSED, NETE_ConnectionRefused },
        { WSAETIMEDOUT, NETE_Timedout },
        { WSAEINPROGRESS, NETE_Inprogress },
        { WSAECONNABORTED, NETE_ConnectionAborted },
        { WSAEWOULDBLOCK, NETE_Wouldblock },
        { WSAENOTSOCK, NETE_InvalidSocket },
        { WSAECONNRESET, NETE_ConnectionReset },
        { WSANO_DATA, NETE_InvalidAddress },
        { WSAEADDRINUSE, NETE_AddressInUse }
    };

    static unsigned int gNetSockCounter = 0;
    int initialize() {
      if (gNetSockCounter == 0) {
        WSADATA wsaData;
        int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (iResult != 0) {
          return iResult;
        }
      }
      gNetSockCounter++;
      return 0;
    }

    int cleanup() {
      gNetSockCounter--;
      if (gNetSockCounter == 0) WSACleanup();
      return 0;
    }

    int setBlocking(SOCKET socket, bool isBlocking) {
      int nCurFlag = isBlocking? 0: 1;
      return ioctlsocket(socket, FIONBIO, (ULONG *)&nCurFlag);
    }

    NetSocketError lastError() {
      int err = WSAGetLastError();

      std::map<int, NetSocketError>::iterator it;
      it = gSocketErrors.find(err);
      if (it != gSocketErrors.end())
        return it->second;
      return NETE_Unknown;
    }

    std::string lastErrorString() {
      static char message[256] = {0};
      FormatMessage(
          FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
          0, WSAGetLastError(), 0, message, 256, 0);
      return std::string(message);
    }

    int close(SOCKET sock) {
      int iResult = closesocket(sock);
      return iResult;
    }
#else

    // Error mappings from https://github.com/DFHack/clsocket/blob/master/src/SimpleSocket.cpp#L948
    std::map<int, NetSocketError> gSocketErrors = {
        { EXIT_SUCCESS, NETE_Success },
        { ENOTCONN, NETE_Notconnected },
        { EINTR, NETE_Interrupted },
        { ECONNREFUSED , NETE_ConnectionRefused },
        { ETIMEDOUT, NETE_Timedout },
        { EINPROGRESS, NETE_Inprogress },
        { ECONNABORTED, NETE_ConnectionAborted },
        { EWOULDBLOCK, NETE_Wouldblock },
        { ECONNRESET, NETE_ConnectionReset },
        { EADDRINUSE, NETE_AddressInUse }
    };

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

    struct addrinfo* addressinfo(const char* address, const char* port) {
      struct addrinfo *result = nullptr, *ptr = nullptr, hints{};

      ZeroMemory( &hints, sizeof(hints) );
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_protocol = IPPROTO_TCP;

      // Resolve the server address and port
      int iResult = getaddrinfo(address, port, &hints, &result);
      if ( iResult != 0 ) {
        return nullptr;
      }

      return result;
    }

    int connect(SOCKET& sock, const char* address, const char* port) {
      struct addrinfo *result = nullptr, *ptr = nullptr;
      result = addressinfo(address, port);
      if ( result == nullptr ) {
        return NETE_SocketError;
      }

      // Attempt to connect to an address until one succeeds
      for(ptr=result; ptr != nullptr ;ptr=ptr->ai_next) {

        // Create a SOCKET for connecting to server
        sock = socket(ptr->ai_family, ptr->ai_socktype,
                      ptr->ai_protocol);
        if (sock == INVALID_SOCKET) {
          continue;
        }

        // Connect to server.
        int iResult = ::connect( sock, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == NETE_SocketError) {
          close(sock);
          sock = INVALID_SOCKET;
          continue;
        }
        break;
      }

      freeaddrinfo(result);

      if (sock == INVALID_SOCKET) {
        return NETE_SocketError;
      }
      return 0;
    }

    int setsockopt(SOCKET sock, int opt, const void* val, int size) {
      const char* s = (const char *)val;
      return ::setsockopt(sock, SOL_SOCKET, opt, s, size);
    }

    int send(SOCKET sock, const char* buffer) {
      int iResult = ::send( sock, buffer, (int)strlen(buffer), 0 );
      return iResult;
    }

    int recv(SOCKET sock, char* buffer, int len) {
      int iResult = ::recv(sock, buffer, len, 0);
      return iResult;
    }

    int shutdown(SOCKET sock, char c) {
      int iResult = ::shutdown(sock, c);
      if (iResult == NETE_SocketError) {
        closesocket(sock);
        WSACleanup();
        return -1;
      }
      return iResult;
    }

    int listen(SOCKET& sock, const char* address, const char* port) {
      struct addrinfo *result = nullptr, *ptr = nullptr;
      result = addressinfo(address, port);
      if ( result == nullptr ) {
        return lastError();
      }

      sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
      if (sock == INVALID_SOCKET) {
        freeaddrinfo(result);
        return lastError();
      }

      // Setup the TCP listening socket
      int iResult = ::bind( sock, result->ai_addr, (int)result->ai_addrlen);
      if (iResult == NETE_SocketError) {
        freeaddrinfo(result);
        close(sock);
        return lastError();
      }

      freeaddrinfo(result);

      iResult = ::listen(sock, SOMAXCONN);
      if (iResult == NETE_SocketError) {
        close(sock);
        return lastError();
      }

      return 0;
    }

    SOCKET accept(SOCKET sock) {
      // Accept a client socket
      SOCKET cliSocket = ::accept(sock, nullptr, nullptr);
      if (cliSocket == INVALID_SOCKET) {
        close(sock);
      }
      return cliSocket;
    }
  }

}

#endif //NetLib_NET_WIN_H
#endif // _WIN32