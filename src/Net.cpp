#include <Net.h>

thisptr::net::BlockingTcpSocket::~BlockingTcpSocket() {
  if (m_sock != -1)
    thisptr::net_p::close(m_sock);
  thisptr::net_p::cleanup();
}

thisptr::net::BlockingTcpSocket::BlockingTcpSocket(unsigned long long int sock) : m_sock(sock) {
  thisptr::net_p::initialize();
}

bool thisptr::net::BlockingTcpSocket::connect(const std::string& address, const std::string& port) {
  int res = thisptr::net_p::connect(m_sock, address.c_str(), port.c_str());
  return res != thisptr::net_p::NETE_SocketError;
}

int thisptr::net::BlockingTcpSocket::recv(char *buf, int len) {
  int iRes = thisptr::net_p::recv(m_sock, buf, len);
  if ( iRes < 0 ) {
    thisptr::net_p::NetSocketError err = thisptr::net_p::lastError();
    if (err == thisptr::net_p::NETE_Wouldblock)
      return thisptr::net_p::NETE_Success;
    close();
    return err;
  } else if ( iRes == 0 )
    return thisptr::net_p::NETE_Notconnected;
  return iRes;
}

int thisptr::net::BlockingTcpSocket::send(const char *buf) {
  int iRes = thisptr::net_p::send(m_sock, buf, strlen(buf));
  if (iRes == thisptr::net_p::NETE_SocketError) {
    close();
  }
  return iRes;
}

int thisptr::net::BlockingTcpSocket::send(const char *buf, int len) {
  int iRes = thisptr::net_p::send(m_sock, buf, len);
  if (iRes == thisptr::net_p::NETE_SocketError) {
    close();
  }
  return iRes;
}

bool thisptr::net::BlockingTcpSocket::close() {
  if (thisptr::net_p::close(m_sock) == 0) {
    m_sock = -1;
    return true;
  }
  return false;
}

bool thisptr::net::BlockingTcpSocket::bind(const std::string &address, const std::string &port) {
  int err = thisptr::net_p::listen(m_sock, address.c_str(), port.c_str());
  bool bRes = (err == thisptr::net_p::NETE_Success && m_sock != INVALID_SOCKET);

  return bRes;
}

std::shared_ptr<thisptr::net::BlockingTcpSocket> thisptr::net::BlockingTcpSocket::accept() {
  unsigned long long sock = thisptr::net_p::accept(m_sock);
  if (sock == INVALID_SOCKET || thisptr::net_p::lastError() == thisptr::net_p::NETE_Wouldblock)
    return nullptr;

  return std::make_shared<BlockingTcpSocket>(sock);
}

void thisptr::net::ConnectionHandlerBase::operator()() {
  onConnect();
  while(true) {
    char buffer[256] = {0};
    int res = m_conn->recv(buffer, 256);
    if (res < 0 && res != thisptr::net_p::NETE_Notconnected) {
      std::cout << " : error occured, res: " << res << std::endl;
      break;
    } else if (res == thisptr::net_p::NETE_Notconnected) {
      std::cout << " : connection closed" << std::endl;
      break;
    } else {
      std::string data(buffer, res);
      onMessage(data);
    }
  }
  onDisconnect();
}

void thisptr::net::ConnectionHandlerBase::setTcpServer(thisptr::net::TcpServerBase *server) {
  m_server = server;
}

void thisptr::net::ConnectionHandlerBase::setTcpConn(std::shared_ptr<net::BlockingTcpSocket>& conn) {
  m_conn = conn;
}

void thisptr::net::ConnectionHandlerBase::onConnect() {
}

void thisptr::net::ConnectionHandlerBase::onDisconnect() {
}

void thisptr::net::ConnectionHandlerBase::onMessage(std::string data) {
  std::cerr << "if you can read this, it means you should think about handling connections!" << std::endl;
}
