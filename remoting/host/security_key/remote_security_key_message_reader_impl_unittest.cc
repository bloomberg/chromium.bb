// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/remote_security_key_message_reader.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/host/security_key/remote_security_key_message_reader_impl.h"
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
  void OnMessage(scoped_ptr<SecurityKeyMessage> message);

 protected:
  // testing::Test interface.
  void SetUp() override;

  // Runs the MessageLoop until the reader has completed and called back.
  void Run();

  // Writes a message (header+code+body) to the write-end of the pipe.
  void WriteMessage(RemoteSecurityKeyMessageType message_type,
                    const std::string& message_payload);

  // Writes some data to the write-end of the pipe.
  void WriteData(const char* data, int length);

  scoped_ptr<RemoteSecurityKeyMessageReader> reader_;
  base::File read_file_;
  base::File write_file_;

  std::vector<scoped_ptr<SecurityKeyMessage>> messages_received_;

 private:
  base::MessageLoopForIO message_loop_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSecurityKeyMessageReaderImplTest);
};

RemoteSecurityKeyMessageReaderImplTest::
    RemoteSecurityKeyMessageReaderImplTest() {}

RemoteSecurityKeyMessageReaderImplTest::
    ~RemoteSecurityKeyMessageReaderImplTest() {}

void RemoteSecurityKeyMessageReaderImplTest::SetUp() {
  ASSERT_TRUE(MakePipe(&read_file_, &write_file_));
  reader_.reset(new RemoteSecurityKeyMessageReaderImpl(std::move(read_file_)));

  // base::Unretained is safe since no further tasks can run after
  // RunLoop::Run() returns.
  reader_->Start(base::Bind(&RemoteSecurityKeyMessageReaderImplTest::OnMessage,
                            base::Unretained(this)),
                 run_loop_.QuitClosure());
}

void RemoteSecurityKeyMessageReaderImplTest::Run() {
  // Close the write-end, so the reader doesn't block waiting for more data.
  write_file_.Close();
  run_loop_.Run();
}

void RemoteSecurityKeyMessageReaderImplTest::OnMessage(
    scoped_ptr<SecurityKeyMessage> message) {
  messages_received_.push_back(std::move(message));
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

TEST_F(RemoteSecurityKeyMessageReaderImplTest, EnsureReaderTornDownCleanly) {
  // This test is different from the others as the files used for reading and
  // writing are still open when the reader instance is destroyed.  This test is
  // meant to ensure that no asserts/exceptions/hangs occur during shutdown.
  WriteMessage(kTestMessageType, std::string());
  reader_.reset();
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, SingleMessageWithNoPayload) {
  WriteMessage(kTestMessageType, std::string());
  Run();
  ASSERT_EQ(1u, messages_received_.size());
  ASSERT_EQ(kTestMessageType, messages_received_[0]->type());
  ASSERT_EQ("", messages_received_[0]->payload());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, SingleMessageWithPayload) {
  std::string payload("I AM A VALID MESSAGE PAYLOAD!!!!!!!!!!!!!!!!!!!!!!");
  WriteMessage(kTestMessageType, payload);
  Run();
  ASSERT_EQ(1u, messages_received_.size());
  ASSERT_EQ(kTestMessageType, messages_received_[0]->type());
  ASSERT_EQ(payload, messages_received_[0]->payload());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, SingleMessageWithLargePayload) {
  std::string payload(kMaxSecurityKeyMessageByteCount -
                          SecurityKeyMessage::kMessageTypeSizeBytes,
                      'Y');
  WriteMessage(kTestMessageType, payload);
  Run();
  ASSERT_EQ(1u, messages_received_.size());
  ASSERT_EQ(kTestMessageType, messages_received_[0]->type());
  ASSERT_EQ(payload, messages_received_[0]->payload());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, EmptyFile) {
  Run();
  ASSERT_EQ(0u, messages_received_.size());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, InvalidMessageLength) {
  uint32_t length = kMaxSecurityKeyMessageByteCount + 1;
  ASSERT_FALSE(SecurityKeyMessage::IsValidMessageSize(length));
  WriteData(reinterpret_cast<char*>(&length), sizeof(length));
  Run();
  ASSERT_EQ(0u, messages_received_.size());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, ShortHeader) {
  // Write only 3 bytes - the message length header is supposed to be 4 bytes.
  WriteData("xxx", SecurityKeyMessage::kHeaderSizeBytes - 1);
  Run();
  ASSERT_EQ(0u, messages_received_.size());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, ZeroLengthMessage) {
  uint32_t length = 0;
  WriteData(reinterpret_cast<char*>(&length), sizeof(length));
  Run();
  ASSERT_EQ(0u, messages_received_.size());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, MissingControlCode) {
  uint32_t length = 1;
  WriteData(reinterpret_cast<char*>(&length), sizeof(length));
  Run();
  ASSERT_EQ(0u, messages_received_.size());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, MissingPayload) {
  uint32_t length = 2;
  WriteData(reinterpret_cast<char*>(&length), sizeof(length));

  char test_control_code = static_cast<char>(kTestMessageType);
  WriteData(&test_control_code, sizeof(test_control_code));
  Run();
  ASSERT_EQ(0u, messages_received_.size());
}

TEST_F(RemoteSecurityKeyMessageReaderImplTest, MultipleMessages) {
  std::vector<std::string> payloads({"", "S",  // Really short
                                     "", "Short", "", "Medium Length", "",
                                     "Longer than medium, but not super long",
                                     "", std::string(2048, 'Y'), ""});

  for (auto& payload : payloads) {
    WriteMessage(kTestMessageType, payload);
  }

  Run();
  ASSERT_EQ(payloads.size(), messages_received_.size());

  for (size_t i = 0; i < payloads.size(); i++) {
    ASSERT_EQ(kTestMessageType, messages_received_[i]->type());
    ASSERT_EQ(payloads[i], messages_received_[i]->payload());
  }
}

}  // namespace remoting
