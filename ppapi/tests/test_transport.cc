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
#include "ppapi/cpp/dev/transport_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Transport);

#define RUN_SUBTEST(function) { \
    std::string result = function; \
    if (!result.empty()) \
      return result; \
  }

namespace {

const char kTestChannelName[] = "test";
const int kReadBufferSize = 65536;

class StreamReader {
 public:
  StreamReader(pp::Transport_Dev* transport,
               int expected_size,
               pp::CompletionCallback done_callback)
      : expected_size_(expected_size),
        done_callback_(done_callback),
        PP_ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)),
        transport_(transport),
        received_size_(0) {
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
          callback_factory_.NewOptionalCallback(&StreamReader::OnReadFinished));
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
      received_size_ += buffer_.size();
      if (received_size_ >= expected_size_)
        done_callback_.Run(0);
    }
  }

  int expected_size_;
  pp::CompletionCallback done_callback_;
  pp::CompletionCallbackFactory<StreamReader> callback_factory_;
  pp::Transport_Dev* transport_;
  std::vector<char> buffer_;
  std::list<std::vector<char> > received_;
  int received_size_;
  std::list<std::string> errors_;
};

}  // namespace

TestTransport::~TestTransport() {
  delete transport1_;
  delete transport2_;
}

bool TestTransport::Init() {
  transport_interface_ = reinterpret_cast<PPB_Transport_Dev const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_TRANSPORT_DEV_INTERFACE));
  return transport_interface_ && InitTestingInterface();
}

