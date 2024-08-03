#include <iostream>
#include <set>
#include <string>

#include "weight_raft/server.h"
#include "weight_raft/utils.h"

using namespace std;
using namespace weight_raft;

int main(int argc, char* args[]) {
  if (argc != 4) {
    cout << "client <datapath> <all_ipaddresses> <my_ipaddress>" << endl;
    return -1;
  }
  int port = 1088;
  string datapath = string(args[1]);
  set<string> ips = parse_nodes(string(args[2]));

  WeightServer* server_ptr = new WeightServer(datapath, ips, port, args[3], args[3]);
  server_ptr->start();

  // server_ptr->start();
  int count = 0;
  while (!brpc::IsAskedToQuit()) {
    sleep(2);
    cout << "." << std::flush;
    count++;
    if (count == 5) {
      cout << endl;
      cout << "running" << std::flush;
    }
  }
  return 0;
}