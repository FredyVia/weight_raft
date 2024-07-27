#include <csignal>
#include <iostream>
#include <set>
#include <string>

#include "server.h"

using namespace std;
using namespace weight_raft;

Server* server_ptr = nullptr;

void signalHandler(int signal) {
  std::cout << "Caught signal " << signal << ", cleaning up...\n";
  if (server_ptr != nullptr) delete server_ptr;
  exit(signal);
}
int main(int argc, char* args[]) {
  if (argc != 2) {
    cout << "plz specify id !" << endl;
    return -1;
  }

  std::signal(SIGINT, signalHandler);

  set<string> ips = {"127.0.0.1", "127.0.0.2", "127.0.0.3"};
  int port = 1088;
  string datapath = "/tmp/node" + string(args[1]);
  server_ptr = new Server(datapath, ips, port);
  server_ptr->start();
  return 0;
}