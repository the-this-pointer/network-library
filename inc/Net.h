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
      explicit TcpSocket(unsigned long long sock);
      virtual ~TcpSocket();

      int recv(char* buf, int len) override;
      int send(const char* buf) override;
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

    class TcpServer;
    typedef void(*OnConnectCallback)(const std::shared_ptr<net::TcpSocket>& conn);
    typedef void(*OnMessageCallback)(const std::shared_ptr<net::TcpSocket>& conn, const std::string& message);

    class ConnectionHandler {
    public:
      explicit ConnectionHandler(std::shared_ptr<net::TcpSocket> conn = nullptr): m_conn(std::move(conn)) {};
      ~ConnectionHandler() = default;

      void operator() ();

      void setConnectCallback(OnConnectCallback connectCallback);
      void setDisconnectCallback(OnConnectCallback disconnectCallback);
      void setMessageCallback(OnMessageCallback messageCallback);
      void setTcpServer(TcpServer* server);
      void setTcpConn(std::shared_ptr<net::TcpSocket>& conn);

    private:
      TcpServer* m_server{};
      std::shared_ptr<net::TcpSocket> m_conn{};
      OnConnectCallback m_connectCallback{};
      OnConnectCallback m_disconnectCallback{};
      OnMessageCallback m_messageCallback{};
    };

    class TcpServer: public TcpSocket {
    public:
      TcpServer(): TcpSocket() {}
      virtual ~TcpServer();

      bool bind(const std::string& address, const std::string& port);
      std::shared_ptr<TcpSocket> accept();
      void setConnectCallback(OnConnectCallback connectCallback);
      void setDisconnectCallback(OnConnectCallback disconnectCallback);
      void setMessageCallback(OnMessageCallback messageCallback);
      void start(const std::string& address, const std::string& port);
      void stop();
      utils::Pool<ConnectionHandler, TcpServer *> *pool() const;

    private:
      void stopRunnable();

      bool m_stopRequested = false;
      std::thread m_serverThread;
      std::thread m_stopThread;
      std::mutex m_stopMutex;
      std::condition_variable m_stopCv;

      utils::Pool<ConnectionHandler, TcpServer*>* m_pool;
      std::string m_address;
      std::string m_port;

      OnConnectCallback m_connectCallback{};
      OnConnectCallback m_disconnectCallback{};
      OnMessageCallback m_messageCallback{};
    };
  }
}

#endif //NetLib_TCPCONNECTION_H
