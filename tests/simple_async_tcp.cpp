#ifndef WITH_ASIO
#warning "To run this sample, you should enable asio in the cmakelists options."
#else

#include <iostream>
#include <sstream>
#include <vector>
#include <thread>
#include <Net.h>

using namespace thisptr::utils;
using namespace thisptr::net;
using namespace std::chrono_literals;

class ServerHandler: public AsyncConnectionHandlerBase<AsioTcpSocket<ServerHandler>> {
public:

  void onDataReceived(std::error_code ec, const char *buff, std::size_t len) override {
    if (ec) {
      std::cerr << "[server] unable to read from socket, ec: " << ec << std::endl;
    } else
      std::cout << "[server] data received: " << std::string(buff, len).c_str() << std::endl;
    delete[] buff;
  }

  void onDataSent(std::error_code ec, const char *buff, std::size_t len) override {
    if (ec) {
      std::cerr << "[server] unable to write to socket" << std::endl;
    } else
      std::cout << "[server] data sent, len: " << len << std::endl;
  }

  void onNewConnection(std::shared_ptr<AsioTcpSocket<ServerHandler>> sock) override {
    std::cout << "[server] on new connection" << std::endl;
    m_connections.push_back(sock);

    sock->send("hi from server.");

    char* buff = new char[11] {0};
    sock->recv(buff, 10);
  }

private:
  std::deque<std::shared_ptr<AsioTcpSocket<ServerHandler>>> m_connections;
};

class ClientHandler: public AsyncConnectionHandlerBase<AsioTcpSocket<ServerHandler>> {
public:
  void onConnected(const std::string &endpoint) override {
    std::cout << "[client] connected to " << endpoint.c_str() << std::endl;
  }

  void onDisconnected() override {
    std::cout << "[client] disconnected" << std::endl;
  }

  void onDataReceived(std::error_code ec, const char *buff, std::size_t len) override {
    if (ec) {
      std::cerr << "[client] unable to read from socket, ec: " << ec << std::endl;
    } else
      std::cout << "[client] data received: " << std::string(buff, len).c_str() << std::endl;
    delete[] buff;
  }

  void onDataSent(std::error_code ec, const char *buff, std::size_t len) override {
    if (ec) {
      std::cerr << "[client] unable to write to socket" << std::endl;
    } else
      std::cout << "[client] data sent, len: " << len << std::endl;
  }

};

auto handler = std::make_shared<ServerHandler>();
auto chandler = std::make_shared<ClientHandler>();
AsyncTcpServer<ServerHandler> s(handler);

void stopServer() {
  // stop server after 10 seconds
  std::this_thread::sleep_for(10000ms);
  s.stop();
}

void client(int idx) {
  std::this_thread::sleep_for(3000ms);
  AsioTcpSocket<ClientHandler> c(chandler);
  if (!c.connect("127.0.0.1", "7232"))
  {
    std::cout << idx << " : unable to connect to host" << std::endl;
    return;
  }

  std::stringstream ss;
  ss << idx << ":" << "hello!\r\n";

  c.send(ss.str().c_str());

  char buffer[16] = {0};
  c.recv(buffer, 15);
  std::this_thread::sleep_for(5000ms);

  std::string recData(buffer, 256);
  std::cout << "cli[" << idx << "] data:" << recData << std::endl;
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

  return 0;
}


#endif