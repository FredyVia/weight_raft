#include <brpc/channel.h>
#include <brpc/controller.h>  // brpc::Controller

int main(int argc, char* args[]) {
  if (argc != 2) {
    cout << "plz specify ip address !" << endl;
    return -1;
  }
  Controller cntl;
  WeightRequest req;
  WeightResponse resp;
  brpc::Channel channel;
  if (channel.Init(string(args[1]).c_str(), NULL) != 0) {
    throw runtime_error("namenode master channel init failed");
  }
  stub_ptr = new WeightService_Stub(&channel);

  stub_ptr->get_namenodes(&cntl, &req, &resp, NULL);
  if (cntl.Failed()) {
    throw runtime_error("brpc not success: " + cntl.ErrorText());
  }
  if (!response.success()) {
    throw runtime_error("logic error");
  }
  cout << response.data() << endl;
  return 0;
}