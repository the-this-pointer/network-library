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

    template <typename T>
    class Socket {
      using socket_type = T;
    public:
      Socket() = default;
      ~Socket() = default;

      bool connect(const std::string& address, const std::string& port) {
        return m_sock.connect(address, port);
      }

      bool bind(const std::string& address, const std::string& port) {
        return m_sock.bind(address, port);
      }

      std::shared_ptr<T> accept() {
        return m_sock.accept();
      }

      int recv(char* buf, int len) {
        return m_sock.recv(buf, len);
      }

      int send(const char* buf) {
        return m_sock.send(buf);
      }

      int send(const char* buf, int len) {
        return m_sock.send(buf, len);
      }

      bool close() {
        return m_sock.close();
      }

    private:
      socket_type m_sock;
    };

    class BlockingTcpSocket {
    public:
      BlockingTcpSocket() : BlockingTcpSocket(-1) {}
      explicit BlockingTcpSocket(unsigned long long sock);
      virtual ~BlockingTcpSocket();

      bool connect(const std::string& address, const std::string& port);
      bool bind(const std::string& address, const std::string& port);
      std::shared_ptr<BlockingTcpSocket> accept();

      int recv(char* buf, int len);
      int send(const char* buf);
      int send(const char* buf, int len);
      bool close();

    protected:
      unsigned long long m_sock;
    };

    template <typename T>
    class TcpClient: public Socket<T> {
      using socket_type = T;
    public:
      TcpClient(): Socket<socket_type>() {}
      virtual ~TcpClient() = default;

    private:
      std::string m_address;
      std::string m_port;
    };

    class TcpServerBase {};

    class ConnectionHandlerBase {
    public:
      void operator() ();

      virtual void onConnect();
      virtual void onDisconnect();
      virtual void onMessage(std::string data);

      void setTcpServer(TcpServerBase* server);
      void setTcpConn(std::shared_ptr<net::BlockingTcpSocket>& conn);
    protected:
      TcpServerBase* m_server {};
      std::shared_ptr<net::BlockingTcpSocket> m_conn{};
    };

    template <typename S, typename H>
    class TcpServer: public TcpServerBase {
      using socket_type = S;
      using handler_type = H;
    public:
      TcpServer() = default;

      virtual ~TcpServer() {
        if (m_stopThread.joinable())
          m_stopThread.join();
        if (m_serverThread.joinable())
          m_serverThread.join();
      }

      bool bind(const std::string& address, const std::string& port) {
        bool bRes = m_sock.bind(address, port);
        if (bRes) m_stopThread = std::thread(&TcpServer::stopRunnable, this);
        return bRes;
      }

      std::shared_ptr<BlockingTcpSocket> accept() {
        return m_sock.accept();
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

            std::shared_ptr<handler_type> h = newHandler();
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

      void setNewHandler(std::shared_ptr<handler_type>(*newHandler)())
      {
        m_newHandlerCallback = newHandler;
      }

      std::shared_ptr<handler_type> newHandler()
      {
        if (!m_newHandlerCallback)
        {
          std::cerr << "you need some initialization for your server!" << std::endl;
          return nullptr;
        }
        return m_newHandlerCallback();
      }

      void removeHandler(std::shared_ptr<handler_type> handler)
      {
        if (m_removeHandlerCallback)
          m_removeHandlerCallback(handler);
      }

    protected:
      void stopRunnable() {
        std::unique_lock<std::mutex> lk(m_stopMutex);
        m_stopCv.wait(lk, [=]{ return m_stopRequested; });
        m_sock.close();
      }

      Socket<socket_type> m_sock;

      bool m_stopRequested = false;
      std::thread m_serverThread;
      std::thread m_stopThread;
      std::mutex m_stopMutex;
      std::condition_variable m_stopCv;

      std::string m_address;
      std::string m_port;

      std::shared_ptr<handler_type>(*m_newHandlerCallback)();
      void(*m_removeHandlerCallback)(std::shared_ptr<handler_type>);

    };
  }
}

#endif //NetLib_TCPCONNECTION_H
