#ifndef WEIGHT_RAFT_SERVER_H
#define WEIGHT_RAFT_SERVER_H
#include <brpc/server.h>  // brpc::WeightServer

#include <memory>
#include <set>
#include <string>

#include "weight_raft/weight_raft.h"

namespace weight_raft {

  class WeightServer {
    // std::filesystem::path m_datapath;
    // std::set<std::string> m_ips;
    std::string m_network_id;
    std::string m_my_ip;
    int m_port;

    brpc::Server m_server;
    WeightRaftStateMachine *m_raft_ptr = nullptr;
    WeightService *m_service_ptr = nullptr;
    ServerService *m_server_service_ptr = nullptr;

  public:
    WeightServer(std::string datapath, std::set<std::string> ips, int port, std::string network_id,
                 std::string my_ip);
    void start();
    void stop();
    void setWeight(int weight);
    std::vector<WeightInfo> get_weights();
    std::string get_leader();
    std::string get_leader_network_id();
    ~WeightServer();
  };

  class ServerServiceImpl : public ServerService {
    WeightServer *m_weight_server_ptr;

  public:
    ServerServiceImpl(WeightServer *weight_server_ptr) : m_weight_server_ptr(weight_server_ptr){};

    // Impelements service methods
    void setServerWeight(::google::protobuf::RpcController *controller,
                         const WeightRequest *request, WeightResponse *response,
                         ::google::protobuf::Closure *done) override {
      brpc::ClosureGuard done_guard(done);
      m_weight_server_ptr->setWeight(request->weight_info().weight());
      response->set_success(true);
    }
  };
}  // namespace weight_raft
#endif