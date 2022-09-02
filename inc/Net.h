#ifndef NetLib_TCPCONNECTION_H
#define NetLib_TCPCONNECTION_H

#include <iostream>
#include <sstream>
#include <thread>
#include <memory>
#include <string>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <net_p.h>
#include <Pool.h>

namespace thisptr {
  namespace net {

    class Socket {
    public:
      Socket() = default;
      virtual ~Socket() = default;

      virtual int recv(char* buf, int len) = 0;
      virtual int send(const char* buf) = 0;
      virtual bool close() = 0;
    };

    class TcpSocket : public Socket {
    public:
      TcpSocket() : TcpSocket(-1) {}
      explicit TcpSocket(unsigned long long sock) : m_sock(sock) {
        thisptr::net_p::initialize();
      }

      virtual ~TcpSocket() {
        if (m_sock != -1)
          thisptr::net_p::close(m_sock);
        thisptr::net_p::cleanup();
      }

      int recv(char* buf, int len) override {
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

      int send(const char* buf) override {
        int iRes = thisptr::net_p::send(m_sock, buf);
        if (iRes == thisptr::net_p::NETE_SocketError) {
          close();
        }
        return iRes;
      }

      bool close() override {
        if (thisptr::net_p::close(m_sock) == 0) {
          m_sock = -1;
          return true;
        }
        return false;
      }

      bool setTimeout(int opt, int val) {
        struct timeval timeout;
        timeout.tv_sec = val;
        timeout.tv_usec = 0;

        return setOpt(opt, &timeout, sizeof(timeout));
      }

    protected:
      bool setTimeout(unsigned long long sock, int opt, int val) {
        struct timeval timeout;
        timeout.tv_sec = val;
        timeout.tv_usec = 0;

        return setOpt(sock, opt, &timeout, sizeof(timeout));
      }

      bool setOpt(unsigned long long sock, int opt, const void* val, int size) {
        return thisptr::net_p::setsockopt(sock, opt, val, size) == 0;
      }

      bool setOpt(int opt, const void* val, int size) {
        return thisptr::net_p::setsockopt(m_sock, opt, val, size) == 0;
      }

      unsigned long long m_sock;
    };

    class TcpClient: public TcpSocket {
    public:
      TcpClient(): TcpSocket() {}
      virtual ~TcpClient() {}

      bool connect(const std::string& address, const std::string& port) {
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

    private:
      std::string m_address;
      std::string m_port;
    };


    typedef void(*OnMessageCallback)(const std::shared_ptr<net::TcpSocket>& conn, const std::string& message);

    class ConnectionHandler {
    public:
      explicit ConnectionHandler(std::shared_ptr<net::TcpSocket> conn): m_conn(std::move(conn)) {};
      ~ConnectionHandler() = default;

      void operator() () {
        while(true) {
          char buffer[256] = {0};
          int res = m_conn->recv(buffer, 256);
          if (res < 0 && res != thisptr::net_p::NETE_Notconnected) {
            std::cout << " : error occured" << std::endl;
            break;
          } else if (res == thisptr::net_p::NETE_Notconnected) {
            std::cout << " : connection closed" << std::endl;
            break;
          }

          std::string recData(buffer, 256);
          if (m_messageCallback) m_messageCallback(m_conn, recData);
        }
      }

      void setMessageCallback(OnMessageCallback messageCallback) {
        m_messageCallback = messageCallback;
      }

    private:
      std::shared_ptr<net::TcpSocket> m_conn;
      OnMessageCallback m_messageCallback{};

    };

    class TcpServer: public TcpSocket {
    public:
      TcpServer(): TcpSocket() {}
      virtual ~TcpServer() {
        if (m_stopThread.joinable())
          m_stopThread.join();
        if (m_serverThread.joinable())
          m_serverThread.join();
      }

      bool bind(const std::string& address, const std::string& port) {
        m_address = address;
        m_port = port;
        int err = thisptr::net_p::listen(m_sock, m_address.c_str(), m_port.c_str());
        bool bRes = (err == thisptr::net_p::NETE_Success && m_sock != INVALID_SOCKET);

        if (bRes) m_stopThread = std::thread(&TcpServer::stopRunnable, this);

        return bRes;
      }

      std::shared_ptr<TcpSocket> accept() {
        unsigned long long sock = thisptr::net_p::accept(m_sock);
        if (sock == INVALID_SOCKET || thisptr::net_p::lastError() == thisptr::net_p::NETE_Wouldblock)
          return nullptr;

        setTimeout(sock, SO_RCVTIMEO, 5000);
        setTimeout(sock, SO_SNDTIMEO, 5000);
        return std::make_shared<TcpSocket>(sock);
      }

      void start (const std::string& address, const std::string& port, OnMessageCallback messageCallback) {
        m_serverThread = std::thread([=](){
          utils::Pool<ConnectionHandler, std::shared_ptr<TcpSocket>> pool([=](std::shared_ptr<TcpSocket> conn){
            auto h = new ConnectionHandler(std::move(conn));
            h->setMessageCallback(messageCallback);
            return h;
          }, [=](ConnectionHandler* h){
            delete h;
          }, 3);

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

            if (pool.isEmpty()) {
              std::cout << "s : server is busy, unable to accept connection!" << std::endl;
              conn->close();
              continue;
            }
            auto* h = pool.pop(0, std::move(conn));
            std::thread(*h).detach();
          }
          });
        m_serverThread.detach();
      }

      void stop() {
        m_stopRequested = true;
        m_stopCv.notify_all();
      }

    private:
      void stopRunnable() {
        std::unique_lock<std::mutex> lk(m_stopMutex);
        m_stopCv.wait(lk, [=]{ return m_stopRequested; });
        thisptr::net_p::close(m_sock);
      }

      bool m_stopRequested = false;
      std::thread m_serverThread;
      std::thread m_stopThread;
      std::mutex m_stopMutex;
      std::condition_variable m_stopCv;

      std::string m_address;
      std::string m_port;
    };
  }
}

#endif //NetLib_TCPCONNECTION_H