void TestTransport::RunTests(const std::string& filter) {
  RUN_TEST(Create, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(Connect, filter);
  RUN_TEST(SetProperty, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(SendDataUdp, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(SendDataTcp, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(ConnectAndCloseUdp, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(ConnectAndCloseTcp, filter);
}

std::string TestTransport::InitTargets(PP_TransportType type) {
  transport1_ = new pp::Transport_Dev(instance_, kTestChannelName, type);
  transport2_ = new pp::Transport_Dev(instance_, kTestChannelName, type);

  ASSERT_NE(NULL, transport1_);
  ASSERT_NE(NULL, transport2_);

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
  delete transport1_;
  transport1_ = NULL;
  delete transport2_;
  transport2_ = NULL;

  PASS();
}

std::string TestTransport::TestCreate() {
  RUN_SUBTEST(InitTargets(PP_TRANSPORTTYPE_DATAGRAM));

  Clean();

  PASS();
}

std::string TestTransport::TestSetProperty() {
  RUN_SUBTEST(InitTargets(PP_TRANSPORTTYPE_STREAM));

  // Try settings STUN and Relay properties.
  ASSERT_EQ(transport1_->SetProperty(
      PP_TRANSPORTPROPERTY_STUN_SERVER,
      pp::Var("stun.example.com:19302")), PP_OK);

  ASSERT_EQ(transport1_->SetProperty(
      PP_TRANSPORTPROPERTY_RELAY_SERVER,
      pp::Var("ralay.example.com:80")), PP_OK);

  ASSERT_EQ(transport1_->SetProperty(
      PP_TRANSPORTPROPERTY_RELAY_USERNAME,
      pp::Var("USERNAME")), PP_OK);
  ASSERT_EQ(transport1_->SetProperty(
      PP_TRANSPORTPROPERTY_RELAY_PASSWORD,
      pp::Var("PASSWORD")), PP_OK);

  // Try changing TCP window size.
  ASSERT_EQ(transport1_->SetProperty(PP_TRANSPORTPROPERTY_TCP_RECEIVE_WINDOW,
                                     pp::Var(65536)), PP_OK);
  ASSERT_EQ(transport1_->SetProperty(PP_TRANSPORTPROPERTY_TCP_RECEIVE_WINDOW,
                                     pp::Var(10000000)), PP_ERROR_BADARGUMENT);

  ASSERT_EQ(transport1_->SetProperty(PP_TRANSPORTPROPERTY_TCP_SEND_WINDOW,
                                     pp::Var(65536)), PP_OK);
  ASSERT_EQ(transport1_->SetProperty(PP_TRANSPORTPROPERTY_TCP_SEND_WINDOW,
                                     pp::Var(10000000)), PP_ERROR_BADARGUMENT);

  ASSERT_EQ(transport1_->SetProperty(PP_TRANSPORTPROPERTY_TCP_NO_DELAY,
                                     pp::Var(true)), PP_OK);

  ASSERT_EQ(transport1_->SetProperty(PP_TRANSPORTPROPERTY_TCP_ACK_DELAY,
                                     pp::Var(10)), PP_OK);
  ASSERT_EQ(transport1_->SetProperty(PP_TRANSPORTPROPERTY_TCP_ACK_DELAY,
                                     pp::Var(10000)), PP_ERROR_BADARGUMENT);

  TestCompletionCallback connect_cb(instance_->pp_instance());
  ASSERT_EQ(transport1_->Connect(connect_cb), PP_OK_COMPLETIONPENDING);

  // SetProperty() should fail after we've connected.
  ASSERT_EQ(transport1_->SetProperty(
      PP_TRANSPORTPROPERTY_STUN_SERVER,
      pp::Var("stun.example.com:31323")), PP_ERROR_FAILED);

  Clean();
  ASSERT_EQ(connect_cb.WaitForResult(), PP_ERROR_ABORTED);

  PASS();
}

std::string TestTransport::TestConnect() {
  RUN_SUBTEST(InitTargets(PP_TRANSPORTTYPE_DATAGRAM));
  RUN_SUBTEST(Connect());

  Clean();

  PASS();
}

// Creating datagram connection and try sending data over it. Verify
// that at least some packets are received (some packets may be lost).
std::string TestTransport::TestSendDataUdp() {
  RUN_SUBTEST(InitTargets(PP_TRANSPORTTYPE_DATAGRAM));
  RUN_SUBTEST(Connect());

  const int kNumPackets = 100;
  const int kSendBufferSize = 1200;
  const int kUdpWaitTimeMs = 1000;  // 1 second.

  TestCompletionCallback done_cb(instance_->pp_instance());
  StreamReader reader(transport1_, kSendBufferSize * kNumPackets, done_cb);

  std::map<int, std::vector<char> > sent_packets;
  for (int i = 0; i < kNumPackets; ++i) {
    std::vector<char> send_buffer(kSendBufferSize);
    for (size_t j = 0; j < send_buffer.size(); ++j) {
      send_buffer[j] = rand() % 256;
    }
    // Put packet index in the beginning.
    memcpy(&send_buffer[0], &i, sizeof(i));

    TestCompletionCallback send_cb(instance_->pp_instance(), force_async_);
    int32_t result = transport2_->Send(&send_buffer[0], send_buffer.size(),
                                       send_cb);
    if (force_async_) {
      ASSERT_EQ(result, PP_OK_COMPLETIONPENDING);
      ASSERT_EQ(send_cb.WaitForResult(), static_cast<int>(send_buffer.size()));
    } else {
      ASSERT_EQ(result, static_cast<int>(send_buffer.size()));
    }
    sent_packets[i] = send_buffer;
  }

  // Limit waiting time.
  TestCompletionCallback timeout_cb(instance_->pp_instance());
  pp::Module::Get()->core()->CallOnMainThread(kUdpWaitTimeMs, timeout_cb);
  ASSERT_EQ(timeout_cb.WaitForResult(), PP_OK);

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

// Creating reliable (TCP-like) connection and try sending data over
// it. Verify that all data is received correctly.
std::string TestTransport::TestSendDataTcp() {
  RUN_SUBTEST(InitTargets(PP_TRANSPORTTYPE_STREAM));
  RUN_SUBTEST(Connect());

  const int kTcpSendSize = 100000;

  TestCompletionCallback done_cb(instance_->pp_instance());
  StreamReader reader(transport1_, kTcpSendSize, done_cb);

  std::vector<char> send_buffer(kTcpSendSize);
  for (size_t j = 0; j < send_buffer.size(); ++j) {
    send_buffer[j] = rand() % 256;
  }

  int pos = 0;
  while (pos < static_cast<int>(send_buffer.size())) {
    TestCompletionCallback send_cb(instance_->pp_instance(), force_async_);
    int result = transport2_->Send(
        &send_buffer[0] + pos, send_buffer.size() - pos, send_cb);
    if (force_async_)
      ASSERT_EQ(result, PP_OK_COMPLETIONPENDING);
    if (result == PP_OK_COMPLETIONPENDING)
      result = send_cb.WaitForResult();
    ASSERT_TRUE(result > 0);
    pos += result;
  }

  ASSERT_EQ(done_cb.WaitForResult(), PP_OK);

  ASSERT_TRUE(reader.errors().size() == 0);

  std::vector<char> received_data;
  for (std::list<std::vector<char> >::const_iterator it =
           reader.received().begin(); it != reader.received().end(); ++it) {
    received_data.insert(received_data.end(), it->begin(), it->end());
  }
  ASSERT_EQ(send_buffer, received_data);

  Clean();

  PASS();
}

std::string TestTransport::TestConnectAndCloseUdp() {
  RUN_SUBTEST(InitTargets(PP_TRANSPORTTYPE_DATAGRAM));
  RUN_SUBTEST(Connect());

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

std::string TestTransport::TestConnectAndCloseTcp() {
  RUN_SUBTEST(InitTargets(PP_TRANSPORTTYPE_STREAM));
  RUN_SUBTEST(Connect());

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
