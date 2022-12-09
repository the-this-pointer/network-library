#include <net_p.h>

// Error mappings from https://github.com/DFHack/clsocket/blob/master/src/SimpleSocket.cpp#L948
#if defined(WIN32) || defined(WIN64)
std::map<int, thisptr::net_p::NetSocketError> thisptr::net_p::gSocketErrors = {
    { EXIT_SUCCESS, thisptr::net_p::NETE_Success },
    { WSAEBADF, thisptr::net_p::NETE_Notconnected },
    { WSAENOTCONN, thisptr::net_p::NETE_Notconnected },
    { WSAEINTR, thisptr::net_p::NETE_Interrupted },
    { WSAECONNREFUSED, thisptr::net_p::NETE_ConnectionRefused },
    { WSAETIMEDOUT, thisptr::net_p::NETE_Timedout },
    { WSAEINPROGRESS, thisptr::net_p::NETE_Inprogress },
    { WSAECONNABORTED, thisptr::net_p::NETE_ConnectionAborted },
    { WSAEWOULDBLOCK, thisptr::net_p::NETE_Wouldblock },
    { WSAENOTSOCK, thisptr::net_p::NETE_InvalidSocket },
    { WSAECONNRESET, thisptr::net_p::NETE_ConnectionReset },
    { WSANO_DATA, thisptr::net_p::NETE_InvalidAddress },
    { WSAEADDRINUSE, thisptr::net_p::NETE_AddressInUse }
};
#else
    std::map<int, NetSocketError> thisptr::net_p::gSocketErrors = {
        { EXIT_SUCCESS, thisptr::net_p::NETE_Success },
        { ENOTCONN, thisptr::net_p::NETE_Notconnected },
        { EINTR, thisptr::net_p::NETE_Interrupted },
        { ECONNREFUSED , thisptr::net_p::NETE_ConnectionRefused },
        { ETIMEDOUT, thisptr::net_p::NETE_Timedout },
        { EINPROGRESS, thisptr::net_p::NETE_Inprogress },
        { ECONNABORTED, thisptr::net_p::NETE_ConnectionAborted },
        { EWOULDBLOCK, thisptr::net_p::NETE_Wouldblock },
        { ECONNRESET, thisptr::net_p::NETE_ConnectionReset },
        { EADDRINUSE, thisptr::net_p::NETE_AddressInUse }
    };
#endif

int thisptr::net_p::initialize() {
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

int thisptr::net_p::cleanup() {
  gNetSockCounter--;
  if (gNetSockCounter == 0) WSACleanup();
  return 0;
}

int thisptr::net_p::setBlocking(SOCKET socket, bool isBlocking) {
  int nCurFlag = isBlocking? 0: 1;
  return ioctlsocket(socket, FIONBIO, (ULONG *)&nCurFlag);
}

thisptr::net_p::NetSocketError thisptr::net_p::lastError() {
  int err = WSAGetLastError();

  std::map<int, NetSocketError>::iterator it;
  it = gSocketErrors.find(err);
  if (it != gSocketErrors.end())
    return it->second;
  return NETE_Unknown;
}

std::string thisptr::net_p::lastErrorString() {
  static char message[256] = {0};
  FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, WSAGetLastError(), 0, message, 256, 0);
  return {message};
}

int thisptr::net_p::close(SOCKET sock) {
  int iResult = closesocket(sock);
  return iResult;
}

struct addrinfo *thisptr::net_p::addressinfo(const char *address, const char *port) {
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

int thisptr::net_p::connect(SOCKET &sock, const char *address, const char *port) {
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

int thisptr::net_p::setsockopt(SOCKET sock, int opt, const void *val, int size) {
  const char* s = (const char *)val;
  return ::setsockopt(sock, SOL_SOCKET, opt, s, size);
}

int thisptr::net_p::send(SOCKET sock, const char *buffer, int len) {
  int iResult = ::send( sock, buffer, len, 0 );
  return iResult;
}

int thisptr::net_p::recv(SOCKET sock, char *buffer, int len) {
  int iResult = ::recv(sock, buffer, len, 0);
  return iResult;
}

int thisptr::net_p::shutdown(SOCKET sock, char c) {
  int iResult = ::shutdown(sock, c);
  if (iResult == NETE_SocketError) {
    closesocket(sock);
    WSACleanup();
    return -1;
  }
  return iResult;
}

int thisptr::net_p::listen(SOCKET &sock, const char *address, const char *port) {
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

SOCKET thisptr::net_p::accept(SOCKET sock) {
  // Accept a client socket
  SOCKET cliSocket = ::accept(sock, nullptr, nullptr);
  if (cliSocket == INVALID_SOCKET) {
    close(sock);
  }
  return cliSocket;
}
