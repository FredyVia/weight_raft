#include "weight_raft/server.h"

#include <brpc/channel.h>
#include <brpc/controller.h>  // brpc::Controller
#include <butil/endpoint.h>

#include <stdexcept>
#ifndef USE_CMAKE
#  include "weight_raft/service.pb.h"
#else
#  include <service.pb.h>
#endif
#include "weight_raft/utils.h"

namespace weight_raft {
  using namespace std;
  using namespace brpc;
  using namespace braft;

  WeightServer::WeightServer(std::string datapath, std::set<std::string> ips, int port,
                             std::string network_id, std::string my_ip)
      : m_network_id(network_id), m_my_ip(my_ip), m_port(port) {
    m_raft_ptr = new WeightRaftStateMachine(datapath, ips, port, my_ip);
    m_service_ptr = new WeightServiceImpl(m_raft_ptr);
    m_server_service_ptr = new ServerServiceImpl(this);
    if (m_server.AddService(m_service_ptr, brpc::SERVER_OWNS_SERVICE) != 0) {
      throw runtime_error("server failed to add common service");
    }
    if (m_server.AddService(m_server_service_ptr, brpc::SERVER_OWNS_SERVICE) != 0) {
      throw runtime_error("server failed to add common service");
    }

#ifndef USE_CMAKE
    butil::EndPoint ep(butil::IP_ANY, m_port);
#else
    butil::EndPoint ep;
    butil::str2endpoint(m_my_ip.c_str(), m_port, &ep);
#endif
    if (add_service(&m_server, ep) != 0) {
      throw runtime_error("server failed to add raft service");
    }
  }
  void WeightServer::start() {
#ifndef USE_CMAKE
    butil::EndPoint ep(butil::IP_ANY, m_port);
#else
    butil::EndPoint ep;
    butil::str2endpoint(m_my_ip.c_str(), m_port, &ep);
#endif
    if (m_server.Start(ep, NULL) != 0) {
      cout << "Fail to start WeightServer" << endl;
      throw runtime_error("start service failed");
    }
    // waiting_for_rpc();
    m_raft_ptr->start();
  }

  void WeightServer::stop() {
    m_server.Stop(0);
    m_server.Join();
    if (m_raft_ptr != nullptr) {
      m_raft_ptr->shutdown();
      delete m_raft_ptr;
      m_raft_ptr = nullptr;
    }
    if (m_service_ptr != nullptr) {
      delete m_service_ptr;
      m_service_ptr = nullptr;
    }
  }

  std::string WeightServer::get_leader_network_id() { return m_raft_ptr->leader_network_id(); }

  void WeightServer::setWeight(int weight) {
    if (m_raft_ptr == nullptr) throw runtime_error("start before setWeight");
    Controller cntl;
    WeightRequest req;
    WeightResponse resp;
    brpc::Channel channel;
    if (channel.Init(m_raft_ptr->leader().c_str(), NULL) != 0) {
      throw runtime_error("master channel init failed");
    }

    WeightService_Stub *stub_ptr = new WeightService_Stub(&channel);
    req.mutable_weight_info()->set_network_id(m_network_id);
    req.mutable_weight_info()->set_ip_addr(m_my_ip);
    req.mutable_weight_info()->set_weight(weight);

    stub_ptr->setWeight(&cntl, &req, &resp, nullptr);
    if (cntl.Failed()) {
      string res = string("cntl setWeight failed") + cntl.ErrorText();
      cout << res << endl;
      throw runtime_error(res);
    }
    if (resp.success()) {
      log_info("setWeight success");
    } else {
      string res = string("resp setWeight failed") + resp.fail_info();
      cout << res << endl;
      throw runtime_error(res);
    }
  }

  std::vector<WeightInfo> WeightServer::get_weights() { return m_raft_ptr->get_weights(); }

  string WeightServer::get_leader() { return m_raft_ptr->leader(); }

  WeightServer::~WeightServer() { stop(); }

}  // namespace weight_raft