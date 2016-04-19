// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/remote_security_key_message_reader_impl.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/host/security_key/security_key_message.h"
#include "remoting/host/setup/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const remoting::RemoteSecurityKeyMessageType kTestMessageType =
    remoting::RemoteSecurityKeyMessageType::CONNECT;
const uint32_t kMaxSecurityKeyMessageByteCount = 256 * 1024;
}  // namespace

namespace remoting {

class RemoteSecurityKeyMessageReaderImplTest : public testing::Test {
 public:
  RemoteSecurityKeyMessageReaderImplTest();
  ~RemoteSecurityKeyMessageReaderImplTest() override;

  // SecurityKeyMessageCallback passed to the Reader. Stores |message| so it can
  // be verified by tests.
  void OnMessage(std::unique_ptr<SecurityKeyMessage> message);

  // Used as a callback to signal completion.
  void OperationComplete();

 protected:
  // testing::Test interface.
  void SetUp() override;

  // Runs the MessageLoop until the reader has completed and called back.
  void RunLoop();

  // Closes |write_file_| and runs the MessageLoop until the reader has
  // completed and called back.
  void CloseWriteFileAndRunLoop();

  // Writes a message (header+code+body) to the write-end of the pipe.
  void WriteMessage(RemoteSecurityKeyMessageType message_type,
                    const std::string& message_payload);

  // Writes some data to the write-end of the pipe.
  void WriteData(const char* data, int length);

  std::unique_ptr<RemoteSecurityKeyMessageReader> reader_;
  base::File read_file_;
  base::File write_file_;

  std::vector<std::unique_ptr<SecurityKeyMessage>> messages_received_;

 private:
  base::MessageLoopForIO message_loop_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSecurityKeyMessageReaderImplTest);
};

RemoteSecurityKeyMessageReaderImplTest::RemoteSecurityKeyMessageReaderImplTest()
    : run_loop_(new base::RunLoop()) {}

RemoteSecurityKeyMessageReaderImplTest::
    ~RemoteSecurityKeyMessageReaderImplTest() {}

void RemoteSecurityKeyMessageReaderImplTest::SetUp() {
  ASSERT_TRUE(MakePipe(&read_file_, &write_file_));
  reader_.reset(new RemoteSecurityKeyMessageReaderImpl(std::move(read_file_)));

  // base::Unretained is safe since no further tasks can run after
  // RunLoop::Run() returns.
  reader_->Start(
      base::Bind(&RemoteSecurityKeyMessageReaderImplTest::OnMessage,
                 base::Unretained(this)),
      base::Bind(&RemoteSecurityKeyMessageReaderImplTest::OperationComplete,
                 base::Unretained(this)));
}

