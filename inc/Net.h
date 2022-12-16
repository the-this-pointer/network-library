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

#ifdef WITH_ASIO
#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#endif

namespace thisptr {
  namespace net {

    template <typename S>
    class Socket {
      using socket_type = S;
    public:
      Socket() = default;
      ~Socket() = default;

      bool connect(const std::string& address, const std::string& port) {
        return m_sock.connect(address, port);
      }

      bool bind(const std::string& address, const std::string& port) {
        return m_sock.bind(address, port);
      }

      std::shared_ptr<socket_type> accept() {
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

    template <typename H>
    class AsioTcpSocket;

    template <typename H>
    class Socket<AsioTcpSocket<H>> {
      using socket_type = AsioTcpSocket<H>;
      using handler_ptr = std::shared_ptr<H>;
    public:
      Socket(handler_ptr handler): m_sock(handler) {}
      ~Socket() = default;

      bool connect(const std::string& address, const std::string& port) {
        return m_sock.connect(address, port);
      }

      bool bind(const std::string& address, const std::string& port) {
        return m_sock.bind(address, port);
      }

      std::shared_ptr<AsioTcpSocket<H>> accept() {
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

    template <typename H>
    class AsioTcpSocket {
      using handler_type = H;
      using handler_ptr = std::shared_ptr<H>;
    public:
      explicit AsioTcpSocket(handler_ptr handler):
      m_handler(handler), m_socket(m_context), m_acceptor(make_strand(m_context))
      {}

      ~AsioTcpSocket() {
        close();
      }

      bool connect(const std::string& address, const std::string& port) {
        try {
          asio::ip::tcp::resolver resolver(m_context);
          asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(address, port);

          asio::async_connect(m_socket, endpoints, [this](std::error_code ec, asio::ip::tcp::endpoint endpoint){
            if (ec)
              std::cerr << "unable to connect to endpoint: " << endpoint.address().to_string() << ", ec: " << ec << std::endl;
            else
              m_handler->onConnected(endpoint.address().to_string());
          });

          m_thread = std::thread([this](){ m_context.run(); });
        } catch (std::exception& e) {
          std::cerr << "exception occured on asio socket connect" << e.what() << std::endl;
          return false;
        }
        return true;
      }

      bool bind(const std::string& address, const std::string& port) {
        try {
          asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), std::stoi(port));
          m_acceptor.open(endpoint.protocol());
          m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
          m_acceptor.bind(endpoint);
          m_acceptor.listen();

          m_thread = std::thread([this](){ m_context.run(); });
        } catch (std::exception& e) {
          std::cerr << "exception occured on asio bind" << e.what() << std::endl;
          return false;
        }
        return true;
      }

      std::shared_ptr<AsioTcpSocket> accept() {

        std::shared_ptr<asio::ip::tcp::socket> sock{new asio::ip::tcp::socket(m_context)};
        m_acceptor.async_accept(*sock,
                                [this, sock](std::error_code ec)
                                {
                                  if (ec)
                                  {
                                    std::cerr << "unable to accept connection, ec: " << ec << std::endl;
                                    m_acceptor.cancel();
                                  } else {
                                    m_handler->onNewConnection(sock);
                                    accept();
                                  }
                                });

        return nullptr;
      }

      int recv(char* buf, int len) {
        asio::async_read(m_socket, asio::buffer(buf, len),
                         [this, buf](std::error_code ec, std::size_t length){
                           if (ec) {
                             std::cerr << "unable to read from socket, ec: " << ec << std::endl;
                             m_socket.close();
                           } else
                             m_handler->onDataReceived(buf, length);
                         });
        return 0;
      }

      int send(const char* buf) {
        return send(buf, strlen(buf));
      }

      int send(const char* buf, int len) {
        asio::async_write(m_socket, asio::buffer(buf, len),
                          [this, buf](std::error_code ec, std::size_t length){
                            if (ec) {
                              std::cerr << "unable to write to socket" << std::endl;
                              m_socket.close();
                            } else
                              m_handler->onDataSent(buf, length);
                          });
        return 0;
      }

      bool close() {
        if (m_acceptor.is_open())
          asio::post(m_acceptor.get_executor(), [this] { m_acceptor.cancel(); });

        if (m_socket.is_open())
          asio::post(m_context, [this]() { m_socket.close(); });

        m_context.stop();
        if (m_thread.joinable())
          m_thread.join();

        m_handler->onDisconnected();
        return true;
      }

    private:
      asio::io_context m_context;
      std::thread m_thread;

      asio::ip::tcp::socket m_socket;
      asio::ip::tcp::acceptor m_acceptor;

      handler_ptr m_handler;
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

    template <typename S>
    class TcpClient: public Socket<S> {
      using socket_type = S;
    public:
      TcpClient(): Socket<socket_type>() {}
      virtual ~TcpClient() = default;
    };

    class TcpServerBase {};

    class AsyncConnectionHandlerBase {
    public:
      virtual void onConnected(const std::string& endpoint) = 0;
      virtual void onDisconnected() = 0;
      virtual void onDataReceived(const char* buff, std::size_t len) = 0;
      virtual void onDataSent(const char* buff, std::size_t len) = 0;
      virtual void onNewConnection(std::shared_ptr<asio::ip::tcp::socket> sock) = 0;
    };

    class BlockingTcpHandler {
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

      std::shared_ptr<socket_type> accept() {
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

    template <typename H>
    class TcpServer<AsioTcpSocket<H>, H>: public TcpServerBase {
      using socket_type = AsioTcpSocket<H>;
      using handler_ptr = std::shared_ptr<H>;
    public:
      TcpServer(handler_ptr handler): m_sock(handler) {}

      virtual ~TcpServer() {
        stop();
      }

      bool bind(const std::string& address, const std::string& port) {
        return m_sock.bind(address, port);
      }

      std::shared_ptr<socket_type> accept() {
        return m_sock.accept();
      }

      void start(const std::string& address, const std::string& port) {
        if (!bind(address, port))
        {
          std::cout << "s : unable to bind to host" << std::endl;
          return;
        }

        accept();
      }

      void stop() {
        m_sock.close();
      }

    protected:
      Socket<socket_type> m_sock;

      std::string m_address;
      std::string m_port;
    };

    template <typename H>
    using BlockingTcpServer = TcpServer<BlockingTcpSocket, H>;

    template <typename H>
    using AsyncTcpServer = TcpServer<AsioTcpSocket<H>, H>;
  }
}

#endif //NetLib_TCPCONNECTION_H
