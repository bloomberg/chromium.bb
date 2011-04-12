// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_transport.h"

#include <stdlib.h>
#include <string.h>

#include <list>
#include <map>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/dev/transport_dev.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Transport);

#define RUN_SUBTEST(function) { \
    std::string result = function(); \
    if (!result.empty()) \
      return result; \
  }

namespace {

const char kTestChannelName[] = "test";
const int kNumPackets = 100;
const int kSendBufferSize = 1200;
const int kReadBufferSize = 65536;

class StreamReader {
 public:
  explicit StreamReader(pp::Transport_Dev* transport)
      : ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)),
        transport_(transport),
        errors_(0) {
    Read();
  }

  const std::list<std::vector<char> >& received() { return received_; }
  std::list<std::string> errors() { return errors_; }

 private:
  void Read() {
    while (true) {
      buffer_.resize(kReadBufferSize);
      int result = transport_->Recv(
          &buffer_[0], buffer_.size(),
          callback_factory_.NewCallback(&StreamReader::OnReadFinished));
      if (result > 0)
        DidFinishRead(result);
      else
        break;
    }
  }

  void OnReadFinished(int result) {
    DidFinishRead(result);
    if (result > 0)
      Read();
  }

  void DidFinishRead(int result) {
    if (result > 0) {
      if (result > static_cast<int>(buffer_.size())) {
        errors_.push_back(TestCase::MakeFailureMessage(
            __FILE__, __LINE__,
            "Recv() returned value that is bigger than the buffer."));
      }
      buffer_.resize(result);
      received_.push_back(buffer_);
    }
  }

  pp::CompletionCallbackFactory<StreamReader> callback_factory_;
  pp::Transport_Dev* transport_;
  std::vector<char> buffer_;
  std::list<std::vector<char> > received_;
  std::list<std::string> errors_;
};

}  // namespace

bool TestTransport::Init() {
  transport_interface_ = reinterpret_cast<PPB_Transport_Dev const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_TRANSPORT_DEV_INTERFACE));
  return transport_interface_ && InitTestingInterface();
}

void TestTransport::RunTest() {
  RUN_TEST(Create);
  RUN_TEST(Connect);
  RUN_TEST(SendData);
  RUN_TEST(ConnectAndClose);
}

std::string TestTransport::InitTargets() {
  transport1_.reset(new pp::Transport_Dev(instance_, kTestChannelName, ""));
  transport2_.reset(new pp::Transport_Dev(instance_, kTestChannelName, ""));

  ASSERT_TRUE(transport1_.get() != NULL);
  ASSERT_TRUE(transport2_.get() != NULL);

  PASS();
}

std::string TestTransport::Connect() {
  TestCompletionCallback connect_cb1(instance_->pp_instance());
  TestCompletionCallback connect_cb2(instance_->pp_instance());
  ASSERT_EQ(transport1_->Connect(connect_cb1), PP_OK_COMPLETIONPENDING);
  ASSERT_EQ(transport2_->Connect(connect_cb2), PP_OK_COMPLETIONPENDING);

  pp::Var address1;
  pp::Var address2;
  TestCompletionCallback next_address_cb1(instance_->pp_instance());
  TestCompletionCallback next_address_cb2(instance_->pp_instance());
  ASSERT_EQ(transport1_->GetNextAddress(&address1, next_address_cb1),
            PP_OK_COMPLETIONPENDING);
  ASSERT_EQ(transport2_->GetNextAddress(&address2, next_address_cb2),
            PP_OK_COMPLETIONPENDING);
  ASSERT_EQ(next_address_cb1.WaitForResult(), PP_OK);
  ASSERT_EQ(next_address_cb2.WaitForResult(), PP_OK);
  ASSERT_EQ(transport1_->GetNextAddress(&address1, next_address_cb1), PP_OK);
  ASSERT_EQ(transport2_->GetNextAddress(&address2, next_address_cb2), PP_OK);

  ASSERT_EQ(transport1_->ReceiveRemoteAddress(address2), PP_OK);
  ASSERT_EQ(transport2_->ReceiveRemoteAddress(address1), PP_OK);

  ASSERT_EQ(connect_cb1.WaitForResult(), PP_OK);
  ASSERT_EQ(connect_cb2.WaitForResult(), PP_OK);

  ASSERT_TRUE(transport1_->IsWritable());
  ASSERT_TRUE(transport2_->IsWritable());

  PASS();
}

std::string TestTransport::Clean() {
  transport1_.reset();
  transport2_.reset();

  PASS();
}

std::string TestTransport::TestCreate() {
  RUN_SUBTEST(InitTargets);

  Clean();

  PASS();
}

std::string TestTransport::TestConnect() {
  RUN_SUBTEST(InitTargets);
  RUN_SUBTEST(Connect);

  Clean();

  PASS();
}

std::string TestTransport::TestSendData() {
  RUN_SUBTEST(InitTargets);
  RUN_SUBTEST(Connect);

  StreamReader reader(transport1_.get());

  std::map<int, std::vector<char> > sent_packets;
  for (int i = 0; i < kNumPackets; ++i) {
    std::vector<char> send_buffer(kSendBufferSize);
    for (size_t j = 0; j < send_buffer.size(); ++j) {
      send_buffer[j] = rand() % 256;
    }
    // Put packet index in the beginning.
    memcpy(&send_buffer[0], &i, sizeof(i));

    TestCompletionCallback send_cb(instance_->pp_instance());
    ASSERT_EQ(
        transport2_->Send(&send_buffer[0], send_buffer.size(), send_cb),
        static_cast<int>(send_buffer.size()));
    sent_packets[i] = send_buffer;
  }

  // Wait for 1 second.
  TestCompletionCallback wait_cb(instance_->pp_instance());
  pp::Module::Get()->core()->CallOnMainThread(1000, wait_cb);
  ASSERT_EQ(wait_cb.WaitForResult(), PP_OK);

  ASSERT_TRUE(reader.errors().size() == 0);
  ASSERT_TRUE(reader.received().size() > 0);
  for (std::list<std::vector<char> >::const_iterator it =
           reader.received().begin(); it != reader.received().end(); ++it) {
    int index;
    memcpy(&index, &(*it)[0], sizeof(index));
    ASSERT_TRUE(sent_packets[index] == *it);
  }

  Clean();

  PASS();
}

std::string TestTransport::TestConnectAndClose() {
  RUN_SUBTEST(InitTargets);
  RUN_SUBTEST(Connect);

  std::vector<char> recv_buffer(kReadBufferSize);
  TestCompletionCallback recv_cb(instance_->pp_instance());
  ASSERT_EQ(
      transport1_->Recv(&recv_buffer[0], recv_buffer.size(), recv_cb),
      PP_OK_COMPLETIONPENDING);

  // Close the transport and verify that callback is aborted.
  ASSERT_EQ(transport1_->Close(), PP_OK);

  ASSERT_EQ(recv_cb.run_count(), 1);
  ASSERT_EQ(recv_cb.result(), PP_ERROR_ABORTED);

  Clean();

  PASS();
}
