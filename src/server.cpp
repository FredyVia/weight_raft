#include "server.h"

#include <brpc/channel.h>
#include <brpc/controller.h>  // brpc::Controller

#include <stdexcept>

#include "service.pb.h"
#include "utils.h"

namespace weight_raft {
using namespace std;
using namespace brpc;
using namespace braft;
Server::Server(std::string datapath, std::set<std::string> ips, int port,
               std::string my_ip)
    : m_port(port), m_my_ip(my_ip) {
  m_raft_ptr = new WeightRaftStateMachine(datapath, ips, port, my_ip);
  m_service_ptr = new WeightServiceImpl(m_raft_ptr);
  if (m_server.AddService(m_service_ptr, brpc::SERVER_OWNS_SERVICE) != 0) {
    throw runtime_error("server failed to add common service");
  }

  butil::EndPoint ep;
  butil::str2endpoint(m_my_ip.c_str(), m_port, &ep);
  if (add_service(&m_server, ep) != 0) {
    throw runtime_error("server failed to add raft service");
  }
}
void Server::start() {
  butil::EndPoint ep;
  butil::str2endpoint(m_my_ip.c_str(), m_port, &ep);
  if (m_server.Start(ep, NULL) != 0) {
    log_error("Fail to start Server");
    throw runtime_error("start service failed");
  }
  // waiting_for_rpc();
  m_raft_ptr->start();
}

void Server::stop() {
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

void Server::setWeight(int weight) {
  if (m_raft_ptr == nullptr) throw runtime_error("start before setWeight");
  Controller cntl;
  WeightRequest req;
  WeightResponse resp;
  brpc::Channel channel;
  if (channel.Init(m_raft_ptr->leader().c_str(), NULL) != 0) {
    throw runtime_error("master channel init failed");
  }

  WeightService_Stub* stub_ptr = new WeightService_Stub(&channel);
  req.mutable_weight_info()->set_device_id(m_my_ip);
  req.mutable_weight_info()->set_ip_addr(m_my_ip);
  req.mutable_weight_info()->set_weight(weight);

  stub_ptr->setWeight(&cntl, &req, &resp, nullptr);
  if (cntl.Failed()) {
    string res = string("setWeight failed") + resp.fail_info();
    log_error(res.c_str());
    throw runtime_error(res);
  }
  if (resp.success()) {
    log_info("setWeight success");
  } else {
    string res = string("setWeight failed") + resp.fail_info();

    log_error(res.c_str());
    throw runtime_error(res);
  }
  cntl.Reset();
  req.mutable_weight_info()->set_weight(0);
  stub_ptr->getWeight(&cntl, &req, &resp, nullptr);
  if (cntl.Failed()) {
    log_error("cntl getWeight failed");
    throw runtime_error("cntl getWeight failed");
  }
  if (false == resp.success()) {
    throw runtime_error("setWeight show failed" + resp.fail_info());
  }
  if (resp.weight_info().weight() == weight) {
    log_info("setWeight really success");
  } else {
    log_info("setWeight really failed");
    throw runtime_error("setWeight really failed: " +
                        to_string(resp.weight_info().weight()));
  }
}

Server::~Server() { stop(); }

}  // namespace weight_raft