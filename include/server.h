#include <brpc/server.h>  // brpc::Server

#include <memory>
#include <set>
#include <string>

#include "weight_raft.h"

namespace weight_raft {

class Server {
  // std::filesystem::path m_datapath;
  // std::set<std::string> m_ips;
  int m_port;

  brpc::Server m_server;
  WeightRaftStateMachine *m_raft_ptr = nullptr;
  WeightService *m_service_ptr = nullptr;

 public:
  Server(std::string datapath, std::set<std::string> ips, int port);
  void start();
  void stop();
  ~Server();
};

}  // namespace weight_raft