#include <braft/raft.h>

#include <atomic>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>

#include "service.pb.h"
namespace nlohmann {
template <>
struct adl_serializer<weight_raft::WeightInfo> {
  static void to_json(json& j, const weight_raft::WeightInfo& w) {
    j = json{{"device_id", w.device_id()},
             {"ip_addr", w.ip_addr()},
             {"weight", w.weight()}};
  }

  static void from_json(const json& j, weight_raft::WeightInfo& w) {
    w.set_device_id(j.at("device_id").get<std::string>());
    w.set_ip_addr(j.at("ip_addr").get<std::string>());
    w.set_weight(j.at("weight").get<int64_t>());
  }
};
}  // namespace nlohmann
namespace weight_raft {

class WeightRaftStateMachine : public braft::StateMachine {
 private:
  std::filesystem::path m_datapath;
  std::set<std::string> m_ips;
  int m_port;
  std::string m_my_ip;
  std::shared_ptr<braft::Node> m_raft_node_ptr;
  butil::atomic<int64_t> m_leader_term = -1;
  std::thread* m_worker_thread_ptr;

  std::shared_mutex m_weights_mutex;
  std::map<std::string, WeightInfo>
      m_weights_ip_addr;  // service.proto中weights是int64
  // std::map<std::string, WeightInfo>
  //     m_weights_device_id;  // service.proto中weights是int64

  bool m_stop_thread = false;
  // std::atomic<bool> m_work_flag = false;
  // std::string m_new_master_ip;
  // int m_new_max;

 public:
  WeightRaftStateMachine(std::string datapath, std::set<std::string> ips,
                         int port, std::string my_ip);

  void start();
  void on_apply(braft::Iterator& iter) override;
  std::string leader();
  void setWeight(::google::protobuf::RpcController* controller,
                 const WeightRequest*, WeightResponse*,
                 ::google::protobuf::Closure* done);
  std::vector<WeightInfo> get_sorted_weights();
  WeightInfo getWeight(std::string ip);
  void redirect(WeightResponse* response);
  void shutdown();
  void on_snapshot_save(braft::SnapshotWriter* writer, braft::Closure* done);
  int on_snapshot_load(braft::SnapshotReader* reader);
  void on_leader_start(int64_t term);
  void on_leader_stop(const butil::Status& status);
  bool is_leader() const;
  void checker_thread();
  void thread_function();
  void try_transfer_master();

  friend class WeightServiceImpl;
};

class WeightClosure : public braft::Closure {
 public:
  WeightClosure(WeightResponse* response, google::protobuf::Closure* done)
      : m_response(response), m_done(done) {}
  ~WeightClosure() {}

  WeightResponse* response() const { return m_response; }
  void Run() override;

 private:
  WeightResponse* m_response;
  google::protobuf::Closure* m_done;
};
class WeightServiceImpl : public WeightService {
  WeightRaftStateMachine* m_raft_ptr;

 public:
  WeightServiceImpl(WeightRaftStateMachine* raft_ptr) : m_raft_ptr(raft_ptr){};

  // Impelements service methods
  void setWeight(::google::protobuf::RpcController* controller,
                 const WeightRequest* request, WeightResponse* response,
                 ::google::protobuf::Closure* done) override;
  void getWeight(::google::protobuf::RpcController* controller,
                 const WeightRequest* request, WeightResponse* response,
                 ::google::protobuf::Closure* done) override;
  void getMaster(::google::protobuf::RpcController* controller,
                 const MasterRequest* request, MasterResponse* response,
                 ::google::protobuf::Closure* done) override;
};

}  // namespace weight_raft