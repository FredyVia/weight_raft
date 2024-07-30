#include "weight_raft.h"

#include <braft/storage.h>
#include <braft/util.h>
#include <brpc/closure_guard.h>
#include <butil/iobuf.h>
#include <butil/logging.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include "service.pb.h"
#include "utils.h"

namespace weight_raft {
using namespace std;
using namespace nlohmann;
namespace fs = std::filesystem;

WeightRaftStateMachine::WeightRaftStateMachine(std::string datapath,
                                               std::set<std::string> ips,
                                               int port, std::string my_ip)
    : m_datapath(datapath), m_ips(ips), m_port(port), m_my_ip(my_ip) {
  auto parent_path = m_datapath.parent_path();
  if (!fs::exists(parent_path) || !fs::is_directory(parent_path)) {
    throw runtime_error("directory not exists: " + parent_path.string());
  }
  if (!fs::exists(datapath)) {
    error_code ec;
    fs::create_directories(datapath, ec);
    if (ec) {
      cout << "Error: Unable to create directory " << datapath << ". "
           << ec.message();
      throw runtime_error("mkdir error: " + datapath + ec.message());
    }
  } else if (!fs::is_directory(datapath)) {
    throw runtime_error("datapath is file not directory: " + datapath);
  }
}
void WeightRaftStateMachine::start() {
  butil::EndPoint addr;
  butil::str2endpoint(m_my_ip.c_str(), m_port, &addr);
  braft::NodeOptions node_options;
  node_options.initial_conf = braft::Configuration(get_peerids(m_ips, m_port));
  node_options.fsm = this;
  node_options.node_owns_fsm = false;
  node_options.snapshot_interval_s = 60;
  string prefix = string("local://") + m_datapath.string();
  node_options.log_uri = prefix + "/log";
  node_options.raft_meta_uri = prefix + "/raft_meta";
  node_options.snapshot_uri = prefix + "/snapshot";
  m_raft_node_ptr = make_shared<braft::Node>("default", braft::PeerId(addr));
  if (m_raft_node_ptr->init(node_options) != 0) {
    throw runtime_error("Fail to init raft node");
  }
}

void WeightRaftStateMachine::on_apply(braft::Iterator& iter) {
  // A batch of tasks are committed, which must be processed through
  // |iter|
  for (; iter.valid(); iter.next()) {
    // todo: waiting for gwy to implement: update weight in weights and then
    // response to client
    // 1 This guard helps invoke iter.done()->Run() asynchronously to
    // avoid that callback blocks the StateMachine.
    braft::AsyncClosureGuard done_guard(iter.done());
    butil::IOBufAsZeroCopyInputStream wrapper(iter.data());
    WeightInfo weight_info;
    bool flag = false;
    CHECK(weight_info.ParseFromZeroCopyStream(&wrapper));
    if (!weight_info.device_id().empty() && !weight_info.ip_addr().empty()) {
      //  && (!m_weights_ip_addr.contains(weight_info.ip_addr()) ||
      //    m_weights_ip_addr[weight_info.ip_addr()].version() + 1 ==
      //        weight_info.version())
      flag = true;
    }
    if (flag) {
      std::unique_lock<std::shared_mutex> lock(m_weights_mutex);
      m_weights_ip_addr[weight_info.ip_addr()] = weight_info;
      log_info("changing weight %s, weight: %d", weight_info.ip_addr().c_str(),
               weight_info.weight());
      // m_work_flag.store(true);
      // m_weights_device_id[weight_info.device_id()] = weight_info;
    }

    if (iter.done()) {
      // done only execute on local device
      WeightClosure* c = dynamic_cast<WeightClosure*>(iter.done());
      WeightResponse* response = c->response();
      if (flag) {
        response->set_success(true);
      } else {
        response->set_success(false);
        response->set_fail_info("both ip and deviceid need to be non empty");
      }
    }
  }
}

void WeightRaftStateMachine::on_snapshot_save(braft::SnapshotWriter* writer,
                                              braft::Closure* done) {
  // Save current StateMachine in memory and starts a new bthread to avoid
  // blocking StateMachine since it's a bit slow to write data to disk
  // file.
  nlohmann::json jsonMap1;  //, jsonMap2;
  string file_path;
  {
    std::shared_lock<std::shared_mutex> lock(m_weights_mutex);
    // jsonMap1 = m_weights_device_id;
    jsonMap1 = m_weights_ip_addr;
  }
  file_path = writer->get_path() + "/m_weights_ip_addr.json";
  ofstream file1(file_path, ios::binary);
  if (!file1) {
    LOG(ERROR) << "Failed to open file for writing." << file_path;
    done->status().set_error(EIO, "Fail to save " + file_path);
  }
  file1 << jsonMap1.dump();
  file1.close();

  // file_path = writer->get_path() + "/m_weights_device_id.json";
  // ofstream file2(file_path, ios::binary);
  // if (!file2) {
  //   LOG(ERROR) << "Failed to open file2 for writing." << file_path;
  //   done->status().set_error(EIO, "Fail to save " + file_path);
  // }
  // file2 << jsonMap2.dump();
  // file2.close();
}

WeightInfo WeightRaftStateMachine::get_max_weight() {
  std::shared_lock<std::shared_mutex> lock(m_weights_mutex);
  auto p = max_element(m_weights_ip_addr.begin(), m_weights_ip_addr.end(),
                       [](const auto& p1, const auto& p2) {
                         return p1.second.weight() < p2.second.weight();
                       });
  WeightInfo res;
  if (p != m_weights_ip_addr.end()) res = p->second;
  LOG(INFO) << "ip " << res.ip_addr() << ", max weight: " << res.weight();
  return res;
}

WeightInfo WeightRaftStateMachine::getWeight(std::string ip) {
  std::shared_lock<std::shared_mutex> lock(m_weights_mutex);
  return m_weights_ip_addr[ip];
}

int WeightRaftStateMachine::on_snapshot_load(braft::SnapshotReader* reader) {
  // 反序列化 JSON 为 std::map
  string str;
  string file_path;

  // file_path = reader->get_path() + "/m_weights_device_id.json";
  // str = read_file(file_path);
  // if (!str.empty()) {
  //   auto _json = json::parse(str);
  //   tmp_weights_device_id = _json.get<std::map<std::string, WeightInfo>>();
  // }

  file_path = reader->get_path() + "/m_weights_ip_addr.json";
  str = read_file(file_path);
  if (!str.empty()) {
    auto _json = json::parse(str);
    std::unique_lock<std::shared_mutex> lock(m_weights_mutex);
    m_weights_ip_addr = _json.get<std::map<std::string, WeightInfo>>();
  }
  // if (m_weights_ip_addr.size() != m_weights_device_id.size()) {
  //   throw
  //   runtime_error("m_weights_ip_addr.size()!=m_weights_device_id.size()");
  // }

  return 0;
}

void WeightRaftStateMachine::on_leader_start(int64_t term) {
  m_leader_term.store(term, butil::memory_order_release);
  LOG(INFO) << "Node becomes leader";
  m_stop_thread = false;
  m_worker_thread_ptr =
      new std::thread(&WeightRaftStateMachine::thread_function, this);
}

void WeightRaftStateMachine::on_leader_stop(const butil::Status& status) {
  m_leader_term.store(-1, butil::memory_order_release);
  LOG(INFO) << "Node stepped down : " << status;
  if (m_worker_thread_ptr != nullptr) {
    if (m_worker_thread_ptr->joinable()) {
      m_stop_thread = true;
      m_worker_thread_ptr->join();
      delete m_worker_thread_ptr;
    }
  }
}
// Impelements service methods: setWeight and getWeight
void WeightRaftStateMachine::setWeight(
    ::google::protobuf::RpcController* controller, const WeightRequest* request,
    WeightResponse* response, ::google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  // 1 Check leader's term
  if (!is_leader()) {
    return redirect(response);
  }
  WeightInfo weight_info = request->weight_info();
  // weight_info.set_version(m_weights_ip_addr[weight_info.ip_addr()].version()
  // +
  //                         1);
  // 2 Serialize request
  butil::IOBuf log;
  // std::string requestkv = std::to_string(request->device_id()) + ":" +
  // std::to_string(request->weight()); log
  // append的应该是一个sql的指令，insert(k,v) log.append(requestkv);
  butil::IOBufAsZeroCopyOutputStream wrapper(&log);
  if (false == weight_info.SerializeToZeroCopyStream(&wrapper)) {
    LOG(ERROR) << "Fail to serialize request";
    response->set_success(false);
    return;
  }
  // 3 Apply this log as a braft::Task
  braft::Task task;
  task.data = &log;
  task.done = new WeightClosure(response, done_guard.release());
  /*if(FLAGS_check_term){
    task.expected_term = term;
  }*/
  // 4 Apply task to the group, waiting for the result
  return m_raft_node_ptr->apply(task);
  // controller->SetFailed("setWeight() not implemented.");
}

std::string WeightRaftStateMachine::leader() {
  braft::PeerId leader = m_raft_node_ptr->leader_id();
  string res;
  if (is_leader()) {
    res = m_my_ip + ":" + to_string(m_port);
  } else {
    res = endpoint2str(leader.addr).c_str();
  }
  log_info("leader: %s", res.c_str());
  return res;
}

void WeightRaftStateMachine::redirect(WeightResponse* response) {
  response->set_success(false);
  response->set_redirect(leader());
}

void WeightRaftStateMachine::shutdown() { m_raft_node_ptr->shutdown(nullptr); }

bool WeightRaftStateMachine::is_leader() const {
  return m_leader_term.load(butil::memory_order_acquire) > 0;
}

void WeightRaftStateMachine::thread_function() {
  try_transfer_master();
  int count = 0;
  while (!m_stop_thread) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    count++;
    if (count != 2) continue;
    count = 0;
    // if (m_work_flag.load()) {
    try_transfer_master();
    // m_work_flag.store(false);
    // }
  }
}

