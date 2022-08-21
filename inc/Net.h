#ifndef NetLib_TCPCONNECTION_H
#define NetLib_TCPCONNECTION_H

#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <net_win.h>

namespace thisptr {
  namespace net {

    static int initialize() {
      return thisptr::net_p::initialize();
    }

    static int cleanup() {
      return thisptr::net_p::cleanup();
    }

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
      TcpSocket() : m_sock(-1) {}
      explicit TcpSocket(unsigned long long sock) : m_sock(sock) {}

      virtual ~TcpSocket() {
        if (m_sock != -1)
          thisptr::net_p::close(m_sock);
      }

      int recv(char* buf, int len) override {
        return thisptr::net_p::recv(m_sock, buf, len);
      }

      int send(const char* buf) override {
        return thisptr::net_p::send(m_sock, buf);
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
        bool bRes = (res != -1 && m_sock != INVALID_SOCKET);
        if (bRes) {
          setTimeout(SO_RCVTIMEO, 5000);
          setTimeout(SO_SNDTIMEO, 5000);
        }
        return bRes;
      }

    private:
      std::string m_address;
      std::string m_port;
    };

    class TcpServer: public TcpSocket {
    public:
      TcpServer(): TcpSocket() {}
      virtual ~TcpServer() {
        if (m_stopThread.joinable())
          m_stopThread.join();
      }

      bool bind(const std::string& address, const std::string& port) {
        m_address = address;
        m_port = port;
        int res = thisptr::net_p::listen(m_sock, m_address.c_str(), m_port.c_str());
        bool bRes = (res != -1 && m_sock != INVALID_SOCKET);

        m_stopThread = std::thread(&TcpServer::stopRunnable, this);

        return bRes;
      }

      TcpSocket* accept() {
        unsigned long long sock = thisptr::net_p::accept(m_sock);
        if (sock == INVALID_SOCKET)
          return nullptr;

        setTimeout(sock, SO_RCVTIMEO, 5000);
        setTimeout(sock, SO_SNDTIMEO, 5000);
        return new TcpSocket(sock);
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
      std::thread m_stopThread;
      std::mutex m_stopMutex;
      std::condition_variable m_stopCv;

      std::string m_address;
      std::string m_port;
    };
  }
}

#endif //NetLib_TCPCONNECTION_H
