#include <Net.h>

using namespace thisptr::net;

BlockingTcpSocket::~BlockingTcpSocket() {
  if (m_sock != -1)
    thisptr::net_p::close(m_sock);
  thisptr::net_p::cleanup();
}

BlockingTcpSocket::BlockingTcpSocket(unsigned long long int sock) : m_sock(sock) {
  thisptr::net_p::initialize();
}

bool BlockingTcpSocket::connect(const std::string& address, const std::string& port) {
  int res = thisptr::net_p::connect(m_sock, address.c_str(), port.c_str());
  return res != thisptr::net_p::NETE_SocketError;
}

int BlockingTcpSocket::recv(char *buf, int len) {
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

int BlockingTcpSocket::send(const char *buf) {
  int iRes = thisptr::net_p::send(m_sock, buf, strlen(buf));
  if (iRes == thisptr::net_p::NETE_SocketError) {
    close();
  }
  return iRes;
}

int BlockingTcpSocket::send(const char *buf, int len) {
  int iRes = thisptr::net_p::send(m_sock, buf, len);
  if (iRes == thisptr::net_p::NETE_SocketError) {
    close();
  }
  return iRes;
}

bool BlockingTcpSocket::close() {
  if (thisptr::net_p::close(m_sock) == 0) {
    m_sock = -1;
    return true;
  }
  return false;
}

bool BlockingTcpSocket::bind(const std::string &address, const std::string &port) {
  int err = thisptr::net_p::listen(m_sock, address.c_str(), port.c_str());
  bool bRes = (err == thisptr::net_p::NETE_Success && m_sock != INVALID_SOCKET);

  return bRes;
}

std::shared_ptr<BlockingTcpSocket> BlockingTcpSocket::accept() {
  unsigned long long sock = thisptr::net_p::accept(m_sock);
  if (sock == INVALID_SOCKET || thisptr::net_p::lastError() == thisptr::net_p::NETE_Wouldblock)
    return nullptr;

  return std::make_shared<BlockingTcpSocket>(sock);
}

void BlockingTcpHandler::operator()() {
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

void BlockingTcpHandler::setTcpServer(TcpServerBase *server) {
  m_server = server;
}

void BlockingTcpHandler::setTcpConn(std::shared_ptr<net::BlockingTcpSocket>& conn) {
  m_conn = conn;
}

void BlockingTcpHandler::onConnect() {
}

void BlockingTcpHandler::onDisconnect() {
}

void BlockingTcpHandler::onMessage(std::string data) {
  std::cerr << "if you can read this, it means you should think about handling connections!" << std::endl;
}
