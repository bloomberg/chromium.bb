// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_transfer_message_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/host/file_transfer/fake_file_operations.h"
#include "remoting/protocol/fake_message_pipe.h"
#include "remoting/protocol/fake_message_pipe_wrapper.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestDatachannelName[] = "filetransfer-test";
constexpr char kTestFilename[] = "test-file.txt";

std::unique_ptr<remoting::CompoundBuffer> StringToBuffer(
    const std::string& data) {
  std::unique_ptr<remoting::CompoundBuffer> buffer =
      std::make_unique<remoting::CompoundBuffer>();
  buffer->Append(base::MakeRefCounted<net::StringIOBuffer>(data.data()),
                 data.size());
  return buffer;
}

std::unique_ptr<remoting::CompoundBuffer> MessageToBuffer(
    const remoting::protocol::FileTransfer& message) {
  return StringToBuffer(message.SerializeAsString());
}

std::unique_ptr<remoting::CompoundBuffer> DataToBuffer(
    const std::string& data) {
  remoting::protocol::FileTransfer message;
  message.mutable_data()->set_data(data);
  return MessageToBuffer(message);
}

// base::queue doesn't provide operator==.
template <typename T>
bool QueuesEqual(const base::queue<T>& a, const base::queue<T>& b) {
  if (a.size() != b.size())
    return false;

  auto a_copy = a;
  auto b_copy = b;
  while (!a_copy.empty()) {
    if (a_copy.front() != b_copy.front())
      return false;
    a_copy.pop();
    b_copy.pop();
  }
  return true;
}

}  // namespace

namespace remoting {

class FileTransferMessageHandlerTest : public testing::Test {
 public:
  FileTransferMessageHandlerTest();
  ~FileTransferMessageHandlerTest() override;

  // testing::Test implementation.
  void SetUp() override;
  void TearDown() override;

