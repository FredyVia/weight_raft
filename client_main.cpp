#include <brpc/channel.h>
#include <brpc/controller.h>  // brpc::Controller

#include "service.pb.h"

using namespace std;
using namespace brpc;
using namespace weight_raft;

int main(int argc, char* args[]) {
  if (argc != 2) {
    cout << "plz specify ip address !" << endl;
    return -1;
  }
  Controller cntl;
  WeightRequest req;
  WeightResponse resp;
  brpc::Channel channel;
  if (channel.Init(args[1], NULL) != 0) {
    throw runtime_error("namenode master channel init failed");
  }

  WeightService_Stub* stub_ptr = new WeightService_Stub(&channel);

  stub_ptr->getWeight(&cntl, &req, &resp, NULL);
  if (cntl.Failed()) {
    throw runtime_error("brpc not success: " + cntl.ErrorText());
  }
  if (!resp.success()) {
    throw runtime_error("logic error");
  }
  cout << resp.data() << endl;
  return 0;
}