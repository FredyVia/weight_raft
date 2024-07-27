#include <set>
#include <string>

#include "braft/configuration.h"

namespace weight_raft {

#define log_info(format, ...) log_("INFO", format, ##__VA_ARGS__)
#define log_error(format, ...) log_("ERROR", format, ##__VA_ARGS__)
void log_(const char *label, const char *format, ...);

std::set<std::string> get_all_ips();
std::string get_my_ip(const std::set<std::string> &ips);
std::vector<braft::PeerId> parse_to_peerids(const std::set<std::string> &ips,
                                            const int &port);

}  // namespace weight_raft