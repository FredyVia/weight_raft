#include <csignal>
#include <iostream>
#include <set>
#include <string>

#include "server.h"

using namespace std;
using namespace weight_raft;

static Server* server_ptr = nullptr;

int main(int argc, char* args[]) {
  if (argc != 3) {
    cout << "client <datapath> <ipaddress>" << endl;
    return -1;
  }

  set<string> ips = {"127.0.0.1", "127.0.0.2", "127.0.0.3"};
  int port = 1088;
  string datapath = string(args[1]);
  server_ptr = new Server(datapath, ips, port, args[2]);
  server_ptr->start();
  return 0;
}