// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/socket_factory.h"
#include "services/network/tcp_connected_socket.h"
#include "services/network/tcp_server_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class TestSocketObserver : public mojom::TCPConnectedSocketObserver {
 public:
  TestSocketObserver() : binding_(this) {}
  ~TestSocketObserver() override {
    EXPECT_EQ(net::OK, read_error_);
    EXPECT_EQ(net::OK, write_error_);
  }

  // Returns a mojo pointer. This can only be called once.
  mojom::TCPConnectedSocketObserverPtr GetObserverPtr() {
    DCHECK(!binding_);
    mojom::TCPConnectedSocketObserverPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // Waits for Read and Write error. Returns the error observed.
  int WaitForReadError() {
    read_loop_.Run();
    int error = read_error_;
    read_error_ = net::OK;
    return error;
  }

  int WaitForWriteError() {
    write_loop_.Run();
    int error = write_error_;
    write_error_ = net::OK;
    return error;
  }

 private:
  // mojom::TCPConnectedSocketObserver implementation.
  void OnReadError(int net_error) override {
    read_error_ = net_error;
    read_loop_.Quit();
  }

  void OnWriteError(int net_error) override {
    write_error_ = net_error;
    write_loop_.Quit();
  }

  int read_error_ = net::OK;
  int write_error_ = net::OK;
  base::RunLoop read_loop_;
  base::RunLoop write_loop_;
  mojo::Binding<mojom::TCPConnectedSocketObserver> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestSocketObserver);
};

}  // namespace

