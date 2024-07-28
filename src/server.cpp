#include "server.h"

#include <stdexcept>

#include "braft/raft.h"
#include "utils.h"

namespace weight_raft {
using namespace braft;
using namespace std;
Server::Server(std::string datapath, std::set<std::string> ips, int port,
               std::string my_ip)
    : m_port(port), m_my_ip(my_ip) {
  m_raft_ptr = new WeightRaftStateMachine(datapath, ips, port, my_ip);
  m_service_ptr = new WeightServiceImpl(m_raft_ptr);
  if (m_server.AddService(m_service_ptr, brpc::SERVER_DOESNT_OWN_SERVICE) !=
      0) {
    throw runtime_error("server failed to add dn common service");
  }

  butil::EndPoint ep;
  butil::str2endpoint(m_my_ip.c_str(), m_port, &ep);
  if (add_service(&m_server, ep) != 0) {
    throw runtime_error("server failed to add dn service");
  }
}
void Server::start() {
  butil::EndPoint ep;
  butil::str2endpoint(m_my_ip.c_str(), m_port, &ep);
  if (m_server.Start(ep, NULL) != 0) {
    LOG(ERROR) << "Fail to start Server";
    throw runtime_error("start service failed");
  }
  // waiting_for_rpc();
  m_raft_ptr->start();
  int count = 0;
  while (!brpc::IsAskedToQuit()) {
    cout << "running" << std::flush;
    for (int i = 0; i < 20; i++) {
      sleep(2);
      cout << "." << std::flush;
    }
    cout << endl;
  }
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
Server::~Server() { stop(); }

}  // namespace weight_raft