void WeightRaftStateMachine::try_transfer_master() {
  WeightInfo weight_info = get_max_weight();
  if (weight_info.weight() > getWeight(m_my_ip).weight()) {
    LOG(INFO) << "transfer leadership";
    if (m_raft_node_ptr->transfer_leadership_to(
            get_peerid(weight_info.ip_addr().c_str(), m_port)) != 0)
      throw runtime_error("transfer_leadership_to failed");
  }
}

void WeightClosure::Run() {
  // Auto delete this after Run()
  std::unique_ptr<WeightClosure> self_guard(this);
  // Repsond this RPC.
  brpc::ClosureGuard done_guard(m_done);
}

void WeightServiceImpl::setWeight(::google::protobuf::RpcController* controller,
                                  const WeightRequest* request,
                                  WeightResponse* response,
                                  ::google::protobuf::Closure* done) {
  m_raft_ptr->setWeight(controller, request, response, done);
}

void WeightServiceImpl::getWeight(::google::protobuf::RpcController* controller,
                                  const WeightRequest* request,
                                  WeightResponse* response,
                                  ::google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  WeightInfo weight_info = request->weight_info();
  // if (weight_info.device_id().size()) {
  //   response->set_success(true);
  //   response->mutable_weight_info()->set_weight(
  //       m_raft_ptr->m_weights_device_id[weight_info.device_id()].weight());
  // } else
  if (weight_info.ip_addr().size()) {
    response->set_success(true);
    response->mutable_weight_info()->set_ip_addr(weight_info.ip_addr());
    response->mutable_weight_info()->set_weight(
        m_raft_ptr->getWeight(weight_info.ip_addr()).weight());
  } else {
    response->set_success(false);
    response->set_fail_info("both ip and deviceid are empty");
  }
  // controller->SetFailed("getWeight() not implemented.");
}

void WeightServiceImpl::getMaster(::google::protobuf::RpcController* controller,
                                  const MasterRequest* request,
                                  MasterResponse* response,
                                  ::google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  // response->set_success(false);
  // response->set_fail_info("m_raft_node_ptr nor empty");
  // if (m_raft_node_ptr) {
  //   braft::PeerId leader = m_raft_node_ptr->leader_id();
  //   if (!leader.is_empty()) {
  response->set_success(true);
  response->set_master(m_raft_ptr->leader());
  // }
  // }
}
}  // namespace weight_raft