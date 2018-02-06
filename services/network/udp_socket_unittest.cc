// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <utility>

#include "services/network/udp_socket.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/net_errors.h"
#include "net/socket/udp_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/interfaces/udp_socket.mojom.h"
#include "services/network/udp_socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

const size_t kDatagramSize = 255;

class SocketWrapperTestImpl : public UDPSocket::SocketWrapper {
 public:
  SocketWrapperTestImpl() {}
  ~SocketWrapperTestImpl() override {}

  int Open(net::AddressFamily address_family) override {
    NOTREACHED();
    return net::ERR_NOT_IMPLEMENTED;
  }
  int Connect(const net::IPEndPoint& remote_addr) override {
    NOTREACHED();
    return net::ERR_NOT_IMPLEMENTED;
  }
  int Bind(const net::IPEndPoint& local_addr) override {
    NOTREACHED();
    return net::ERR_NOT_IMPLEMENTED;
  }
  int SetSendBufferSize(uint32_t size) override {
    NOTREACHED();
    return net::ERR_NOT_IMPLEMENTED;
  }
  int SetReceiveBufferSize(uint32_t size) override {
    NOTREACHED();
    return net::ERR_NOT_IMPLEMENTED;
  }
  int SendTo(
      net::IOBuffer* buf,
      int buf_len,
      const net::IPEndPoint& dest_addr,
      const net::CompletionCallback& callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    NOTREACHED();
    return net::ERR_NOT_IMPLEMENTED;
  }
  int SetBroadcast(bool broadcast) override {
    NOTREACHED();
    return net::ERR_NOT_IMPLEMENTED;
  }
  int JoinGroup(const net::IPAddress& group_address) override {
    NOTREACHED();
    return net::ERR_NOT_IMPLEMENTED;
  }
  int LeaveGroup(const net::IPAddress& group_address) override {
    NOTREACHED();
    return net::ERR_NOT_IMPLEMENTED;
  }
  int Write(
      net::IOBuffer* buf,
      int buf_len,
      const net::CompletionCallback& callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    NOTREACHED();
    return net::ERR_NOT_IMPLEMENTED;
  }
  int RecvFrom(net::IOBuffer* buf,
               int buf_len,
               net::IPEndPoint* address,
               const net::CompletionCallback& callback) override {
    NOTREACHED();
    return net::ERR_NOT_IMPLEMENTED;
  }
  int GetLocalAddress(net::IPEndPoint* address) const override {
    NOTREACHED();
    return net::ERR_NOT_IMPLEMENTED;
  }
  int ConfigureOptions(mojom::UDPSocketOptionsPtr options) override {
    NOTREACHED();
    return net::ERR_NOT_IMPLEMENTED;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SocketWrapperTestImpl);
};

net::IPEndPoint GetLocalHostWithAnyPort() {
  return net::IPEndPoint(net::IPAddress(127, 0, 0, 1), 0);
}

std::vector<uint8_t> CreateTestMessage(uint8_t initial, size_t size) {
  std::vector<uint8_t> array(size);
  for (size_t i = 0; i < size; ++i)
    array[i] = static_cast<uint8_t>((i + initial) % 256);
  return array;
}

// A Mock UDPSocket that always returns net::ERR_IO_PENDING for SendTo()s.
class HangingUDPSocket : public SocketWrapperTestImpl {
 public:
  HangingUDPSocket() {}

  // SocketWrapperTestImpl implementation.
  int Open(net::AddressFamily address_family) override { return net::OK; }
  int Bind(const net::IPEndPoint& local_addr) override { return net::OK; }
  int SendTo(
      net::IOBuffer* buf,
      int buf_len,
      const net::IPEndPoint& address,
      const net::CompletionCallback& callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    EXPECT_EQ(expected_data_,
              std::vector<unsigned char>(buf->data(), buf->data() + buf_len));
    if (should_complete_requests_)
      return net::OK;
    pending_io_buffers_.push_back(buf);
    pending_io_buffer_lengths_.push_back(buf_len);
    pending_send_requests_.push_back(callback);
    return net::ERR_IO_PENDING;
  }
  int GetLocalAddress(net::IPEndPoint* address) const override {
    *address = GetLocalHostWithAnyPort();
    return net::OK;
  }

