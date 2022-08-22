#include <iostream>

#include <sstream>
#include <vector>
#include <thread>
#include <Net.h>

using namespace thisptr::net;

TcpServer s;

void stopServer() {
  // stop server after 10 seconds
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(10000ms);
  s.stop();
}

void server() {
  int counter = 0;
  if (!s.bind("127.0.0.1", "7232"))
  {
    std::cout << "s : unable to bind to host" << std::endl;
    return;
  }
  char buffer[256] = {0};

  while(true) {
    TcpSocket* conn = s.accept();
    if (conn == nullptr) {
      std::cout << "s : unable to accept connection" << std::endl;
      break;
    }

    counter++;
    int res = conn->recv(buffer, 256);
    if (res <= 0) {
      std::cout << "s" << counter << " : connection closed or error occured" << std::endl;
      break;
    }

    std::string recData(buffer, 256);
    std::cout << "s" << counter << " : rec data:" << recData << std::endl;

    std::stringstream ss;
    ss << "s" << counter << " : " << "hello!\r\n";

    int n = conn->send(ss.str().c_str());
    if (n == 0) {
      std::cout << "s" << counter << " : unable to send data to host" << std::endl;
      break;
    }

    if (!conn->close()) {
      std::cout << "s" << counter << " : unable to close socket" << std::endl;
      break;
    }

    delete conn;
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

  if (!c.close()) {
    std::cout << idx << " : unable to close socket" << std::endl;
    return;
  }
}

int main() {

  std::vector<std::thread> threads;
  for (int i = 0; i < 5; ++i) {
    if (i == 0)
    {
      threads.emplace_back([&, i](){ server();});
      continue;
    } else if (i == 4) {
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
