#include <brpc/server.h>  // brpc::Server

#include <memory>
#include <set>
#include <string>

#include "weight_raft.h"

namespace weight_raft {

class Server {
  // std::filesystem::path m_datapath;
  // std::set<std::string> m_ips;
  std::string m_my_ip;
  int m_port;

  brpc::Server m_server;
  WeightRaftStateMachine *m_raft_ptr = nullptr;
  WeightService *m_service_ptr = nullptr;

 public:
  Server(std::string datapath, std::set<std::string> ips, int port,
         std::string my_ip);
  void start();
  void stop();
  void setWeight(int weight);
  ~Server();
};

}  // namespace weight_raft