  void set_expected_data(std::vector<uint8_t> expected_data) {
    expected_data_ = expected_data;
  }

  const std::vector<net::IOBuffer*>& pending_io_buffers() const {
    return pending_io_buffers_;
  }

  const std::vector<int>& pending_io_buffer_lengths() const {
    return pending_io_buffer_lengths_;
  }

  // Completes all pending requests.
  void CompleteAllPendingRequests() {
    should_complete_requests_ = true;
    for (auto request : pending_send_requests_) {
      request.Run(net::OK);
    }
    pending_send_requests_.clear();
  }

 private:
  std::vector<uint8_t> expected_data_;
  bool should_complete_requests_ = false;
  std::vector<net::IOBuffer*> pending_io_buffers_;
  std::vector<int> pending_io_buffer_lengths_;
  std::vector<net::CompletionCallback> pending_send_requests_;
};

// A Mock UDPSocket that returns 0 byte read.
class ZeroByteReadUDPSocket : public SocketWrapperTestImpl {
 public:
  ZeroByteReadUDPSocket() {}
  int Open(net::AddressFamily address_family) override { return net::OK; }
  int Bind(const net::IPEndPoint& local_addr) override { return net::OK; }
  int RecvFrom(net::IOBuffer* buf,
               int buf_len,
               net::IPEndPoint* address,
               const net::CompletionCallback& callback) override {
    *address = GetLocalHostWithAnyPort();
    return 0;
  }
  int GetLocalAddress(net::IPEndPoint* address) const override {
    *address = GetLocalHostWithAnyPort();
    return net::OK;
  }
};

}  // namespace

class UDPSocketTest : public testing::Test {
 public:
  UDPSocketTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {}
  ~UDPSocketTest() override {}

  void SetWrappedSocket(
      UDPSocket* socket,
      std::unique_ptr<UDPSocket::SocketWrapper> socket_wrapper) {
    socket->wrapped_socket_ = std::move(socket_wrapper);
  }

  uint32_t GetRemainingRecvSlots(UDPSocket* socket) {
    return socket->remaining_recv_slots_;
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocketTest);
};

TEST_F(UDPSocketTest, Settings) {
  mojom::UDPSocketReceiverPtr receiver_interface_ptr;
  mojom::UDPSocketPtr socket_ptr;
  UDPSocket impl(mojo::MakeRequest(&socket_ptr),
                 std::move(receiver_interface_ptr));
  net::IPEndPoint server_addr;
  net::IPEndPoint any_port(GetLocalHostWithAnyPort());

  test::UDPSocketTestHelper helper(&socket_ptr);
  ASSERT_EQ(net::OK, helper.OpenSync(any_port.GetFamily()));
  net::IPEndPoint local_addr;
  ASSERT_EQ(net::OK, helper.BindSync(any_port, &local_addr));
  EXPECT_NE(local_addr.ToString(), any_port.ToString());
  EXPECT_EQ(net::OK, helper.SetSendBufferSizeSync(1024));
  EXPECT_EQ(net::OK, helper.SetReceiveBufferSizeSync(2048));
}

// Tests that Send() is used after Bind() is not supported. Send() should only
// be used after Connect().
TEST_F(UDPSocketTest, TestSendWithBind) {
  mojom::UDPSocketReceiverPtr receiver_interface_ptr;
  mojom::UDPSocketPtr socket_ptr;
  UDPSocket impl(mojo::MakeRequest(&socket_ptr),
                 std::move(receiver_interface_ptr));

  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());

  // Bind() the socket.
  test::UDPSocketTestHelper helper(&socket_ptr);
  ASSERT_EQ(net::OK, helper.OpenSync(server_addr.GetFamily()));
  ASSERT_EQ(net::OK, helper.BindSync(server_addr, &server_addr));

  // Connect() has not been used, so Send() is not supported.
  std::vector<uint8_t> test_msg{1};
  int result = helper.SendSync(test_msg);
  EXPECT_EQ(net::ERR_UNEXPECTED, result);
}

