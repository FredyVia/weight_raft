#include "utils.h"

#include <arpa/inet.h>
#include <dbg.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace weight_raft {
using namespace std;
namespace fs = std::filesystem;

void log_(const char *label, const char *format, ...) {
  va_list args;
  va_start(args, format);
  printf("[%s] ", label);
  vprintf(format, args);
  printf("\n");
  va_end(args);
}

std::set<std::string> get_all_ips() {
  std::set<std::string> res;
  struct ifaddrs *interfaces = nullptr;
  struct ifaddrs *addr = nullptr;
  void *tmpAddrPtr = nullptr;

  if (getifaddrs(&interfaces) == -1) {
    throw runtime_error("get_all_ips getifaddrs failed");
  }

  log_info("get_all_ips:");
  for (addr = interfaces; addr != nullptr; addr = addr->ifa_next) {
    if (addr->ifa_addr == nullptr) {
      continue;
    }
    if (addr->ifa_addr->sa_family == AF_INET) {  // check it is IP4
      // is a valid IP4 Address
      tmpAddrPtr = &((struct sockaddr_in *)addr->ifa_addr)->sin_addr;
      char addressBuffer[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
      log_info("ifa_name: %s, ip_addr: ", addr->ifa_name,
               " IP Address: ", addressBuffer);
      res.insert(string(addressBuffer));
      // } else if (addr->ifa_addr->sa_family == AF_INET6) {  // not support IP6
      // curr
      //   // is a valid IP6 Address
      //   tmpAddrPtr = &((struct sockaddr_in6 *)addr->ifa_addr)->sin6_addr;
      //   char addressBuffer[INET6_ADDRSTRLEN];
      //   inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
      //   std::cout << addr->ifa_name << " IP Address: " << addressBuffer;
    }
  }

  if (interfaces) {
    freeifaddrs(interfaces);
  }
  return res;
}
std::string get_my_ip(const std::set<std::string> &ips) {
  std::set<string> intersection;
  // 使用 std::set_intersection 查找交集

  set<string> local = get_all_ips();
  std::set_intersection(ips.begin(), ips.end(), local.begin(), local.end(),
                        std::inserter(intersection, intersection.begin()));
  cout << "using ip";
  for (auto &str : intersection) {
    cout << str << ", ";
  }
  if (intersection.size() == 0) {
    cout << "cluster ips: ";
    dbg::pretty_print(cout, ips);
    cout << "local ips: ";
    dbg::pretty_print(cout, local);
    throw runtime_error("cannot find equal ips");
  }
  return *(intersection.begin());
}

std::set<std::string> parse_nodes(const std::string &nodes_str) {
  set<std::string> res;
  istringstream stream(nodes_str);
  string token;

  while (getline(stream, token, ',')) {
    if (token.size()) res.insert(token);
  }
  return res;
}

braft::PeerId get_peerid(const std::string &ip, const int &port) {
  butil::EndPoint ep;
  butil::str2endpoint(ip.c_str(), port, &ep);
  return braft::PeerId(ep);
}
vector<braft::PeerId> get_peerids(const std::set<std::string> &ips,
                                  const int &port) {
  vector<braft::PeerId> res;
  res.reserve(ips.size());
  butil::EndPoint ep;
  for (auto ip : ips) {
    res.push_back(get_peerid(ip, port));
    cout << res.back() << endl;
  }
  return res;
}

std::string read_file(const std::string &path) {
  int filesize = fs::file_size(path);
  string block;
  block.resize(filesize);
  ifstream ifile(path, std::ios::binary);
  if (!ifile) {
    LOG(FATAL) << "Failed to open file for reading." << path << endl;
    throw std::runtime_error("openfile error:" + path);
  }
  if (!ifile.read(&block[0], filesize)) {
    LOG(FATAL) << "Failed to read file content." << endl;
    throw std::runtime_error("readfile error:" + path);
  }
  ifile.close();
  return block;
}
void split_address_port(const std::string& address_port, std::string& address, int& port) {
    size_t colon_pos = address_port.find(':');
    if (colon_pos == std::string::npos) {
        throw std::invalid_argument("Invalid address:port format");
    }

    address = address_port.substr(0, colon_pos);
    std::string port_str = address_port.substr(colon_pos + 1);

    port = std::stoi(port_str);  // 将端口字符串转换为整数
}


}  // namespace weight_raft