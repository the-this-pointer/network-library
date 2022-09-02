#include <iostream>

#include <utility>
#include <vector>
#include <thread>
#include <Net.h>
#include <Pool.h>

using namespace thisptr::utils;
using namespace thisptr::net;

TcpServer s;

void onMessageCallback(const std::shared_ptr<thisptr::net::TcpSocket>& conn, const std::string& message) {
  std::cout << " : rec data:" << message << std::endl;

  std::stringstream ss;
  ss << "s : " << "hello!\r\n";

  int n = conn->send(ss.str().c_str());
  if (n < 0) {
    std::cout << " : unable to send data to host" << std::endl;
    conn->close();
  }
}

void stopServer() {
  // stop server after 10 seconds
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(10000ms);
  s.stop();
}

void server() {
  Pool<thisptr::ConnectionHandler, std::shared_ptr<TcpSocket>> pool([](std::shared_ptr<TcpSocket> conn){
    auto h = new thisptr::ConnectionHandler(std::move(conn));
    h->setMessageCallback(onMessageCallback);
    return h;
  }, [=](thisptr::ConnectionHandler* h){
    delete h;
  }, 3);

  if (!s.bind("127.0.0.1", "7232"))
  {
    std::cout << "s : unable to bind to host" << std::endl;
    return;
  }

  while(true) {
    auto conn = s.accept();
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
}

void client(int idx) {
  TcpClient c;
  if (!c.connect("127.0.0.1", "7232"))
  {
    std::cout << idx << " : unable to connect to host" << std::endl;
    return;
  }

  std::stringstream ss;
  ss << idx << " : " << "hello!\r\n";

  int i = 0;
  while (i++ < 3) {
    int n = c.send(ss.str().c_str());
    if (n == 0) {
      std::cout << idx << " : unable to send data to host" << std::endl;
      return;
    }

    char buffer[256] = {0};
    int res = c.recv(buffer, 256);
    if (res <= 0) {
      std::cout << idx << " : connection closed or error occured" << std::endl;
      return;
    }

    std::string recData(buffer, 256);
    std::cout << idx << " : rec data:" << recData << std::endl;
  }

  if (!c.close()) {
    std::cout << idx << " : unable to close socket" << std::endl;
    return;
  }
}

int main() {
  std::vector<std::thread> threads;
  for (int i = 0; i < 6; ++i) {
    if (i == 0)
    {
      threads.emplace_back([&, i](){ server();});
      continue;
    } else if (i == 1) {
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