// Tests that when SendTo() is used after Connect() is not supported. SendTo()
// should only be used after Bind().
TEST_F(UDPSocketTest, TestSendToWithConnect) {
  // Create a server socket to listen for incoming datagrams.
  test::UDPSocketReceiverImpl receiver;
  mojo::Binding<mojom::UDPSocketReceiver> receiver_binding(&receiver);
  mojom::UDPSocketReceiverPtr receiver_interface_ptr;
  receiver_binding.Bind(mojo::MakeRequest(&receiver_interface_ptr));

  mojom::UDPSocketPtr server_socket;
  UDPSocket impl(mojo::MakeRequest(&server_socket),
                 std::move(receiver_interface_ptr));

  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());
  test::UDPSocketTestHelper helper(&server_socket);
  ASSERT_EQ(net::OK, helper.OpenSync(server_addr.GetFamily()));
  ASSERT_EQ(net::OK, helper.BindSync(server_addr, &server_addr));

  // Create a client socket to send datagrams.
  mojom::UDPSocketReceiverPtr client_receiver_ptr;
  mojom::UDPSocketPtr client_socket;
  UDPSocket client_impl(mojo::MakeRequest(&client_socket),
                        std::move(client_receiver_ptr));
  net::IPEndPoint client_addr(GetLocalHostWithAnyPort());
  test::UDPSocketTestHelper client_helper(&client_socket);
  ASSERT_EQ(net::OK, client_helper.OpenSync(client_addr.GetFamily()));
  ASSERT_EQ(net::OK, client_helper.ConnectSync(server_addr, &client_addr));

  std::vector<uint8_t> test_msg({1});
  int result = client_helper.SendToSync(server_addr, test_msg);
  EXPECT_EQ(net::ERR_UNEXPECTED, result);
}

// Tests that the sequence of calling Open(), Bind()/Connect() and setters is
// important.
TEST_F(UDPSocketTest, TestUnexpectedSequences) {
  mojom::UDPSocketReceiverPtr receiver_interface_ptr;
  mojom::UDPSocketPtr socket_ptr;
  UDPSocket impl(mojo::MakeRequest(&socket_ptr),
                 std::move(receiver_interface_ptr));
  net::IPEndPoint server_addr;
  net::IPEndPoint any_port(GetLocalHostWithAnyPort());

  // Before Open(), calling Connect() or Bind() will result in an error.
  net::IPEndPoint local_addr;
  test::UDPSocketTestHelper helper(&socket_ptr);
  ASSERT_EQ(net::ERR_UNEXPECTED, helper.BindSync(any_port, &local_addr));
  ASSERT_EQ(net::ERR_UNEXPECTED, helper.ConnectSync(any_port, &local_addr));

  // Calling any Setters that depend on Open() should fail.
  EXPECT_EQ(net::ERR_UNEXPECTED, helper.SetSendBufferSizeSync(1024));
  EXPECT_EQ(net::ERR_UNEXPECTED, helper.SetReceiveBufferSizeSync(2048));

  // Now call Open().
  ASSERT_EQ(net::OK, helper.OpenSync(any_port.GetFamily()));
  EXPECT_NE(local_addr.ToString(), any_port.ToString());

  // It is illegal call Open() twice.
  ASSERT_EQ(net::ERR_UNEXPECTED, helper.OpenSync(any_port.GetFamily()));

  // Now these Setters should work.
  EXPECT_EQ(net::OK, helper.SetSendBufferSizeSync(1024));
  EXPECT_EQ(net::OK, helper.SetReceiveBufferSizeSync(2048));

  // Now Bind() the socket.
  ASSERT_EQ(net::OK, helper.BindSync(any_port, &local_addr));

  // Calling Connect() after Bind() should fail because they can't be both used.
  ASSERT_EQ(net::ERR_SOCKET_IS_CONNECTED,
            helper.ConnectSync(any_port, &local_addr));
}

