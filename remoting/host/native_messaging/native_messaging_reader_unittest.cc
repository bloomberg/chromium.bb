// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/native_messaging/native_messaging_reader.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "remoting/host/setup/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class NativeMessagingReaderTest : public testing::Test {
 public:
  NativeMessagingReaderTest();
  virtual ~NativeMessagingReaderTest();

  virtual void SetUp() OVERRIDE;

  // Starts the reader and runs the MessageLoop to completion.
  void Run();

  // MessageCallback passed to the Reader. Stores |message| so it can be
  // verified by tests.
  void OnMessage(scoped_ptr<base::Value> message);

  // Writes a message (header+body) to the write-end of the pipe.
  void WriteMessage(std::string message);

  // Writes some data to the write-end of the pipe.
  void WriteData(const char* data, int length);

 protected:
  scoped_ptr<NativeMessagingReader> reader_;
  base::File read_file_;
  base::File write_file_;
  scoped_ptr<base::Value> message_;

 private:
  // MessageLoop declared here, since the NativeMessageReader ctor requires a
  // MessageLoop to have been created.
  base::MessageLoopForIO message_loop_;
  base::RunLoop run_loop_;
};

NativeMessagingReaderTest::NativeMessagingReaderTest() {
}

NativeMessagingReaderTest::~NativeMessagingReaderTest() {}

void NativeMessagingReaderTest::SetUp() {
  ASSERT_TRUE(MakePipe(&read_file_, &write_file_));
  reader_.reset(new NativeMessagingReader(read_file_.Pass()));
}

void NativeMessagingReaderTest::Run() {
  // Close the write-end, so the reader doesn't block waiting for more data.
  write_file_.Close();

  // base::Unretained is safe since no further tasks can run after
  // RunLoop::Run() returns.
  reader_->Start(
      base::Bind(&NativeMessagingReaderTest::OnMessage, base::Unretained(this)),
      run_loop_.QuitClosure());
  run_loop_.Run();
}

void NativeMessagingReaderTest::OnMessage(scoped_ptr<base::Value> message) {
  message_ = message.Pass();
}

void NativeMessagingReaderTest::WriteMessage(std::string message) {
  uint32 length = message.length();
  WriteData(reinterpret_cast<char*>(&length), 4);
  WriteData(message.data(), length);
}

void NativeMessagingReaderTest::WriteData(const char* data, int length) {
  int written = write_file_.WriteAtCurrentPos(data, length);
  ASSERT_EQ(length, written);
}

TEST_F(NativeMessagingReaderTest, GoodMessage) {
  WriteMessage("{\"foo\": 42}");
  Run();
  EXPECT_TRUE(message_);
  base::DictionaryValue* message_dict;
  EXPECT_TRUE(message_->GetAsDictionary(&message_dict));
  int result;
  EXPECT_TRUE(message_dict->GetInteger("foo", &result));
  EXPECT_EQ(42, result);
}

TEST_F(NativeMessagingReaderTest, InvalidLength) {
  uint32 length = 0xffffffff;
  WriteData(reinterpret_cast<char*>(&length), 4);
  Run();
  EXPECT_FALSE(message_);
}

TEST_F(NativeMessagingReaderTest, EmptyFile) {
  Run();
  EXPECT_FALSE(message_);
}

TEST_F(NativeMessagingReaderTest, ShortHeader) {
  // Write only 3 bytes - the message length header is supposed to be 4 bytes.
  WriteData("xxx", 3);
  Run();
  EXPECT_FALSE(message_);
}

TEST_F(NativeMessagingReaderTest, EmptyBody) {
  uint32 length = 1;
  WriteData(reinterpret_cast<char*>(&length), 4);
  Run();
  EXPECT_FALSE(message_);
}

TEST_F(NativeMessagingReaderTest, ShortBody) {
  uint32 length = 2;
  WriteData(reinterpret_cast<char*>(&length), 4);

  // Only write 1 byte, where the header indicates there should be 2 bytes.
  WriteData("x", 1);
  Run();
  EXPECT_FALSE(message_);
}

TEST_F(NativeMessagingReaderTest, InvalidJSON) {
  std::string text = "{";
  WriteMessage(text);
  Run();
  EXPECT_FALSE(message_);
}

TEST_F(NativeMessagingReaderTest, SecondMessage) {
  WriteMessage("{}");
  WriteMessage("{\"foo\": 42}");
  Run();
  EXPECT_TRUE(message_);
  base::DictionaryValue* message_dict;
  EXPECT_TRUE(message_->GetAsDictionary(&message_dict));
  int result;
  EXPECT_TRUE(message_dict->GetInteger("foo", &result));
  EXPECT_EQ(42, result);
}

}  // namespace remoting
