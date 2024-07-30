#include <brpc/channel.h>
#include <brpc/controller.h>  // brpc::Controller
#include <gflags/gflags.h>

#include <stdexcept>
#include <string>

#include "bits/stdc++.h"
#include "service.pb.h"
// #include "utils.h"

using namespace std;
using namespace brpc;
using namespace weight_raft;

DEFINE_string(connect_node, "127.0.0.1:1088",
              "connect to this node to getweight");
DEFINE_string(ip, "127.0.0.1", "query ip's weight");
DEFINE_string(command, "get", "optype");
DEFINE_int32(weight, 0, "weight");

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  Controller cntl;
  brpc::Channel channel;
  if (channel.Init(FLAGS_connect_node.c_str(), NULL) != 0) {
    throw runtime_error("master channel init failed");
  }
  WeightService_Stub* stub_ptr = new WeightService_Stub(&channel);
  WeightRequest req;
  WeightResponse resp;
  req.mutable_weight_info()->set_device_id(FLAGS_ip);
  req.mutable_weight_info()->set_ip_addr(FLAGS_ip);
  if (FLAGS_command == "get") {
    stub_ptr->getWeight(&cntl, &req, &resp, NULL);
    if (cntl.Failed()) {
      throw runtime_error("brpc weight not success: " + cntl.ErrorText());
    }
    if (!resp.success()) {
      cout << resp.fail_info() << endl;
      throw runtime_error("weight logic error");
    }
    cout << resp.weight_info().ip_addr() << " : " << resp.weight_info().weight()
         << endl;
  } else {
    MasterRequest mreq;
    MasterResponse mresp;
    stub_ptr->getMaster(&cntl, &mreq, &mresp, NULL);
    if (cntl.Failed()) {
      throw runtime_error("brpc master not success: " + cntl.ErrorText());
    }
    if (!mresp.success()) {
      cout << mresp.fail_info() << endl;
      throw runtime_error("weight logic error");
    }
    cout << "master: " << mresp.master() << endl;
    cntl.Reset();
    brpc::Channel mchannel;
    if (mresp.master() != FLAGS_connect_node) {
      cout << "master ip not equal connect_node" << endl;
      if (mchannel.Init(mresp.master().c_str(), NULL) != 0) {
        throw runtime_error("master mchannel init failed");
      }
      stub_ptr = new WeightService_Stub(&mchannel);
    }
    req.mutable_weight_info()->set_weight(FLAGS_weight);
    stub_ptr->setWeight(&cntl, &req, &resp, NULL);
    if (cntl.Failed()) {
      throw runtime_error("brpc master not success: " + cntl.ErrorText());
    }
    if (!mresp.success()) {
      cout << mresp.fail_info() << endl;
      throw runtime_error("weight logic error");
    } else {
      cout << "success" << endl;
    }
  }

  return 0;
}