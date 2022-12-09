#include <Net.h>

thisptr::net::TcpSocket::~TcpSocket() {
  if (m_sock != -1)
    thisptr::net_p::close(m_sock);
  thisptr::net_p::cleanup();
}

thisptr::net::TcpSocket::TcpSocket(unsigned long long int sock) : m_sock(sock) {
  thisptr::net_p::initialize();
}

int thisptr::net::TcpSocket::recv(char *buf, int len) {
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

int thisptr::net::TcpSocket::send(const char *buf) {
  int iRes = thisptr::net_p::send(m_sock, buf);
  if (iRes == thisptr::net_p::NETE_SocketError) {
    close();
  }
  return iRes;
}

bool thisptr::net::TcpSocket::close() {
  if (thisptr::net_p::close(m_sock) == 0) {
    m_sock = -1;
    return true;
  }
  return false;
}

bool thisptr::net::TcpSocket::setTimeout(int opt, int val) {
  struct timeval timeout{};
  timeout.tv_sec = val;
  timeout.tv_usec = 0;

  return setOpt(opt, &timeout, sizeof(timeout));
}

bool thisptr::net::TcpSocket::setTimeout(unsigned long long int sock, int opt, int val) {
  struct timeval timeout;
  timeout.tv_sec = val;
  timeout.tv_usec = 0;

  return setOpt(sock, opt, &timeout, sizeof(timeout));
}

bool thisptr::net::TcpSocket::setOpt(unsigned long long int sock, int opt, const void *val, int size) {
  return thisptr::net_p::setsockopt(sock, opt, val, size) == 0;
}

bool thisptr::net::TcpSocket::setOpt(int opt, const void *val, int size) {
  return thisptr::net_p::setsockopt(m_sock, opt, val, size) == 0;
}

bool thisptr::net::TcpClient::connect(const std::string &address, const std::string &port) {
  m_address = address;
  m_port = port;
  int res = thisptr::net_p::connect(m_sock, m_address.c_str(), m_port.c_str());
  if (res != thisptr::net_p::NETE_SocketError) {
    setTimeout(SO_RCVTIMEO, 5000);
    setTimeout(SO_SNDTIMEO, 5000);
    return true;
  }
  return false;
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
      std::string data(buffer, 256);
      onMessage(data);
    }
  }
  onDisconnect();
}

void thisptr::net::ConnectionHandlerBase::setTcpServer(thisptr::net::TcpServerBase *server) {
  m_server = server;
}

void thisptr::net::ConnectionHandlerBase::setTcpConn(std::shared_ptr<net::TcpSocket>& conn) {
  m_conn = conn;
}

void thisptr::net::ConnectionHandlerBase::onConnect() {
}

void thisptr::net::ConnectionHandlerBase::onDisconnect() {
}

void thisptr::net::ConnectionHandlerBase::onMessage(std::string data) {
  std::cerr << "if you can read this, it means you should think about handling connections!" << std::endl;
}
