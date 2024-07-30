#include <braft/configuration.h>

#include <set>
#include <string>

namespace weight_raft {

#define log_info(format, ...) log_("INFO", format, ##__VA_ARGS__)
#define log_error(format, ...) log_("ERROR", format, ##__VA_ARGS__)
void log_(const char *label, const char *format, ...);

std::set<std::string> get_all_ips();
std::string get_my_ip(const std::set<std::string> &ips);
braft::PeerId get_peerid(const std::string &ip, const int &port);
std::vector<braft::PeerId> get_peerids(const std::set<std::string> &ips,
                                       const int &port);
std::set<std::string> parse_nodes(const std::string &nodes_str);
std::string read_file(const std::string &path);
void split_address_port(const std::string &address_port, std::string &address,
                        int &port);
}  // namespace weight_raft