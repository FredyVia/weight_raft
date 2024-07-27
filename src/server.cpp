#include "server.h"

#include <stdexcept>

#include "braft/raft.h"
#include "utils.h"

namespace weight_raft {
using namespace braft;
using namespace std;
Server::Server(std::string datapath, std::set<std::string> ips, int port)
    : m_port(port) {
  m_raft_ptr = new WeightRaftStateMachine(datapath, ips, port);
  m_service_ptr = new WeightServiceImpl(m_raft_ptr);
}
void Server::start() {
  if (m_server.AddService(m_service_ptr, brpc::SERVER_DOESNT_OWN_SERVICE) !=
      0) {
    throw runtime_error("server failed to add dn common service");
  }
  if (add_service(&m_server, m_port) != 0) {
    throw runtime_error("server failed to add dn service");
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