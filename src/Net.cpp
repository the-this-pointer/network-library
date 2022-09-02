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

void thisptr::net::ConnectionHandler::operator()() {
  if (m_connectCallback) m_connectCallback(m_conn);

  while(true) {
    char buffer[256] = {0};
    int res = m_conn->recv(buffer, 256);
    if (res < 0 && res != thisptr::net_p::NETE_Notconnected) {
      std::cout << " : error occured" << std::endl;
      if (m_disconnectCallback) m_disconnectCallback(m_conn);
      break;
    } else if (res == thisptr::net_p::NETE_Notconnected) {
      std::cout << " : connection closed" << std::endl;
      if (m_disconnectCallback) m_disconnectCallback(m_conn);
      break;
    }

    std::string recData(buffer, 256);
    if (m_messageCallback) m_messageCallback(m_conn, recData);
  }
  m_server->pool()->push(this);
}

void thisptr::net::ConnectionHandler::setConnectCallback(thisptr::net::OnConnectCallback connectCallback) {
  m_connectCallback = connectCallback;
}

void thisptr::net::ConnectionHandler::setDisconnectCallback(thisptr::net::OnConnectCallback disconnectCallback) {
  m_disconnectCallback = disconnectCallback;
}

void thisptr::net::ConnectionHandler::setMessageCallback(thisptr::net::OnMessageCallback messageCallback) {
  m_messageCallback = messageCallback;
}

void thisptr::net::ConnectionHandler::setTcpServer(thisptr::net::TcpServer *server) {
  m_server = server;
}

void thisptr::net::ConnectionHandler::setTcpConn(std::shared_ptr<net::TcpSocket>& conn) {
  m_conn = conn;
}

thisptr::net::TcpServer::~TcpServer() {
  if (m_stopThread.joinable())
    m_stopThread.join();
  if (m_serverThread.joinable())
    m_serverThread.join();
}

bool thisptr::net::TcpServer::bind(const std::string &address, const std::string &port) {
  m_address = address;
  m_port = port;
  int err = thisptr::net_p::listen(m_sock, m_address.c_str(), m_port.c_str());
  bool bRes = (err == thisptr::net_p::NETE_Success && m_sock != INVALID_SOCKET);

  if (bRes) m_stopThread = std::thread(&TcpServer::stopRunnable, this);

  return bRes;
}

std::shared_ptr<thisptr::net::TcpSocket> thisptr::net::TcpServer::accept() {
  unsigned long long sock = thisptr::net_p::accept(m_sock);
  if (sock == INVALID_SOCKET || thisptr::net_p::lastError() == thisptr::net_p::NETE_Wouldblock)
    return nullptr;

  setTimeout(sock, SO_RCVTIMEO, 5000);
  setTimeout(sock, SO_SNDTIMEO, 5000);
  return std::make_shared<TcpSocket>(sock);
}

void thisptr::net::TcpServer::setConnectCallback(thisptr::net::OnConnectCallback connectCallback) {
  m_connectCallback = connectCallback;
}

void thisptr::net::TcpServer::setDisconnectCallback(thisptr::net::OnConnectCallback disconnectCallback) {
  m_disconnectCallback = disconnectCallback;
}

void thisptr::net::TcpServer::setMessageCallback(thisptr::net::OnMessageCallback messageCallback) {
  m_messageCallback = messageCallback;
}

void thisptr::net::TcpServer::start(const std::string &address, const std::string &port) {
  m_pool = new utils::Pool<ConnectionHandler, TcpServer*>([=](TcpServer* server){
    auto h = new ConnectionHandler();
    h->setTcpServer(server);
    h->setConnectCallback(m_connectCallback);
    h->setDisconnectCallback(m_disconnectCallback);
    h->setMessageCallback(m_messageCallback);
    return h;
  }, [=](ConnectionHandler* h){
    delete h;
  }, 3);

  m_serverThread = std::thread([=](){
    if (!bind(address, port))
    {
      std::cout << "s : unable to bind to host" << std::endl;
      return;
    }

    while(true) {
      auto conn = accept();
      if (conn == nullptr) {
        std::cout << "s : unable to accept connection" << std::endl;
        break;
      }

      if (m_pool->isEmpty()) {
        std::cout << "s : server is busy, unable to accept connection!" << std::endl;
        conn->close();
        continue;
      }
      auto* h = m_pool->pop(0, this);
      h->setTcpConn(conn);
      std::thread(*h).detach();
    }
  });
  m_serverThread.detach();
}

void thisptr::net::TcpServer::stop() {
  m_stopRequested = true;
  m_stopCv.notify_all();
}

thisptr::utils::Pool<thisptr::net::ConnectionHandler, thisptr::net::TcpServer *> *thisptr::net::TcpServer::pool() const {
  return m_pool;
}

void thisptr::net::TcpServer::stopRunnable() {
  std::unique_lock<std::mutex> lk(m_stopMutex);
  m_stopCv.wait(lk, [=]{ return m_stopRequested; });
  thisptr::net_p::close(m_sock);
}
