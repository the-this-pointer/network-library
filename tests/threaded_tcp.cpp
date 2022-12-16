#include <iostream>

#include <utility>
#include <vector>
#include <thread>
#include <Net.h>
#include <Pool.h>

using namespace thisptr::utils;
using namespace thisptr::net;

class SimpleConnectionHandler: public ConnectionHandlerBase {
public:
  void onConnect() override {
    std::cout << "new connection received" << std::endl;
  }

  void onDisconnect() override {
    std::cout << "new connection dropped" << std::endl;
  }

  void onMessage(std::string data) override {
    std::cout << "new message received: " << data << std::endl;

    std::stringstream ss;
    ss << "s : " << "hello!\r\n";

    int n = m_conn->send(ss.str().c_str());
    if (n < 0) {
      std::cout << " : unable to send data to host" << std::endl;
      m_conn->close();
    }
  }
};


TcpServer<BlockingTcpSocket, SimpleConnectionHandler> s;

void stopServer() {
  // stop server after 10 seconds
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(10000ms);
  s.stop();
}

void client(int idx) {
  TcpClient<BlockingTcpSocket> c;
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
    std::cout << idx << " : data:" << recData << std::endl;
  }

  if (!c.close()) {
    std::cout << idx << " : unable to close socket" << std::endl;
    return;
  }
}

int main() {
  std::vector<std::thread> threads;
  s.setNewHandler([]() -> std::shared_ptr<SimpleConnectionHandler> {
    return std::make_shared<SimpleConnectionHandler>();
  });
  s.start("127.0.0.1", "7232");

  for (int i = 0; i < 4; ++i) {
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
