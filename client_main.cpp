#include <brpc/channel.h>
#include <brpc/controller.h>  // brpc::Controller
#include <gflags/gflags.h>

#include <iostream>
#include <stdexcept>
#include <string>

#ifndef USE_CMAKE
#  include "weight_raft/service.pb.h"
#else
#  include <service.pb.h>
#endif
// #include "utils.h"

using namespace std;
using namespace brpc;
using namespace weight_raft;

DEFINE_string(ip, "127.0.0.1:1088", "query ip's weight");
DEFINE_string(command, "get", "optype");  // get set get_leader
DEFINE_int32(weight, 0, "weight");

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  Controller cntl;
  brpc::Channel channel;
  if (channel.Init(FLAGS_ip.c_str(), NULL) != 0) {
    throw runtime_error("cannot connect to ip" + FLAGS_ip);
  }
  if (FLAGS_command == "get") {
    WeightRequest req;
    WeightResponse resp;
    req.mutable_weight_info()->set_ip_addr(FLAGS_ip);
    WeightService_Stub *stub_ptr = new WeightService_Stub(&channel);
    stub_ptr->getWeight(&cntl, &req, &resp, NULL);
    if (cntl.Failed()) {
      throw runtime_error("brpc weight not success: " + cntl.ErrorText());
    }
    if (!resp.success()) {
      cout << resp.fail_info() << endl;
      throw runtime_error("weight logic error");
    }
    cout << resp.weight_info().ip_addr() << " : " << resp.weight_info().weight() << endl;
  } else if (FLAGS_command == "set") {
    WeightRequest req;
    WeightResponse resp;
    req.mutable_weight_info()->set_weight(FLAGS_weight);
    ServerService_Stub *stub_ptr = new ServerService_Stub(&channel);
    cntl.set_timeout_ms(2000);
    stub_ptr->setServerWeight(&cntl, &req, &resp, NULL);
    if (cntl.Failed()) {
      throw runtime_error("brpc master not success: " + cntl.ErrorText());
    }
    if (!resp.success()) {
      cout << resp.fail_info() << endl;
      throw runtime_error("weight logic error");
    } else {
      cout << "SUCCESS" << endl;
    }
  } else {  // get leader
    MasterRequest mreq;
    MasterResponse mresp;
    WeightService_Stub *stub_ptr = new WeightService_Stub(&channel);
    stub_ptr->getMaster(&cntl, &mreq, &mresp, NULL);
    if (cntl.Failed()) {
      throw runtime_error("brpc master not success: " + cntl.ErrorText());
    }
    if (!mresp.success()) {
      cout << mresp.fail_info() << endl;
      throw runtime_error("logic error");
    }
    cout << "master: " << mresp.master_ip() << endl;
  }

  return 0;
}