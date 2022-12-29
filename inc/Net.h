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
#include <deque>
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

    class AsioContextHolder {
    public:
      virtual ~AsioContextHolder() {
        stop();
      }

      bool start() {
        if (m_running)
          return true;
        m_running = true;

        try {
          m_thread = std::thread([this](){ m_context.run(); });
          return true;
        } catch (std::exception& e) {
          std::cerr << "exception occured on asio context start" << e.what() << std::endl;
        }
        return false;
      }

      void stop() {
        if (!m_running)
          return;
        m_running = false;
        m_context.stop();
        if (m_thread.joinable())
          m_thread.join();
      }

      asio::io_context& ctx() {
        return m_context;
      }

    private:
      bool m_running {false};
      asio::io_context m_context;
      std::thread m_thread;
    };

    template <typename H>
    class Socket<AsioTcpSocket<H>> {
      using socket_type = AsioTcpSocket<H>;
      using handler_ptr = std::shared_ptr<H>;
    public:
      explicit Socket(handler_ptr handler): m_sock(m_contextHolder.ctx(), handler) {}
      Socket(): m_sock(m_contextHolder.ctx(), nullptr) {}
      ~Socket() { m_contextHolder.stop(); }

      void setHandler(handler_ptr handler) {
        m_sock.setHandler(handler);
      }

      bool connect(const std::string& address, const std::string& port) {
        bool res = m_sock.connect(address, port);
        m_contextHolder.start();
        return res;
      }

      int recv() {
        return m_sock.recv();
      }

      int recv(unsigned int len) {
        return m_sock.recv(len);
      }

      int send(const std::string& payload) {
        return m_sock.send(payload);
      }

      int send(const char* buf, int len) {
        return m_sock.send(buf, len);
      }

      bool close() {
        return m_sock.close();
      }

    private:
      AsioContextHolder m_contextHolder;
      socket_type m_sock;
    };

    template <typename H>
    class AsioTcpSocket {
      using handler_ptr = std::shared_ptr<H>;
    public:
      explicit AsioTcpSocket(asio::ip::tcp::socket& socket, handler_ptr handler = nullptr):
      m_handler(handler), m_socket(std::move(socket))
      {}

      explicit AsioTcpSocket(asio::io_context& context, handler_ptr handler = nullptr):
      m_handler(handler), m_socket(context)
      {}

      ~AsioTcpSocket() {
        close();
      }

      void setHandler(handler_ptr handler) {
        m_handler = handler;
      }

      bool connect(const std::string& address, const std::string& port) {
        try {
          asio::ip::tcp::resolver resolver(m_socket.get_executor());
          asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(address, port);

          asio::async_connect(m_socket, endpoints, [this](std::error_code ec, asio::ip::tcp::endpoint endpoint){
            if (ec)
              std::cerr << "unable to connect to endpoint: " << endpoint.address().to_string() << ", ec: " << ec << std::endl;
            else
              m_handler->onConnected(m_socket, endpoint.address().to_string());
          });

        } catch (std::exception& e) {
          std::cerr << "exception occured on asio socket connect" << e.what() << std::endl;
          return false;
        }
        return true;
      }

      int recv() {
        asio::async_read(m_socket, m_buffer,
                         asio::transfer_at_least(1),
                         [this](std::error_code ec, std::size_t length){
                           std::string payload;
                           {
                             std::stringstream ss;
                             ss << &m_buffer;
                             ss.flush();
                             payload = ss.str();
                           }
                           if (m_handler->onDataReceived(m_socket, ec, payload))
                             recv();
                         });
        return 0;
      }

      int recv(unsigned int len) {
        asio::async_read(m_socket, m_buffer,
                         asio::transfer_exactly(len),
                         [this](std::error_code ec, std::size_t length){
                           std::string payload;
                           {
                             std::stringstream ss;
                             ss << &m_buffer;
                             ss.flush();
                             payload = ss.str();
                           }
                           m_handler->onDataReceived(m_socket, ec, payload);
                         });
        return 0;
      }

      int send(const std::string& payload) {
        asio::async_write(m_socket, asio::buffer(payload),
                          [this, payload](std::error_code ec, std::size_t length){
                            m_handler->onDataSent(m_socket, ec, payload);
                          });
        return -1;
      }

      int send(const char* buf, int len) {
        return send(std::string(buf, len));
      }

      bool close() {
        bool bWasOpen;
        if ((bWasOpen = m_socket.is_open()))
          asio::post(m_socket.get_executor(), [this]() {
            m_socket.close();
          });

        if (bWasOpen && m_handler)
          m_handler->onDisconnected(m_socket);

        return true;
      }

    private:
      asio::streambuf m_buffer;

      asio::ip::tcp::socket m_socket;
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

    template <typename S>
    class AsyncConnectionHandlerBase {
      using socket_type = S;
    public:
      virtual void onConnected(asio::ip::tcp::socket& sock, const std::string& endpoint) {}
      virtual void onDisconnected(asio::ip::tcp::socket& sock) {}
      virtual bool onDataReceived(asio::ip::tcp::socket& sock, std::error_code ec, const std::string& payload) = 0;
      virtual void onDataSent(asio::ip::tcp::socket& sock, std::error_code ec, const std::string& payload) = 0;
      virtual void onNewConnection(asio::ip::tcp::socket& sock) {}
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
      explicit TcpServer(handler_ptr handler) :m_handler(handler), m_acceptor(make_strand(m_contextHolder.ctx())) {}

      virtual ~TcpServer() {
        stop();
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
        if (m_acceptor.is_open())
          asio::post(m_acceptor.get_executor(), [this] { m_acceptor.cancel(); });

        m_contextHolder.stop();
      }

      bool bind(const std::string& address, const std::string& port) {
        try {
          asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), std::stoi(port));
          m_acceptor.open(endpoint.protocol());
          m_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
          m_acceptor.bind(endpoint);
          m_acceptor.listen();

          m_contextHolder.start();
        } catch (std::exception& e) {
          std::cerr << "exception occured on asio bind" << e.what() << std::endl;
          return false;
        }
        return true;
      }

      std::shared_ptr<socket_type> accept() {

        auto* sock = new asio::ip::tcp::socket(m_contextHolder.ctx());
        m_acceptor.async_accept(*sock,
                                [this, sock](std::error_code ec)
                                {
                                  if (ec)
                                  {
                                    std::cerr << "unable to accept connection, ec: " << ec << std::endl;
                                    stop();
                                  } else {
                                    m_handler->onNewConnection(*sock);
                                    accept();
                                  }
                                });

        return nullptr;
      }

    protected:
      AsioContextHolder m_contextHolder;
      asio::ip::tcp::acceptor m_acceptor;
      handler_ptr m_handler;
    };

    template <typename H>
    using BlockingTcpServer = TcpServer<BlockingTcpSocket, H>;

    template <typename H>
    using AsyncTcpServer = TcpServer<AsioTcpSocket<H>, H>;

    template <typename H>
    using AsyncTcpClient = Socket<AsioTcpSocket<H>>;
  }
}

#endif //NetLib_TCPCONNECTION_H