// Tests that if the underlying socket implementation's Send() returned
// ERR_IO_PENDING, udp_socket.cc doesn't free the send buffer.
TEST_F(UDPSocketTest, TestBufferValid) {
  mojom::UDPSocketReceiverPtr receiver_interface_ptr;
  mojom::UDPSocketPtr socket_ptr;
  UDPSocket impl(mojo::MakeRequest(&socket_ptr),
                 std::move(receiver_interface_ptr));

  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());
  test::UDPSocketTestHelper helper(&socket_ptr);
  ASSERT_EQ(net::OK, helper.OpenSync(server_addr.GetFamily()));
  ASSERT_EQ(net::OK, helper.BindSync(server_addr, &server_addr));

  HangingUDPSocket* socket_raw_ptr = new HangingUDPSocket();
  SetWrappedSocket(&impl, base::WrapUnique(socket_raw_ptr));

  std::vector<uint8_t> test_msg(CreateTestMessage(0, kDatagramSize));
  socket_raw_ptr->set_expected_data(test_msg);
  base::RunLoop run_loop;
  socket_ptr->SendTo(
      GetLocalHostWithAnyPort(), test_msg,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      base::BindOnce(
          [](base::RunLoop* run_loop, int result) {
            EXPECT_EQ(net::OK, result);
            run_loop->Quit();
          },
          base::Unretained(&run_loop)));

  socket_ptr.FlushForTesting();

  ASSERT_EQ(1u, socket_raw_ptr->pending_io_buffers().size());
  ASSERT_EQ(1u, socket_raw_ptr->pending_io_buffer_lengths().size());
  // Make sure the caller of HangingUDPSocket doesn't destroy the send buffer,
  // and that buffer still contains the exact same data.
  net::IOBuffer* buf = socket_raw_ptr->pending_io_buffers()[0];
  int buf_len = socket_raw_ptr->pending_io_buffer_lengths()[0];
  EXPECT_EQ(test_msg,
            std::vector<unsigned char>(buf->data(), buf->data() + buf_len));
}

// Test that exercises the queuing of send requests and makes sure
// ERR_INSUFFICIENT_RESOURCES is returned appropriately.
TEST_F(UDPSocketTest, TestInsufficientResources) {
  mojom::UDPSocketReceiverPtr receiver_interface_ptr;
  mojom::UDPSocketPtr socket_ptr;
  UDPSocket impl(mojo::MakeRequest(&socket_ptr),
                 std::move(receiver_interface_ptr));

  const size_t kQueueSize = UDPSocket::kMaxPendingSendRequests;

  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());
  test::UDPSocketTestHelper helper(&socket_ptr);
  ASSERT_EQ(net::OK, helper.OpenSync(server_addr.GetFamily()));
  ASSERT_EQ(net::OK, helper.BindSync(server_addr, &server_addr));

  HangingUDPSocket* socket_raw_ptr = new HangingUDPSocket();
  SetWrappedSocket(&impl, base::WrapUnique(socket_raw_ptr));

  std::vector<uint8_t> test_msg(CreateTestMessage(0, kDatagramSize));
  socket_raw_ptr->set_expected_data(test_msg);
  // Send |kQueueSize| + 1 datagrams in a row, so the queue is filled up.
  std::vector<std::unique_ptr<base::RunLoop>> run_loops;
  for (size_t i = 0; i < kQueueSize + 1; ++i) {
    run_loops.push_back(std::make_unique<base::RunLoop>());
    socket_ptr->SendTo(
        GetLocalHostWithAnyPort(), test_msg,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        base::BindOnce(
            [](base::RunLoop* run_loop, int result) {
              EXPECT_EQ(net::OK, result);
              run_loop->Quit();
            },
            base::Unretained(run_loops[i].get())));
  }

  // SendTo() beyond the queue size should fail.
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            helper.SendToSync(GetLocalHostWithAnyPort(), test_msg));

  // Complete all pending requests. Queued SendTo() should hear back.
  socket_raw_ptr->CompleteAllPendingRequests();
  for (const auto& loop : run_loops) {
    loop->Run();
  }
}

