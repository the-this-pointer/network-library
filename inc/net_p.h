#ifdef _WIN32
#ifndef NetLib_NET_WIN_H
#define NetLib_NET_WIN_H

#include <ws2spi.h>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace thisptr {

  namespace net_p {
    bool gNetInitialized = false;
    int initialize() {
      WSADATA wsaData;
      int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
      if (iResult == 0) {
        thisptr::net_p::gNetInitialized = true;
      }
      return iResult;
    }

    int cleanup() {
      WSACleanup();
      return 0;
    }

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
        return -1;
      }

      // Attempt to connect to an address until one succeeds
      for(ptr=result; ptr != nullptr ;ptr=ptr->ai_next) {

        // Create a SOCKET for connecting to server
        sock = socket(ptr->ai_family, ptr->ai_socktype,
                      ptr->ai_protocol);
        if (sock == INVALID_SOCKET) {
          return -1;
        }

        // Connect to server.
        int iResult = ::connect( sock, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
          closesocket(sock);
          WSACleanup();
          sock = INVALID_SOCKET;
          continue;
        }
        break;
      }

      freeaddrinfo(result);

      if (sock == INVALID_SOCKET) {
        return -1;
      }
      return 0;
    }

    int setsockopt(SOCKET sock, int opt, const void* val, int size) {
      const char* s = (const char *)val;
      return ::setsockopt(sock, SOL_SOCKET, opt, s, size);
    }

    int send(SOCKET sock, const char* buffer) {
      int iResult = ::send( sock, buffer, (int)strlen(buffer), 0 );
      if (iResult == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        return -1;
      }
      return iResult;
    }

    int recv(SOCKET sock, char* buffer, int len) {
      int iResult = 0;
      do {
        iResult = ::recv(sock, buffer, len, 0);
        if ( iResult < 0 ) {
          if (WSAGetLastError() == WSAETIMEDOUT)
            return ETIMEDOUT;
          closesocket(sock);
          WSACleanup();
          return -1; // error occured, check WSAGetLastError()
        } else if ( iResult == 0 )
          return 0; //Connection closed

      } while( 0 );
      return iResult;
    }

    int shutdown(SOCKET sock, char c) {
      int iResult = ::shutdown(sock, c);
      if (iResult == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        return -1;
      }
      return iResult;
    }

    int close(SOCKET sock) {
      int iResult = closesocket(sock);
      return iResult;
    }

    int listen(SOCKET& sock, const char* address, const char* port) {
      struct addrinfo *result = nullptr, *ptr = nullptr;
      result = addressinfo(address, port);
      if ( result == nullptr ) {
        return -1;
      }

      sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
      if (sock == INVALID_SOCKET) {
        freeaddrinfo(result);
        WSACleanup();
        return -1;
      }

      // Setup the TCP listening socket
      int iResult = ::bind( sock, result->ai_addr, (int)result->ai_addrlen);
      if (iResult == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(sock);
        WSACleanup();
        return 1;
      }

      freeaddrinfo(result);

      iResult = ::listen(sock, SOMAXCONN);
      if (iResult == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        return 1;
      }

      return 0;
    }

    SOCKET accept(SOCKET sock) {
      // Accept a client socket
      SOCKET cliSocket = ::accept(sock, nullptr, nullptr);
      if (cliSocket == INVALID_SOCKET) {
        closesocket(sock);
        WSACleanup();
        return INVALID_SOCKET;
      }
      return cliSocket;
    }
  }

}

#endif //NetLib_NET_WIN_H
#endif // _WIN32