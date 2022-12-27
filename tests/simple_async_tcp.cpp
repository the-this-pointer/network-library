#ifndef WITH_ASIO
#warning "To run this sample, you should enable asio in the cmakelists options."
#else

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <thread>
#include <Net.h>

using namespace thisptr::utils;
using namespace thisptr::net;
using namespace std::chrono_literals;

class ServerHandler: public std::enable_shared_from_this<ServerHandler>,
    public AsyncConnectionHandlerBase<AsioTcpSocket<ServerHandler>> {
public:

  void onDisconnected(asio::ip::tcp::socket& sock) override {
    std::cout << "[server] client disconnected" << std::endl;
    m_connections.erase(&sock);
  }

  bool onDataReceived(asio::ip::tcp::socket& sock, std::error_code ec, const std::string& payload) override {
    if (ec) {
      std::cerr << "[server] unable to read from socket, ec: " << ec << std::endl;
      return false;
    } else
      std::cout << "[server] data received: " << payload.c_str() << std::endl;
    return true;
  }

  void onDataSent(asio::ip::tcp::socket& sock, std::error_code ec, const std::string& payload) override {
    if (ec) {
      std::cerr << "[server] unable to write to socket" << std::endl;
    } else
      std::cout << "[server] data sent, len: " << payload.length() << std::endl;
  }

  void onNewConnection(asio::ip::tcp::socket& sock) override {
    std::cout << "[server] on new connection" << std::endl;
    auto socket = std::make_shared<AsioTcpSocket<ServerHandler>>(this->shared_from_this(), sock);
    m_connections[&sock] = socket;

    socket->send("hi from server.");
    socket->recv();
  }

private:
  std::unordered_map<asio::ip::tcp::socket *, std::shared_ptr<AsioTcpSocket<ServerHandler>>> m_connections;
};

class ClientHandler: public AsyncConnectionHandlerBase<AsioTcpSocket<ClientHandler>> {
public:
  void onConnected(asio::ip::tcp::socket& sock, const std::string &endpoint) override {
    std::cout << "[client] connected to " << endpoint.c_str() << std::endl;
  }

  void onDisconnected(asio::ip::tcp::socket& sock) override {
    std::cout << "[client] disconnected" << std::endl;
  }

  bool onDataReceived(asio::ip::tcp::socket& sock, std::error_code ec, const std::string& payload) override {
    if (ec) {
      std::cerr << "[client] unable to read from socket, ec: " << ec << std::endl;
      return false;
    } else
      std::cout << "[client] data received: " << payload.c_str() << std::endl;
    return true;
  }

  void onDataSent(asio::ip::tcp::socket& sock, std::error_code ec, const std::string& payload) override {
    if (ec) {
      std::cerr << "[client] unable to write to socket" << std::endl;
    } else
      std::cout << "[client] data sent, len: " << payload.length() << std::endl;
  }

};

auto handler = std::make_shared<ServerHandler>();
AsyncTcpServer<ServerHandler> s(handler);

void stopServer() {
  // stop server after 10 seconds
  std::this_thread::sleep_for(10000ms);
  s.stop();
}

void client(int idx) {
  std::this_thread::sleep_for(1000ms);
  auto chandler = std::make_shared<ClientHandler>();
  AsioTcpSocket<ClientHandler> c(chandler);
  if (!c.connect("127.0.0.1", "7232"))
  {
    std::cout << idx << " : unable to connect to host" << std::endl;
    return;
  }

  std::stringstream ss;
  ss << idx << ":" << "hello!\r\n";

  c.send(ss.str());

  c.recv();
  std::this_thread::sleep_for(3000ms);
}

int main() {
  std::vector<std::thread> threads;
  s.start("127.0.0.1", "7232");

  for (int i = 0; i < 5; ++i) {
    if (i == 0)
    {
      threads.emplace_back([&, i](){ stopServer();});
      continue;
    }
    threads.emplace_back([&, i](){ client(i);});
  }
  for(auto& t: threads)
  {
    t.join();
  }
  std::this_thread::sleep_for(1000ms);

  return 0;
}


#endif