TEST_F(UDPSocketTest, TestReceiveMoreOverflow) {
  // Create a server socket to listen for incoming datagrams.
  test::UDPSocketReceiverImpl receiver;
  mojo::Binding<mojom::UDPSocketReceiver> receiver_binding(&receiver);
  mojom::UDPSocketReceiverPtr receiver_interface_ptr;
  receiver_binding.Bind(mojo::MakeRequest(&receiver_interface_ptr));

  mojom::UDPSocketPtr server_socket;
  UDPSocket impl(mojo::MakeRequest(&server_socket),
                 std::move(receiver_interface_ptr));

  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());
  test::UDPSocketTestHelper helper(&server_socket);
  ASSERT_EQ(net::OK, helper.OpenSync(server_addr.GetFamily()));
  ASSERT_EQ(net::OK, helper.BindSync(server_addr, &server_addr));

  server_socket->ReceiveMore(std::numeric_limits<uint32_t>::max());
  server_socket.FlushForTesting();
  EXPECT_EQ(std::numeric_limits<uint32_t>::max(), GetRemainingRecvSlots(&impl));
  server_socket->ReceiveMore(1);
  server_socket.FlushForTesting();
  EXPECT_EQ(std::numeric_limits<uint32_t>::max(), GetRemainingRecvSlots(&impl));
}

TEST_F(UDPSocketTest, TestReadSend) {
  // Create a server socket to listen for incoming datagrams.
  test::UDPSocketReceiverImpl receiver;
  mojo::Binding<mojom::UDPSocketReceiver> receiver_binding(&receiver);
  mojom::UDPSocketReceiverPtr receiver_interface_ptr;
  receiver_binding.Bind(mojo::MakeRequest(&receiver_interface_ptr));

  mojom::UDPSocketPtr server_socket;
  UDPSocket impl(mojo::MakeRequest(&server_socket),
                 std::move(receiver_interface_ptr));

  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());
  test::UDPSocketTestHelper helper(&server_socket);
  ASSERT_EQ(net::OK, helper.OpenSync(server_addr.GetFamily()));
  ASSERT_EQ(net::OK, helper.BindSync(server_addr, &server_addr));

  // Create a client socket to send datagrams.
  mojom::UDPSocketReceiverPtr client_receiver_ptr;
  mojom::UDPSocketPtr client_socket;
  UDPSocket client_impl(mojo::MakeRequest(&client_socket),
                        std::move(client_receiver_ptr));
  net::IPEndPoint client_addr(GetLocalHostWithAnyPort());
  test::UDPSocketTestHelper client_helper(&client_socket);
  ASSERT_EQ(net::OK, client_helper.OpenSync(client_addr.GetFamily()));
  ASSERT_EQ(net::OK, client_helper.ConnectSync(server_addr, &client_addr));

  const size_t kDatagramCount = 6;
  server_socket->ReceiveMore(kDatagramCount);

  for (size_t i = 0; i < kDatagramCount; ++i) {
    std::vector<uint8_t> test_msg(
        CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize));
    int result = client_helper.SendSync(test_msg);
    EXPECT_EQ(net::OK, result);
  }

  receiver.WaitForReceivedResults(kDatagramCount);
  EXPECT_EQ(kDatagramCount, receiver.results().size());

  int i = 0;
  for (const auto& result : receiver.results()) {
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_EQ(result.src_addr, client_addr);
    EXPECT_EQ(CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize),
              result.data.value());
    i++;
  }
  // Tests that sending a message that is larger than the specified limit
  // results in an early rejection.
  std::vector<uint8_t> large_msg(64 * 1024, 1);
  EXPECT_EQ(net::ERR_MSG_TOO_BIG, helper.SendToSync(client_addr, large_msg));
}

