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
      virtual int send(const char* buf, int len) = 0;
      virtual bool close() = 0;
    };

    class TcpSocket : public Socket {
    public:
      TcpSocket() : TcpSocket(-1) {}
      explicit TcpSocket(unsigned long long sock);
      virtual ~TcpSocket();

      int recv(char* buf, int len) override;
      int send(const char* buf) override;
      int send(const char* buf, int len) override;
      bool close() override;
      bool setTimeout(int opt, int val);

    protected:
      bool setTimeout(unsigned long long sock, int opt, int val);
      bool setOpt(unsigned long long sock, int opt, const void* val, int size);
      bool setOpt(int opt, const void* val, int size);
      unsigned long long m_sock;
    };

    class TcpClient: public TcpSocket {
    public:
      TcpClient(): TcpSocket() {}
      virtual ~TcpClient() {}

      bool connect(const std::string& address, const std::string& port);

    private:
      std::string m_address;
      std::string m_port;
    };

    class TcpServerBase {

    };

    class ConnectionHandlerBase {
    public:
      void operator() ();

      virtual void onConnect();
      virtual void onDisconnect();
      virtual void onMessage(std::string data);

      void setTcpServer(TcpServerBase* server);
      void setTcpConn(std::shared_ptr<net::TcpSocket>& conn);
    protected:
      TcpServerBase* m_server {};
      std::shared_ptr<net::TcpSocket> m_conn{};
    };

    template <typename T>
    class TcpServer: public TcpServerBase, public TcpSocket {
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

      void start(const std::string& address, const std::string& port) {
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

            std::shared_ptr<T> h = newHandler();
            if (!h)
            {
              conn->close();
              std::cout << "s : cannot accept connection at this time!" << std::endl;
              continue;
            }
            h->setTcpConn(conn);
            h->setTcpServer(this);
            std::thread(*h).detach();
          }
        });
        m_serverThread.detach();
      }

      void stop() {
        m_stopRequested = true;
        m_stopCv.notify_all();
      }

      void setNewHandler(std::shared_ptr<T>(*newHandler)())
      {
        m_newHandlerCallback = newHandler;
      }

      std::shared_ptr<T> newHandler()
      {
        if (!m_newHandlerCallback)
        {
          std::cerr << "you need some initialization for your server!" << std::endl;
          return nullptr;
        }
        return m_newHandlerCallback();
      }

      void removeHandler(std::shared_ptr<T> handler)
      {
        if (m_removeHandlerCallback)
          m_removeHandlerCallback(handler);
      }

    protected:
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

      std::shared_ptr<T>(*m_newHandlerCallback)();
      void(*m_removeHandlerCallback)(std::shared_ptr<T>);

    };
  }
}

#endif //NetLib_TCPCONNECTION_H