class SocketDataPumpTest : public testing::Test,
                           public ::testing::WithParamInterface<net::IoMode> {
 public:
  SocketDataPumpTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {}
  ~SocketDataPumpTest() override {}

  // Initializes the test case with a socket data provider, which will be used
  // to populate the read/write data of the mock socket.
  void Init(net::StaticSocketDataProvider* data_provider) {
    mock_client_socket_factory_.AddSocketDataProvider(data_provider);
    mojo::DataPipe send_pipe;
    mojo::DataPipe receive_pipe;
    receive_handle_ = std::move(receive_pipe.consumer_handle);
    send_handle_ = std::move(send_pipe.producer_handle);
    socket_ = mock_client_socket_factory_.CreateTransportClientSocket(
        net::AddressList(), nullptr /*socket_performance_watcher*/,
        nullptr /*netlog*/, net::NetLogSource());
    net::TestCompletionCallback callback;
    int result = socket_->Connect(callback.callback());
    if (result == net::ERR_IO_PENDING)
      result = callback.WaitForResult();
    EXPECT_EQ(net::OK, result);
    data_pump_ = std::make_unique<SocketDataPump>(
        test_observer_.GetObserverPtr(), socket_.get(),
        std::move(receive_pipe.producer_handle),
        std::move(send_pipe.consumer_handle), TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  // Reads |num_bytes| from |handle| or reads until an error occurs. Returns the
  // bytes read as a string.
  std::string Read(mojo::ScopedDataPipeConsumerHandle* handle,
                   size_t num_bytes) {
    std::string received_contents;
    while (received_contents.size() < num_bytes) {
      base::RunLoop().RunUntilIdle();
      std::vector<char> buffer(num_bytes);
      uint32_t read_size = static_cast<uint32_t>(num_bytes);
      MojoResult result = handle->get().ReadData(buffer.data(), &read_size,
                                                 MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT)
        continue;
      if (result != MOJO_RESULT_OK)
        return received_contents;
      received_contents.append(buffer.data(), read_size);
    }
    return received_contents;
  }

  TestSocketObserver* observer() { return &test_observer_; }

  mojo::ScopedDataPipeConsumerHandle receive_handle_;
  mojo::ScopedDataPipeProducerHandle send_handle_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  net::MockClientSocketFactory mock_client_socket_factory_;
  TestSocketObserver test_observer_;
  std::unique_ptr<net::StreamSocket> socket_;
  std::unique_ptr<SocketDataPump> data_pump_;

  DISALLOW_COPY_AND_ASSIGN(SocketDataPumpTest);
};

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        SocketDataPumpTest,
                        testing::Values(net::SYNCHRONOUS, net::ASYNC));

TEST_P(SocketDataPumpTest, ReadAndWriteMultiple) {
  const char kTestMsg[] = "abcdefghij";
  const size_t kMsgSize = strlen(kTestMsg);
  const int kNumIterations = 3;
  std::vector<net::MockRead> reads;
  std::vector<net::MockWrite> writes;
  int sequence_number = 0;
  net::IoMode mode = GetParam();
  for (int j = 0; j < kNumIterations; ++j) {
    for (size_t i = 0; i < kMsgSize; ++i) {
      reads.push_back(net::MockRead(mode, &kTestMsg[i], 1, sequence_number++));
    }
    if (j == kNumIterations - 1) {
      reads.push_back(net::MockRead(mode, net::OK, sequence_number++));
    }
    for (size_t i = 0; i < kMsgSize; ++i) {
      writes.push_back(
          net::MockWrite(mode, &kTestMsg[i], 1, sequence_number++));
    }
  }
  net::StaticSocketDataProvider data_provider(reads, writes);
  Init(&data_provider);
  // Loop kNumIterations times to test that writes can follow reads, and reads
  // can follow writes.
  for (int j = 0; j < kNumIterations; ++j) {
    // Reading kMsgSize should coalesce the 1-byte mock reads.
    EXPECT_EQ(kTestMsg, Read(&receive_handle_, kMsgSize));
    // Write multiple times.
    for (size_t i = 0; i < kMsgSize; ++i) {
      uint32_t num_bytes = 1;
      EXPECT_EQ(MOJO_RESULT_OK,
                send_handle_->WriteData(&kTestMsg[i], &num_bytes,
                                        MOJO_WRITE_DATA_FLAG_NONE));
      // Flush the 1 byte write.
      base::RunLoop().RunUntilIdle();
    }
  }
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(SocketDataPumpTest, PartialStreamSocketWrite) {
  const char kTestMsg[] = "abcdefghij";
  const size_t kMsgSize = strlen(kTestMsg);
  const int kNumIterations = 3;
  std::vector<net::MockRead> reads;
  std::vector<net::MockWrite> writes;
  int sequence_number = 0;
  net::IoMode mode = GetParam();
  for (int j = 0; j < kNumIterations; ++j) {
    for (size_t i = 0; i < kMsgSize; ++i) {
      reads.push_back(net::MockRead(mode, &kTestMsg[i], 1, sequence_number++));
    }
    if (j == kNumIterations - 1) {
      reads.push_back(net::MockRead(mode, net::OK, sequence_number++));
    }
    for (size_t i = 0; i < kMsgSize; ++i) {
      writes.push_back(
          net::MockWrite(mode, &kTestMsg[i], 1, sequence_number++));
    }
  }
  net::StaticSocketDataProvider data_provider(reads, writes);
  Init(&data_provider);
  // Loop kNumIterations times to test that writes can follow reads, and reads
  // can follow writes.
  for (int j = 0; j < kNumIterations; ++j) {
    // Reading kMsgSize should coalesce the 1-byte mock reads.
    EXPECT_EQ(kTestMsg, Read(&receive_handle_, kMsgSize));
    // Write twice, each with kMsgSize/2 bytes which is bigger than the 1-byte
    // MockWrite(). This is to exercise that StreamSocket::Write() can do
    // partial write.
    uint32_t first_write_size = kMsgSize / 2;
    EXPECT_EQ(MOJO_RESULT_OK,
              send_handle_->WriteData(&kTestMsg[0], &first_write_size,
                                      MOJO_WRITE_DATA_FLAG_NONE));
    EXPECT_EQ(kMsgSize / 2, first_write_size);
    // Flush the kMsgSize/2 byte write.
    base::RunLoop().RunUntilIdle();
    uint32_t second_write_size = kMsgSize - first_write_size;
    EXPECT_EQ(
        MOJO_RESULT_OK,
        send_handle_->WriteData(&kTestMsg[first_write_size], &second_write_size,
                                MOJO_WRITE_DATA_FLAG_NONE));
    EXPECT_EQ(kMsgSize - first_write_size, second_write_size);
    // Flush the kMsgSize/2 byte write.
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(SocketDataPumpTest, ReadError) {
  net::IoMode mode = GetParam();
  net::MockRead reads[] = {net::MockRead(mode, net::ERR_FAILED)};
  const char kTestMsg[] = "hello!";
  net::MockWrite writes[] = {
      net::MockWrite(mode, kTestMsg, strlen(kTestMsg), 0)};
  net::StaticSocketDataProvider data_provider(reads, writes);
  Init(&data_provider);
  EXPECT_EQ("", Read(&receive_handle_, 1));
  EXPECT_EQ(net::ERR_FAILED, observer()->WaitForReadError());
  // Writes can proceed even though there is a read error.
  uint32_t num_bytes = strlen(kTestMsg);
  EXPECT_EQ(MOJO_RESULT_OK, send_handle_->WriteData(&kTestMsg, &num_bytes,
                                                    MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(strlen(kTestMsg), num_bytes);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

TEST_P(SocketDataPumpTest, WriteError) {
  net::IoMode mode = GetParam();
  const char kTestMsg[] = "hello!";
  net::MockRead reads[] = {net::MockRead(mode, kTestMsg, strlen(kTestMsg), 0),
                           net::MockRead(mode, net::OK)};
  net::MockWrite writes[] = {net::MockWrite(mode, net::ERR_FAILED)};
  net::StaticSocketDataProvider data_provider(reads, writes);
  Init(&data_provider);
  uint32_t num_bytes = strlen(kTestMsg);
  EXPECT_EQ(MOJO_RESULT_OK, send_handle_->WriteData(&kTestMsg, &num_bytes,
                                                    MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(strlen(kTestMsg), num_bytes);
  EXPECT_EQ(net::ERR_FAILED, observer()->WaitForWriteError());
  // Reads can proceed even though there is a read error.
  EXPECT_EQ(kTestMsg, Read(&receive_handle_, strlen(kTestMsg)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(data_provider.AllReadDataConsumed());
  EXPECT_TRUE(data_provider.AllWriteDataConsumed());
}

}  // namespace network
