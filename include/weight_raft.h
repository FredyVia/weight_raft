#include <map>
#include <memory>
#include <string>

#include "braft/raft.h"
#include "service.pb.h"
namespace weight_raft {

class WeightRaftStateMachine : public braft::StateMachine {
  std::filesystem::path m_datapath;
  std::set<std::string> m_ips;
  int m_port;
  std::shared_ptr<braft::Node> m_raft_node_ptr;

 public:
  WeightRaftStateMachine(std::string datapath, std::set<std::string> ips,
                         int port);
  void start();
  void on_apply(braft::Iterator& iter) override;
  void redirect(WeightResponse* response);
  void shutdown();
};

class WeightClosure : public braft::Closure {
 public:
  WeightClosure(WeightRaftStateMachine* sm, const WeightRequest* request,
                WeightResponse* response, google::protobuf::Closure* done)
      : m_sm(sm), m_request(request), m_response(response), m_done(done) {}
  ~WeightClosure() {}

  const WeightRequest* request() const { return m_request; }
  WeightResponse* response() const { return m_response; }
  void Run() override;

 private:
  WeightRaftStateMachine* m_sm;
  const WeightRequest* m_request;
  WeightResponse* m_response;
  google::protobuf::Closure* m_done;
};

class WeightServiceImpl : public WeightService {
  std::map<std::string, int> weights;
  WeightRaftStateMachine* m_raft_ptr;

 public:
  WeightServiceImpl(WeightRaftStateMachine* raft_ptr) : m_raft_ptr(raft_ptr){};
  void setWeight(::google::protobuf::RpcController* controller,
                 const WeightRequest*, WeightResponse*,
                 ::google::protobuf::Closure* done) override;
  void getWeight(::google::protobuf::RpcController* controller,
                 const WeightRequest*, WeightResponse*,
                 ::google::protobuf::Closure* done) override;
  friend class WeightRaftStateMachine;
};

}  // namespace weight_raft