void RemoteSecurityKeyMessageReaderImplTest::RunLoop() {
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void RemoteSecurityKeyMessageReaderImplTest::CloseWriteFileAndRunLoop() {
  write_file_.Close();
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void RemoteSecurityKeyMessageReaderImplTest::OnMessage(
    std::unique_ptr<SecurityKeyMessage> message) {
  messages_received_.push_back(std::move(message));
  OperationComplete();
}

void RemoteSecurityKeyMessageReaderImplTest::OperationComplete() {
  run_loop_->Quit();
}

void RemoteSecurityKeyMessageReaderImplTest::WriteMessage(
    RemoteSecurityKeyMessageType message_type,
    const std::string& message_payload) {
  uint32_t length =
      SecurityKeyMessage::kMessageTypeSizeBytes + message_payload.size();
  WriteData(reinterpret_cast<char*>(&length),
            SecurityKeyMessage::kHeaderSizeBytes);
  WriteData(reinterpret_cast<char*>(&message_type),
            SecurityKeyMessage::kMessageTypeSizeBytes);
  if (!message_payload.empty()) {
    WriteData(message_payload.data(), message_payload.size());
  }
}

void RemoteSecurityKeyMessageReaderImplTest::WriteData(const char* data,
                                                       int length) {
  int written = write_file_.WriteAtCurrentPos(data, length);
  ASSERT_EQ(length, written);
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, SingleMessageWithNoPayload) {
  WriteMessage(kTestMessageType, std::string());
  RunLoop();
  ASSERT_EQ(1u, messages_received_.size());
  ASSERT_EQ(kTestMessageType, messages_received_[0]->type());
  ASSERT_EQ("", messages_received_[0]->payload());

  CloseWriteFileAndRunLoop();
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, SingleMessageWithPayload) {
  std::string payload("I AM A VALID MESSAGE PAYLOAD!!!!!!!!!!!!!!!!!!!!!!");
  WriteMessage(kTestMessageType, payload);
  RunLoop();
  ASSERT_EQ(1u, messages_received_.size());
  ASSERT_EQ(kTestMessageType, messages_received_[0]->type());
  ASSERT_EQ(payload, messages_received_[0]->payload());

  CloseWriteFileAndRunLoop();
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, SingleMessageViaSingleWrite) {
  // All other tests write in 2-3 chunks, this writes the message in one shot.
  std::string payload("LLLLTI am the best payload in the history of testing.");
  // Overwite the 'L' values with the actual length.
  uint8_t length = payload.size() - SecurityKeyMessage::kHeaderSizeBytes;
  payload[0] = static_cast<char>(length);
  payload[1] = 0;
  payload[2] = 0;
  payload[3] = 0;
  // Overwite the 'T' value with the actual type.
  payload[4] = static_cast<char>(kTestMessageType);
  WriteData(payload.data(), payload.size());
  RunLoop();
  ASSERT_EQ(1u, messages_received_.size());
  ASSERT_EQ(kTestMessageType, messages_received_[0]->type());
  ASSERT_EQ(payload.substr(5), messages_received_[0]->payload());

  CloseWriteFileAndRunLoop();
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, SingleMessageViaMultipleWrites) {
  // All other tests write in 2-3 chunks, this writes the message byte by byte.
  std::string payload("LLLLTI am the worst payload in the history of testing.");
  // Overwite the 'L' values with the actual length.
  uint8_t length = payload.size() - SecurityKeyMessage::kHeaderSizeBytes;
  payload[0] = static_cast<char>(length);
  payload[1] = 0;
  payload[2] = 0;
  payload[3] = 0;
  // Overwite the 'T' value with the actual type.
  payload[4] = static_cast<char>(kTestMessageType);

  for (uint32_t i = 0; i < payload.size(); i++) {
    WriteData(&payload[i], 1);
  }
  RunLoop();
  ASSERT_EQ(1u, messages_received_.size());
  ASSERT_EQ(kTestMessageType, messages_received_[0]->type());
  ASSERT_EQ(payload.substr(5), messages_received_[0]->payload());

  CloseWriteFileAndRunLoop();
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, SingleMessageWithLargePayload) {
  std::string payload(kMaxSecurityKeyMessageByteCount -
                          SecurityKeyMessage::kMessageTypeSizeBytes,
                      'Y');
  WriteMessage(kTestMessageType, payload);
  RunLoop();
  ASSERT_EQ(1u, messages_received_.size());
  ASSERT_EQ(kTestMessageType, messages_received_[0]->type());
  ASSERT_EQ(payload, messages_received_[0]->payload());

  CloseWriteFileAndRunLoop();
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, EmptyFile) {
  CloseWriteFileAndRunLoop();
  ASSERT_EQ(0u, messages_received_.size());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, InvalidMessageLength) {
  uint32_t length = kMaxSecurityKeyMessageByteCount + 1;
  ASSERT_FALSE(SecurityKeyMessage::IsValidMessageSize(length));
  WriteData(reinterpret_cast<char*>(&length), sizeof(length));
  CloseWriteFileAndRunLoop();
  ASSERT_EQ(0u, messages_received_.size());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, ShortHeader) {
  // Write only 3 bytes - the message length header is supposed to be 4 bytes.
  WriteData("xxx", SecurityKeyMessage::kHeaderSizeBytes - 1);
  CloseWriteFileAndRunLoop();
  ASSERT_EQ(0u, messages_received_.size());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, ZeroLengthMessage) {
  uint32_t length = 0;
  WriteData(reinterpret_cast<char*>(&length), sizeof(length));
  CloseWriteFileAndRunLoop();
  ASSERT_EQ(0u, messages_received_.size());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, MissingControlCode) {
  uint32_t length = 1;
  WriteData(reinterpret_cast<char*>(&length), sizeof(length));
  CloseWriteFileAndRunLoop();
  ASSERT_EQ(0u, messages_received_.size());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, MissingPayload) {
  uint32_t length = 2;
  WriteData(reinterpret_cast<char*>(&length), sizeof(length));

  char test_control_code = static_cast<char>(kTestMessageType);
  WriteData(&test_control_code, sizeof(test_control_code));
  CloseWriteFileAndRunLoop();
  ASSERT_EQ(0u, messages_received_.size());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, MultipleMessages) {
  std::vector<std::string> payloads({"", "S",  // Really short
                                     "", "Short", "", "Medium Length", "",
                                     "Longer than medium, but not super long",
                                     "", std::string(2048, 'Y'), ""});
  for (size_t i = 0; i < payloads.size(); i++) {
    WriteMessage(kTestMessageType, payloads[i]);
    RunLoop();
    ASSERT_EQ(i + 1, messages_received_.size());
  }
  CloseWriteFileAndRunLoop();

  for (size_t i = 0; i < payloads.size(); i++) {
    ASSERT_EQ(kTestMessageType, messages_received_[i]->type());
    ASSERT_EQ(payloads[i], messages_received_[i]->payload());
  }
}

}  // namespace remoting