TEST_F(UDPSocketTest, TestReadSendTo) {
  // Create a server socket to send data.
  mojom::UDPSocketPtr server_socket;
  mojom::UDPSocketReceiverPtr receiver_ptr;
  UDPSocket impl(mojo::MakeRequest(&server_socket), std::move(receiver_ptr));

  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());
  test::UDPSocketTestHelper helper(&server_socket);
  ASSERT_EQ(net::OK, helper.OpenSync(server_addr.GetFamily()));
  ASSERT_EQ(net::OK, helper.BindSync(server_addr, &server_addr));

  // Create a client socket to send datagrams.
  test::UDPSocketReceiverImpl receiver;
  mojo::Binding<mojom::UDPSocketReceiver> receiver_binding(&receiver);
  mojom::UDPSocketReceiverPtr client_receiver_ptr;
  receiver_binding.Bind(mojo::MakeRequest(&client_receiver_ptr));

  mojom::UDPSocketPtr client_socket;
  UDPSocket client_impl(mojo::MakeRequest(&client_socket),
                        std::move(client_receiver_ptr));
  net::IPEndPoint client_addr(GetLocalHostWithAnyPort());
  test::UDPSocketTestHelper client_helper(&client_socket);
  ASSERT_EQ(net::OK, client_helper.OpenSync(client_addr.GetFamily()));
  ASSERT_EQ(net::OK, client_helper.ConnectSync(server_addr, &client_addr));

  const size_t kDatagramCount = 6;
  client_socket->ReceiveMore(kDatagramCount);

  for (size_t i = 0; i < kDatagramCount; ++i) {
    std::vector<uint8_t> test_msg(
        CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize));
    int result = helper.SendToSync(client_addr, test_msg);
    EXPECT_EQ(net::OK, result);
  }

  receiver.WaitForReceivedResults(kDatagramCount);
  EXPECT_EQ(kDatagramCount, receiver.results().size());

  int i = 0;
  for (const auto& result : receiver.results()) {
    EXPECT_EQ(net::OK, result.net_error);
    EXPECT_FALSE(result.src_addr);
    EXPECT_EQ(CreateTestMessage(static_cast<uint8_t>(i), kDatagramSize),
              result.data.value());
    i++;
  }

  // Tests that sending a message that is larger than the specified limit
  // results in an early rejection.
  std::vector<uint8_t> large_msg(64 * 1024, 1);
  EXPECT_EQ(net::ERR_MSG_TOO_BIG, helper.SendToSync(client_addr, large_msg));
}

// Make sure passing an invalid net::IPEndPoint will be detected by
// serialization/deserialization in mojo.
TEST_F(UDPSocketTest, TestSendToInvalidAddress) {
  mojom::UDPSocketReceiverPtr receiver_interface_ptr;
  mojom::UDPSocketPtr server_socket;
  UDPSocket impl(mojo::MakeRequest(&server_socket),
                 std::move(receiver_interface_ptr));

  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());
  test::UDPSocketTestHelper helper(&server_socket);
  ASSERT_EQ(net::OK, helper.OpenSync(server_addr.GetFamily()));
  ASSERT_EQ(net::OK, helper.BindSync(server_addr, &server_addr));

  std::vector<uint8_t> test_msg{1};
  std::vector<uint8_t> invalid_ip_addr{127, 0, 0, 0, 1};
  net::IPAddress ip_address(invalid_ip_addr.data(), invalid_ip_addr.size());
  EXPECT_FALSE(ip_address.IsValid());
  net::IPEndPoint invalid_addr(ip_address, 53);
  server_socket->SendTo(
      invalid_addr, test_msg,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      base::BindOnce([](int result) {}));
  // Make sure that the pipe is broken upon processing |invalid_addr|.
  base::RunLoop run_loop;
  server_socket.set_connection_error_handler(
      base::BindOnce([](base::RunLoop* run_loop) { run_loop->Quit(); },
                     base::Unretained(&run_loop)));
  run_loop.Run();
}

