#include "weight_raft.h"

#include <brpc/closure_guard.h>

#include <filesystem>
#include <stdexcept>
#include <string>

#include "utils.h"

namespace weight_raft {
using namespace std;

namespace fs = std::filesystem;
WeightRaftStateMachine::WeightRaftStateMachine(std::string datapath,
                                               std::set<std::string> ips,
                                               int port)
    : m_datapath(datapath), m_ips(ips), m_port(port) {
  auto parent_path = m_datapath.parent_path();
  if (fs::exists(parent_path) && fs::is_directory(parent_path)) {
    throw runtime_error("path not exists: " + parent_path.string());
  }
}
void WeightRaftStateMachine::start() {
  butil::EndPoint addr;
  string ip = weight_raft::get_my_ip(m_ips);
  butil::str2endpoint(ip.c_str(), m_port, &addr);
  braft::NodeOptions node_options;
  node_options.initial_conf =
      braft::Configuration(parse_to_peerids(m_ips, m_port));
  node_options.election_timeout_ms = 5;
  node_options.fsm = this;
  node_options.node_owns_fsm = false;
  node_options.snapshot_interval_s = 60;
  string prefix = m_datapath;
  node_options.log_uri = prefix + "/log";
  node_options.raft_meta_uri = prefix + "/raft_meta";
  node_options.snapshot_uri = prefix + "/snapshot";
  m_raft_node_ptr = make_shared<braft::Node>("default", braft::PeerId(addr));
  if (m_raft_node_ptr->init(node_options) != 0) {
    throw runtime_error("Fail to init raft node");
  }
}

void WeightRaftStateMachine::on_apply(braft::Iterator& iter) {
  for (; iter.valid(); iter.next()) {
    WeightResponse* response = NULL;
    WeightClosure* c = dynamic_cast<WeightClosure*>(iter.done());
    response = c->response();
    // todo: waiting for gwy to implement: update weight in weights and then
    // response to client
  }
}

void WeightRaftStateMachine::redirect(WeightResponse* response) {
  response->set_success(false);
  if (m_raft_node_ptr) {
    braft::PeerId leader = m_raft_node_ptr->leader_id();
    if (!leader.is_empty()) {
      response->set_redirect(leader.to_string());
    }
  }
}

void WeightRaftStateMachine::shutdown() { m_raft_node_ptr->shutdown(NULL); }

void WeightServiceImpl::setWeight(::google::protobuf::RpcController* controller,
                                  const WeightRequest* request,
                                  WeightResponse* response,
                                  ::google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  // todo: waiting for gwy to implement setWeight(). set map<string, int>
  // weights by request.device_id and request.weight
  controller->SetFailed("setWeight() not implemented.");
}

void WeightServiceImpl::getWeight(::google::protobuf::RpcController* controller,
                                  const WeightRequest* request,
                                  WeightResponse* response,
                                  ::google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  // todo: waiting for gwy to implement getWeight(). return weights by
  // request.device_id
  controller->SetFailed("getWeight() not implemented.");
}

void WeightClosure::Run() {
  // Auto delete this after Run()
  std::unique_ptr<WeightClosure> self_guard(this);
  // Repsond this RPC.
  brpc::ClosureGuard done_guard(m_done);
  if (status().ok()) {
    return;
  }
  // Try redirect if this request failed.
  m_sm->redirect(m_response);
}

}  // namespace weight_raft