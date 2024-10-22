#include "weight_raft/weight_raft.h"

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

#ifndef USE_CMAKE
#  include "weight_raft/service.pb.h"
#else
#  include <service.pb.h>
#endif
#include "weight_raft/utils.h"

namespace weight_raft {
  using namespace std;
  using namespace nlohmann;
  using namespace butil;
  using namespace braft;
  namespace fs = std::filesystem;

  WeightRaftStateMachine::WeightRaftStateMachine(std::string datapath, std::set<std::string> ips,
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
        cout << "Error: Unable to create directory " << datapath << ". " << ec.message();
        throw runtime_error("mkdir error: " + datapath + ec.message());
      }
    } else if (!fs::is_directory(datapath)) {
      throw runtime_error("datapath is file not directory: " + datapath);
    }
  }
  void WeightRaftStateMachine::start() {
    EndPoint addr;
    str2endpoint(m_my_ip.c_str(), m_port, &addr);
    NodeOptions node_options;
    node_options.initial_conf = Configuration(get_peerids(m_ips, m_port));
    node_options.fsm = this;
    node_options.node_owns_fsm = false;
    node_options.snapshot_interval_s = 60;
    string prefix = string("local://") + m_datapath.string();
    node_options.log_uri = prefix + "/log";
    node_options.raft_meta_uri = prefix + "/raft_meta";
    node_options.snapshot_uri = prefix + "/snapshot";
    m_raft_node_ptr = make_shared<Node>("default", PeerId(addr));
    if (m_raft_node_ptr->init(node_options) != 0) {
      throw runtime_error("Fail to init raft node");
    }
  }

  void WeightRaftStateMachine::on_apply(Iterator& iter) {
    // A batch of tasks are committed, which must be processed through
    // |iter|
    for (; iter.valid(); iter.next()) {
      // todo: waiting for gwy to implement: update weight in weights and then
      // response to client
      // 1 This guard helps invoke iter.done()->Run() asynchronously to
      // avoid that callback blocks the StateMachine.
      AsyncClosureGuard done_guard(iter.done());
      IOBufAsZeroCopyInputStream wrapper(iter.data());
      WeightInfo weight_info;
      bool flag = false;
      CHECK(weight_info.ParseFromZeroCopyStream(&wrapper));
      if (!weight_info.network_id().empty() && !weight_info.ip_addr().empty()) {
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
        // m_weights_network_id[weight_info.network_id()] = weight_info;
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

  void WeightRaftStateMachine::on_snapshot_save(SnapshotWriter* writer, Closure* done) {
    // Save current StateMachine in memory and starts a new bthread to avoid
    // blocking StateMachine since it's a bit slow to write data to disk
    // file.
    nlohmann::json jsonMap1;  //, jsonMap2;
    string file_path;
    {
      std::shared_lock<std::shared_mutex> lock(m_weights_mutex);
      // jsonMap1 = m_weights_network_id;
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

    // file_path = writer->get_path() + "/m_weights_network_id.json";
    // ofstream file2(file_path, ios::binary);
    // if (!file2) {
    //   LOG(ERROR) << "Failed to open file2 for writing." << file_path;
    //   done->status().set_error(EIO, "Fail to save " + file_path);
    // }
    // file2 << jsonMap2.dump();
    // file2.close();
  }

  vector<WeightInfo> WeightRaftStateMachine::get_sorted_weights() {
    vector<WeightInfo> res = get_weights();
    sort(res.begin(), res.end(),
         [](const auto& w1, const auto& w2) { return w1.weight() > w2.weight(); });
    return res;
  }

  WeightInfo WeightRaftStateMachine::getWeight(std::string ip) {
    std::shared_lock<std::shared_mutex> lock(m_weights_mutex);
    return m_weights_ip_addr[ip];
  }

  int WeightRaftStateMachine::on_snapshot_load(SnapshotReader* reader) {
    // 反序列化 JSON 为 std::map
    string str;
    string file_path;

    // file_path = reader->get_path() + "/m_weights_network_id.json";
    // str = read_file(file_path);
    // if (!str.empty()) {
    //   auto _json = json::parse(str);
    //   tmp_weights_network_id = _json.get<std::map<std::string, WeightInfo>>();
    // }

    file_path = reader->get_path() + "/m_weights_ip_addr.json";
    str = read_file(file_path);
    if (!str.empty()) {
      auto _json = json::parse(str);
      std::unique_lock<std::shared_mutex> lock(m_weights_mutex);
      m_weights_ip_addr = _json.get<std::map<std::string, WeightInfo>>();
    }
    // if (m_weights_ip_addr.size() != m_weights_network_id.size()) {
    //   throw
    //   runtime_error("m_weights_ip_addr.size()!=m_weights_network_id.size()");
    // }

    return 0;
  }

  std::vector<WeightInfo> WeightRaftStateMachine::get_weights() {
    unique_lock<shared_mutex> lock(m_weights_mutex);
    vector<WeightInfo> res;
    std::transform(m_weights_ip_addr.begin(), m_weights_ip_addr.end(), std::back_inserter(res),
                   [](const auto& p) { return p.second; });
    return res;
  }

  void WeightRaftStateMachine::on_leader_start(int64_t term) {
    m_leader_term.store(term, memory_order_release);
    cout << "\033[32mNode becomes leader\033[0m" << endl;
    m_stop_thread = false;
    m_worker_thread_ptr = new std::thread(&WeightRaftStateMachine::thread_function, this);
  }

  void WeightRaftStateMachine::on_leader_stop(const Status& status) {
    m_leader_term.store(-1, memory_order_release);
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
  void WeightRaftStateMachine::setWeight(::google::protobuf::RpcController* controller,
                                         const WeightRequest* request, WeightResponse* response,
                                         ::google::protobuf::Closure* done) {
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
    IOBuf log;
    // std::string requestkv = std::to_string(request->network_id()) + ":" +
    // std::to_string(request->weight()); log
    // append的应该是一个sql的指令，insert(k,v) log.append(requestkv);
    IOBufAsZeroCopyOutputStream wrapper(&log);
    if (false == weight_info.SerializeToZeroCopyStream(&wrapper)) {
      LOG(ERROR) << "Fail to serialize request";
      response->set_success(false);
      return;
    }
    // 3 Apply this log as a Task
    Task task;
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
    PeerId leader = m_raft_node_ptr->leader_id();
    string res;
    if (is_leader()) {
      res = m_my_ip + ":" + to_string(m_port);
    } else {
      res = endpoint2str(leader.addr).c_str();
    }
    log_info("leader: %s", res.c_str());
    return res;
  }

  std::string WeightRaftStateMachine::leader_network_id() {
    string ip;
    PeerId leader = m_raft_node_ptr->leader_id();
    if (is_leader()) {
      ip = m_my_ip;
    } else {
      ip = ip2str(leader.addr.ip).c_str();
    }
    return m_weights_ip_addr[ip].network_id();
  }

  void WeightRaftStateMachine::redirect(WeightResponse* response) {
    response->set_success(false);
    response->set_redirect(leader());
  }

  void WeightRaftStateMachine::shutdown() { m_raft_node_ptr->shutdown(nullptr); }

  bool WeightRaftStateMachine::is_leader() const {
    return m_leader_term.load(memory_order_acquire) > 0;
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
    vector<WeightInfo> weight_infos = get_sorted_weights();
    for (auto&& weight_info : weight_infos) {
      if (weight_info.weight() <= getWeight(m_my_ip).weight()) {
        break;
      }
      if (m_raft_node_ptr->transfer_leadership_to(get_peerid(weight_info.ip_addr().c_str(), m_port))
          == 0) {
        LOG(INFO) << "transfer leadership" << weight_info.ip_addr();
        break;
      }
    }
  }

  void WeightClosure::Run() {
    // Auto delete this after Run()
    std::unique_ptr<WeightClosure> self_guard(this);
    // Repsond this RPC.
    brpc::ClosureGuard done_guard(m_done);
  }

  void WeightServiceImpl::setWeight(::google::protobuf::RpcController* controller,
                                    const WeightRequest* request, WeightResponse* response,
                                    ::google::protobuf::Closure* done) {
    m_raft_ptr->setWeight(controller, request, response, done);
  }

  void WeightServiceImpl::getWeight(::google::protobuf::RpcController* controller,
                                    const WeightRequest* request, WeightResponse* response,
                                    ::google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    WeightInfo weight_info = request->weight_info();
    // if (weight_info.network_id().size()) {
    //   response->set_success(true);
    //   response->mutable_weight_info()->set_weight(
    //       m_raft_ptr->m_weights_network_id[weight_info.network_id()].weight());
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
                                    const MasterRequest* request, MasterResponse* response,
                                    ::google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    // response->set_success(false);
    // response->set_fail_info("m_raft_node_ptr nor empty");
    // if (m_raft_node_ptr) {
    //   PeerId leader = m_raft_node_ptr->leader_id();
    //   if (!leader.is_empty()) {
    response->set_success(true);
    response->set_master_ip(m_raft_ptr->leader());
    LOG(INFO) << "m_raft_ptr->leader(): " << m_raft_ptr->leader();
    string ip;
    int port;
    split_address_port(m_raft_ptr->leader(), ip, port);
    response->set_master_network_id(m_raft_ptr->getWeight(ip).network_id());
    // }
    // }
  }
}  // namespace weight_raft