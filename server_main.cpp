#include <csignal>
#include <iostream>
#include <set>
#include <string>

#include "server.h"
#include "utils.h"

using namespace std;
using namespace weight_raft;

static Server* server_ptr = nullptr;

int main(int argc, char* args[]) {
  if (argc != 5) {
    cout << "client <datapath> <weight> <all_ipaddresses> <my_ipaddress>"
         << endl;
    return -1;
  }

  int port = 1088;
  string datapath = string(args[1]);
  int weight = stoi(args[2]);
  set<string> ips = parse_nodes(string(args[3]));
  server_ptr = new Server(datapath, ips, port, args[4]);
  server_ptr->start();
  cout << "waiting to set weight" << endl;
  sleep(5);
  server_ptr->setWeight(weight);
  int count = 0;
  while (!brpc::IsAskedToQuit()) {
    cout << "running" << std::flush;
    for (int i = 0; i < 20; i++) {
      sleep(2);
      cout << "." << std::flush;
    }
    cout << endl;
  }
  return 0;
}