// Tests that it is legal for UDPSocketReceiver::OnReceive() to be called with
// 0 byte payload.
TEST_F(UDPSocketTest, TestReadZeroByte) {
  test::UDPSocketReceiverImpl receiver;
  mojo::Binding<mojom::UDPSocketReceiver> receiver_binding(&receiver);
  mojom::UDPSocketReceiverPtr receiver_interface_ptr;
  receiver_binding.Bind(mojo::MakeRequest(&receiver_interface_ptr));

  mojom::UDPSocketPtr socket_ptr;
  UDPSocket impl(mojo::MakeRequest(&socket_ptr),
                 std::move(receiver_interface_ptr));

  net::IPEndPoint server_addr(GetLocalHostWithAnyPort());
  test::UDPSocketTestHelper helper(&socket_ptr);
  ASSERT_EQ(net::OK, helper.OpenSync(server_addr.GetFamily()));
  ASSERT_EQ(net::OK, helper.BindSync(server_addr, &server_addr));

  SetWrappedSocket(&impl, std::make_unique<ZeroByteReadUDPSocket>());

  socket_ptr->ReceiveMore(1);

  receiver.WaitForReceivedResults(1);
  ASSERT_EQ(1u, receiver.results().size());

  auto result = receiver.results()[0];
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_TRUE(result.data);
  EXPECT_EQ(std::vector<uint8_t>(), result.data.value());
}