 protected:
  const std::string kTestDataOne = "this is the first test string";
  const std::string kTestDataTwo = "this is the second test string";
  const std::string kTestDataThree = "this is the third test string";

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<protocol::FakeMessagePipe> fake_pipe_;
  protocol::FileTransfer fake_metadata_;
  protocol::FileTransfer fake_end_;
};

FileTransferMessageHandlerTest::FileTransferMessageHandlerTest()
    : scoped_task_environment_(
          base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT,
          base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED) {}
FileTransferMessageHandlerTest::~FileTransferMessageHandlerTest() = default;

void FileTransferMessageHandlerTest::SetUp() {
  fake_pipe_ =
      base::WrapUnique(new protocol::FakeMessagePipe(false /* asynchronous */));

  fake_metadata_.Clear();
  fake_metadata_.mutable_metadata()->set_filename(kTestFilename);
  fake_metadata_.mutable_metadata()->set_size(
      kTestDataOne.size() + kTestDataTwo.size() + kTestDataThree.size());
  fake_end_.Clear();
  fake_end_.mutable_end();
}

void FileTransferMessageHandlerTest::TearDown() {}

// Verify that the message handler creates, writes to, and closes a
// FileProxyWrapper without errors when given valid input.
TEST_F(FileTransferMessageHandlerTest, WritesThreeChunks) {
  FakeFileOperations::TestIo test_io;
  auto file_operations = std::make_unique<FakeFileOperations>(&test_io);

  // This will delete itself when fake_pipe_->ClosePipe() is called.
  new FileTransferMessageHandler(kTestDatachannelName, fake_pipe_->Wrap(),
                                 std::move(file_operations));

  fake_pipe_->OpenPipe();
  fake_pipe_->Receive(MessageToBuffer(fake_metadata_));
  fake_pipe_->Receive(DataToBuffer(kTestDataOne));
  fake_pipe_->Receive(DataToBuffer(kTestDataTwo));
  fake_pipe_->Receive(DataToBuffer(kTestDataThree));
  fake_pipe_->Receive(MessageToBuffer(fake_end_));
  scoped_task_environment_.RunUntilIdle();

  fake_pipe_->ClosePipe();

  ASSERT_EQ(1ul, test_io.files_written.size());
  ASSERT_EQ(false, test_io.files_written[0].failed);
  std::vector<std::string> expected_chunks = {kTestDataOne, kTestDataTwo,
                                              kTestDataThree};
  ASSERT_EQ(expected_chunks, test_io.files_written[0].chunks);

  const base::queue<std::string>& actual_sent_messages =
      fake_pipe_->sent_messages();
  protocol::FileTransfer expected_response;
  expected_response.mutable_success();
  base::queue<std::string> expected_sent_messages;
  expected_sent_messages.push(expected_response.SerializeAsString());
  ASSERT_TRUE(QueuesEqual(expected_sent_messages, actual_sent_messages));
}

// Verifies that the message handler sends an error protobuf when
// FileProxyWrapper returns an error.
TEST_F(FileTransferMessageHandlerTest, HandlesWriteError) {
  FakeFileOperations::TestIo test_io;
  auto file_operations = std::make_unique<FakeFileOperations>(&test_io);

  protocol::FileTransfer_Error fake_error = protocol::MakeFileTransferError(
      FROM_HERE, protocol::FileTransfer_Error_Type_IO_ERROR);

  // This will delete itself when fake_pipe_->ClosePipe() is called.
  new FileTransferMessageHandler(kTestDatachannelName, fake_pipe_->Wrap(),
                                 std::move(file_operations));

  fake_pipe_->OpenPipe();
  fake_pipe_->Receive(MessageToBuffer(fake_metadata_));
  fake_pipe_->Receive(DataToBuffer(kTestDataOne));
  fake_pipe_->Receive(DataToBuffer(kTestDataTwo));
  scoped_task_environment_.RunUntilIdle();
  test_io.io_error = fake_error;
  fake_pipe_->Receive(DataToBuffer(kTestDataTwo));
  fake_pipe_->Receive(MessageToBuffer(fake_end_));
  scoped_task_environment_.RunUntilIdle();

  fake_pipe_->ClosePipe();

  const base::queue<std::string>& actual_sent_messages =
      fake_pipe_->sent_messages();
  protocol::FileTransfer expected_response;
  *expected_response.mutable_error() = fake_error;
  base::queue<std::string> expected_sent_messages;
  expected_sent_messages.push(expected_response.SerializeAsString());
  ASSERT_TRUE(QueuesEqual(expected_sent_messages, actual_sent_messages));
}

// Verifies that the message handler cancels the write if an error is received
// from the sender.
TEST_F(FileTransferMessageHandlerTest, HandlesErrorMessage) {
  FakeFileOperations::TestIo test_io;
  auto file_operations = std::make_unique<FakeFileOperations>(&test_io);

  // This will delete itself when fake_pipe_->ClosePipe() is called.
  new FileTransferMessageHandler(kTestDatachannelName, fake_pipe_->Wrap(),
                                 std::move(file_operations));

  protocol::FileTransfer fake_error_message;
  *fake_error_message.mutable_error() = protocol::MakeFileTransferError(
      FROM_HERE, protocol::FileTransfer_Error_Type_IO_ERROR);

  fake_pipe_->OpenPipe();
  fake_pipe_->Receive(MessageToBuffer(fake_metadata_));
  fake_pipe_->Receive(DataToBuffer(kTestDataOne));
  fake_pipe_->Receive(DataToBuffer(kTestDataTwo));
  scoped_task_environment_.RunUntilIdle();
  fake_pipe_->Receive(MessageToBuffer(fake_error_message));
  scoped_task_environment_.RunUntilIdle();

  fake_pipe_->ClosePipe();

  ASSERT_EQ(1ul, test_io.files_written.size());
  ASSERT_EQ(true, test_io.files_written[0].failed);
  std::vector<std::string> expected_chunks = {kTestDataOne, kTestDataTwo};
  ASSERT_EQ(expected_chunks, test_io.files_written[0].chunks);

  const base::queue<std::string>& actual_sent_messages =
      fake_pipe_->sent_messages();
  // No messages
  base::queue<std::string> expected_sent_messages;
  ASSERT_TRUE(QueuesEqual(expected_sent_messages, actual_sent_messages));
}

// Verifies that the message handler cancels the write if the connection is
// closed prematurely.
TEST_F(FileTransferMessageHandlerTest, HandlesPrematureClose) {
  FakeFileOperations::TestIo test_io;
  auto file_operations = std::make_unique<FakeFileOperations>(&test_io);

  // This will delete itself when fake_pipe_->ClosePipe() is called.
  new FileTransferMessageHandler(kTestDatachannelName, fake_pipe_->Wrap(),
                                 std::move(file_operations));

  fake_pipe_->OpenPipe();
  fake_pipe_->Receive(MessageToBuffer(fake_metadata_));
  fake_pipe_->Receive(DataToBuffer(kTestDataOne));
  fake_pipe_->Receive(DataToBuffer(kTestDataTwo));
  scoped_task_environment_.RunUntilIdle();

  fake_pipe_->ClosePipe();

  ASSERT_EQ(1ul, test_io.files_written.size());
  ASSERT_EQ(true, test_io.files_written[0].failed);
  std::vector<std::string> expected_chunks = {kTestDataOne, kTestDataTwo};
  ASSERT_EQ(expected_chunks, test_io.files_written[0].chunks);
}

// Verifies that an error is sent if data is sent before/without metadata.
TEST_F(FileTransferMessageHandlerTest, ErrorsOnMissingMetadata) {
  FakeFileOperations::TestIo test_io;
  auto file_operations = std::make_unique<FakeFileOperations>(&test_io);

  // This will delete itself when fake_pipe_->ClosePipe() is called.
  new FileTransferMessageHandler(kTestDatachannelName, fake_pipe_->Wrap(),
                                 std::move(file_operations));

  fake_pipe_->OpenPipe();
  fake_pipe_->Receive(DataToBuffer(kTestDataOne));
  fake_pipe_->Receive(DataToBuffer(kTestDataTwo));
  fake_pipe_->Receive(DataToBuffer(kTestDataThree));
  fake_pipe_->Receive(MessageToBuffer(fake_end_));
  scoped_task_environment_.RunUntilIdle();

  fake_pipe_->ClosePipe();

  ASSERT_EQ(0ul, test_io.files_written.size());

  const base::queue<std::string>& sent_messages = fake_pipe_->sent_messages();
  ASSERT_EQ(1ul, sent_messages.size());
  protocol::FileTransfer response;
  response.ParseFromString(sent_messages.front());
  ASSERT_EQ(protocol::FileTransfer::kError, response.message_case());
  ASSERT_EQ(protocol::FileTransfer_Error_Type_PROTOCOL_ERROR,
            response.error().type());
}

// Verifies that an error is sent if another metadata message is sent.
TEST_F(FileTransferMessageHandlerTest, ErrorsOnNewMetadata) {
  FakeFileOperations::TestIo test_io;
  auto file_operations = std::make_unique<FakeFileOperations>(&test_io);

  // This will delete itself when fake_pipe_->ClosePipe() is called.
  new FileTransferMessageHandler(kTestDatachannelName, fake_pipe_->Wrap(),
                                 std::move(file_operations));

  fake_pipe_->OpenPipe();
  fake_pipe_->Receive(MessageToBuffer(fake_metadata_));
  fake_pipe_->Receive(DataToBuffer(kTestDataOne));
  fake_pipe_->Receive(DataToBuffer(kTestDataTwo));
  fake_pipe_->Receive(DataToBuffer(kTestDataThree));
  fake_pipe_->Receive(MessageToBuffer(fake_end_));
  scoped_task_environment_.RunUntilIdle();
  fake_pipe_->Receive(MessageToBuffer(fake_metadata_));

  fake_pipe_->ClosePipe();

  const base::queue<std::string>& sent_messages = fake_pipe_->sent_messages();
  // First is the sucess message, second should be a protocol error.
  ASSERT_EQ(2ul, sent_messages.size());
  protocol::FileTransfer response;
  response.ParseFromString(sent_messages.back());
  ASSERT_EQ(protocol::FileTransfer::kError, response.message_case());
  ASSERT_EQ(protocol::FileTransfer_Error_Type_PROTOCOL_ERROR,
            response.error().type());
}

// Verifies that an error is sent if more data is sent after Close.
TEST_F(FileTransferMessageHandlerTest, ErrorsOnDataAfterClose) {
  FakeFileOperations::TestIo test_io;
  auto file_operations = std::make_unique<FakeFileOperations>(&test_io);

  // This will delete itself when fake_pipe_->ClosePipe() is called.
  new FileTransferMessageHandler(kTestDatachannelName, fake_pipe_->Wrap(),
                                 std::move(file_operations));

  fake_pipe_->OpenPipe();
  fake_pipe_->Receive(MessageToBuffer(fake_metadata_));
  fake_pipe_->Receive(DataToBuffer(kTestDataOne));
  fake_pipe_->Receive(DataToBuffer(kTestDataTwo));
  fake_pipe_->Receive(DataToBuffer(kTestDataThree));
  fake_pipe_->Receive(MessageToBuffer(fake_end_));
  fake_pipe_->Receive(DataToBuffer(kTestDataOne));
  scoped_task_environment_.RunUntilIdle();

  fake_pipe_->ClosePipe();

  ASSERT_EQ(1ul, test_io.files_written.size());
  ASSERT_EQ(true, test_io.files_written[0].failed);

  const base::queue<std::string>& sent_messages = fake_pipe_->sent_messages();
  // Because the error is triggered before RunUntilIdle is called, there should
  // be no complete message this time.
  ASSERT_EQ(1ul, sent_messages.size());
  protocol::FileTransfer response;
  response.ParseFromString(sent_messages.front());
  ASSERT_EQ(protocol::FileTransfer::kError, response.message_case());
  ASSERT_EQ(protocol::FileTransfer_Error_Type_PROTOCOL_ERROR,
            response.error().type());
}

}  // namespace remoting
