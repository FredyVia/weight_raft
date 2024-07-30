#include <brpc/channel.h>
#include <brpc/controller.h>  // brpc::Controller

#include "bits/stdc++.h"
#include "service.pb.h"

using namespace std;
using namespace brpc;
using namespace weight_raft;

int main(int argc, char* args[]) {
  if (argc != 3) {
    cout << "plz specify <ip> <port> !" << endl;
    return -1;
  }
  Controller cntl;
  WeightRequest req;
  WeightResponse resp;
  brpc::Channel channel;
  string ip = string(args[1]);
  string port = string(args[2]);
  string ip_port = ip + string(":") + port;
  if (channel.Init(ip_port.c_str(), NULL) != 0) {
    throw runtime_error("master channel init failed");
  }

  WeightService_Stub* stub_ptr = new WeightService_Stub(&channel);
  req.mutable_weight_info()->set_device_id(ip);
  req.mutable_weight_info()->set_ip_addr(ip);
  stub_ptr->getWeight(&cntl, &req, &resp, NULL);
  if (cntl.Failed()) {
    throw runtime_error("brpc not success: " + cntl.ErrorText());
  }
  if (!resp.success()) {
    cout << resp.fail_info() << endl;
    throw runtime_error("logic error");
  }
  cout << resp.weight_info().ip_addr() << " : " << resp.weight_info().weight()
       << endl;
  return 0;
}