#if defined(OS_ANDROID)
// Some Android devices do not support multicast socket.
// The ones supporting multicast need WifiManager.MulticastLock to enable it.
// https://developer.android.com/reference/android/net/wifi/WifiManager.MulticastLock.html
#define MAYBE_JoinMulticastGroup DISABLED_JoinMulticastGroup
#else
#define MAYBE_JoinMulticastGroup JoinMulticastGroup
#endif  // defined(OS_ANDROID)
TEST_F(UDPSocketTest, MAYBE_JoinMulticastGroup) {
  const char kGroup[] = "237.132.100.17";

  net::IPAddress group_ip;
  EXPECT_TRUE(group_ip.AssignFromIPLiteral(kGroup));
  mojom::UDPSocketPtr socket_ptr;
  mojom::UDPSocketReceiverPtr receiver_ptr;
  test::UDPSocketReceiverImpl receiver;
  mojo::Binding<mojom::UDPSocketReceiver> receiver_binding(&receiver);
  receiver_binding.Bind(mojo::MakeRequest(&receiver_ptr));
  UDPSocket impl(mojo::MakeRequest(&socket_ptr), std::move(receiver_ptr));

  test::UDPSocketTestHelper helper(&socket_ptr);

  mojom::UDPSocketOptionsPtr options = network::mojom::UDPSocketOptions::New();
#if defined(OS_FUCHSIA)
  // Fuchsia currently doesn't support automatic interface selection for
  // multicast, so interface index needs to be set explicitly.
  // See https://fuchsia.atlassian.net/browse/NET-195 .
  options->multicast_interface = 1;
#endif  // defined(OS_FUCHSIA)

  net::IPAddress bind_ip_address;
  EXPECT_TRUE(bind_ip_address.AssignFromIPLiteral("0.0.0.0"));
  net::IPEndPoint socket_address(bind_ip_address, 0);
  ASSERT_EQ(net::OK, helper.OpenWithOptionsSync(socket_address.GetFamily(),
                                                std::move(options)));
  ASSERT_EQ(net::OK, helper.BindSync(socket_address, &socket_address));
  int port = socket_address.port();
  EXPECT_NE(0, port);
  EXPECT_EQ(net::OK, helper.JoinGroupSync(group_ip));
  // Joining group multiple times.
  EXPECT_NE(net::OK, helper.JoinGroupSync(group_ip));

  // Receive messages from itself.
  std::vector<uint8_t> test_msg(CreateTestMessage(0, kDatagramSize));
  net::IPEndPoint group_alias(group_ip, port);
  EXPECT_EQ(net::OK, helper.SendToSync(group_alias, test_msg));
  socket_ptr->ReceiveMore(1);
  receiver.WaitForReceivedResults(1);
  ASSERT_EQ(1u, receiver.results().size());
  auto result = receiver.results()[0];
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(port, result.src_addr.value().port());
  EXPECT_EQ(test_msg, result.data.value());

  // Create a second socket to send a packet to multicast group.
  mojom::UDPSocketPtr second_socket_ptr;
  mojom::UDPSocketReceiverPtr second_receiver_ptr;
  UDPSocket second_socket_impl(mojo::MakeRequest(&second_socket_ptr),
                               std::move(second_receiver_ptr));
  test::UDPSocketTestHelper second_socket_helper(&second_socket_ptr);
  net::IPEndPoint second_socket_address(bind_ip_address, 0);
  ASSERT_EQ(net::OK, second_socket_helper.OpenWithOptionsSync(
                         second_socket_address.GetFamily(), nullptr));
  ASSERT_EQ(net::OK, second_socket_helper.BindSync(second_socket_address,
                                                   &second_socket_address));
  int second_port = second_socket_address.port();
  ASSERT_EQ(net::OK, second_socket_helper.SendToSync(group_alias, test_msg));

  // First socket should receive packet sent by the second socket.
  socket_ptr->ReceiveMore(1);
  receiver.WaitForReceivedResults(2);
  ASSERT_EQ(2u, receiver.results().size());
  result = receiver.results()[1];
  EXPECT_EQ(net::OK, result.net_error);
  EXPECT_EQ(second_port, result.src_addr.value().port());
  EXPECT_EQ(test_msg, result.data.value());

  // First socket leaves group multiple times.
  EXPECT_EQ(net::OK, helper.LeaveGroupSync(group_ip));
  EXPECT_NE(net::OK, helper.LeaveGroupSync(group_ip));

  // No longer can receive messages from itself or from second socket.
  EXPECT_EQ(net::OK, helper.SendToSync(group_alias, test_msg));
  ASSERT_EQ(net::OK, second_socket_helper.SendToSync(group_alias, test_msg));
  socket_ptr->ReceiveMore(1);
  socket_ptr.FlushForTesting();
  ASSERT_EQ(2u, receiver.results().size());
}

TEST_F(UDPSocketTest, ErrorHappensDuringSocketOptionsConfiguration) {
  mojom::UDPSocketPtr socket_ptr;
  mojom::UDPSocketReceiverPtr receiver_ptr;
  UDPSocket impl(mojo::MakeRequest(&socket_ptr), std::move(receiver_ptr));

  test::UDPSocketTestHelper helper(&socket_ptr);

  // Invalid options.
  mojom::UDPSocketOptionsPtr options = network::mojom::UDPSocketOptions::New();
  options->multicast_time_to_live = 256;

  ASSERT_EQ(
      net::ERR_INVALID_ARGUMENT,
      helper.OpenWithOptionsSync(net::ADDRESS_FAMILY_IPV4, std::move(options)));

  // It's legal to retry Open() with valid options.
  mojom::UDPSocketOptionsPtr valid_options =
      network::mojom::UDPSocketOptions::New();
  valid_options->multicast_time_to_live = 255;
  ASSERT_EQ(net::OK, helper.OpenWithOptionsSync(net::ADDRESS_FAMILY_IPV4,
                                                std::move(options)));
}

}  // namespace network
