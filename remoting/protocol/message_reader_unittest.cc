// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "remoting/protocol/fake_session.h"
#include "remoting/protocol/message_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/libjingle/source/talk/base/byteorder.h"

using testing::_;
using testing::DoAll;
using testing::Mock;
using testing::SaveArg;

namespace remoting {
namespace protocol {

namespace {
const char kTestMessage1[] = "Message1";
const char kTestMessage2[] = "Message2";

ACTION(CallDoneTask) {
  arg0.Run();
}
}  // namespace

class MockMessageReceivedCallback {
 public:
  MOCK_METHOD1(OnMessage, void(const base::Closure&));
};

class MessageReaderTest : public testing::Test {
 public:
  MessageReaderTest()
      : in_callback_(false) {
  }

  // Following two methods are used by the ReadFromCallback test.
  void AddSecondMessage(const base::Closure& task) {
    AddMessage(kTestMessage2);
    in_callback_ = true;
    task.Run();
    in_callback_ = false;
  }

  void OnSecondMessage(const base::Closure& task) {
    EXPECT_FALSE(in_callback_);
    task.Run();
  }

  // Used by the DeleteFromCallback() test.
  void DeleteReader(const base::Closure& task) {
    reader_.reset();
    task.Run();
  }

 protected:
  virtual void SetUp() OVERRIDE {
    reader_.reset(new MessageReader());
  }

  virtual void TearDown() OVERRIDE {
    STLDeleteElements(&messages_);
  }

  void InitReader() {
    reader_->Init(&socket_, base::Bind(
        &MessageReaderTest::OnMessage, base::Unretained(this)));
  }

  void AddMessage(const std::string& message) {
    std::string data = std::string(4, ' ') + message;
    talk_base::SetBE32(const_cast<char*>(data.data()), message.size());

    socket_.AppendInputData(std::vector<char>(data.begin(), data.end()));
  }

  bool CompareResult(CompoundBuffer* buffer, const std::string& expected) {
    std::string result(buffer->total_bytes(), ' ');
    buffer->CopyTo(const_cast<char*>(result.data()), result.size());
    return result == expected;
  }

  void OnMessage(scoped_ptr<CompoundBuffer> buffer,
                 const base::Closure& done_callback) {
    messages_.push_back(buffer.release());
    callback_.OnMessage(done_callback);
  }

  MessageLoop message_loop_;
  scoped_ptr<MessageReader> reader_;
  FakeSocket socket_;
  MockMessageReceivedCallback callback_;
  std::vector<CompoundBuffer*> messages_;
  bool in_callback_;
};

// Receive one message and process it with delay
TEST_F(MessageReaderTest, OneMessage_Delay) {
  base::Closure done_task;

  AddMessage(kTestMessage1);

  EXPECT_CALL(callback_, OnMessage(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&done_task));

  InitReader();
  message_loop_.RunAllPending();

  Mock::VerifyAndClearExpectations(&callback_);
  Mock::VerifyAndClearExpectations(&socket_);

  EXPECT_TRUE(CompareResult(messages_[0], kTestMessage1));

  // Verify that the reader starts reading again only after we've
  // finished processing the previous message.
  EXPECT_FALSE(socket_.read_pending());

  done_task.Run();

  EXPECT_TRUE(socket_.read_pending());
}

// Receive one message and process it instantly.
TEST_F(MessageReaderTest, OneMessage_Instant) {
  AddMessage(kTestMessage1);

  EXPECT_CALL(callback_, OnMessage(_))
      .Times(1)
      .WillOnce(CallDoneTask());

  InitReader();
  message_loop_.RunAllPending();

  EXPECT_TRUE(socket_.read_pending());
  EXPECT_EQ(1U, messages_.size());
}

// Receive two messages in one packet.
TEST_F(MessageReaderTest, TwoMessages_Together) {
  base::Closure done_task1;
  base::Closure done_task2;

  AddMessage(kTestMessage1);
  AddMessage(kTestMessage2);

  EXPECT_CALL(callback_, OnMessage(_))
      .Times(2)
      .WillOnce(SaveArg<0>(&done_task1))
      .WillOnce(SaveArg<0>(&done_task2));

  InitReader();
  message_loop_.RunAllPending();

  Mock::VerifyAndClearExpectations(&callback_);
  Mock::VerifyAndClearExpectations(&socket_);

  EXPECT_TRUE(CompareResult(messages_[0], kTestMessage1));
  EXPECT_TRUE(CompareResult(messages_[1], kTestMessage2));

  // Verify that the reader starts reading again only after we've
  // finished processing the previous message.
  EXPECT_FALSE(socket_.read_pending());

  done_task1.Run();
  message_loop_.RunAllPending();

  EXPECT_FALSE(socket_.read_pending());

  done_task2.Run();
  message_loop_.RunAllPending();

  EXPECT_TRUE(socket_.read_pending());
}

// Receive two messages in one packet, and process the first one
// instantly.
TEST_F(MessageReaderTest, TwoMessages_Instant) {
  base::Closure done_task2;

  AddMessage(kTestMessage1);
  AddMessage(kTestMessage2);

  EXPECT_CALL(callback_, OnMessage(_))
      .Times(2)
      .WillOnce(CallDoneTask())
      .WillOnce(SaveArg<0>(&done_task2));

  InitReader();
  message_loop_.RunAllPending();

  Mock::VerifyAndClearExpectations(&callback_);
  Mock::VerifyAndClearExpectations(&socket_);

  EXPECT_TRUE(CompareResult(messages_[1], kTestMessage2));

  // Verify that the reader starts reading again only after we've
  // finished processing the second message.
  EXPECT_FALSE(socket_.read_pending());

  done_task2.Run();

  EXPECT_TRUE(socket_.read_pending());
}

// Receive two messages in one packet, and process both of them
// instantly.
TEST_F(MessageReaderTest, TwoMessages_Instant2) {
  AddMessage(kTestMessage1);
  AddMessage(kTestMessage2);

  EXPECT_CALL(callback_, OnMessage(_))
      .Times(2)
      .WillOnce(CallDoneTask())
      .WillOnce(CallDoneTask());

  InitReader();
  message_loop_.RunAllPending();

  EXPECT_TRUE(socket_.read_pending());
}

// Receive two messages in separate packets.
TEST_F(MessageReaderTest, TwoMessages_Separately) {
  base::Closure done_task;

  AddMessage(kTestMessage1);

  EXPECT_CALL(callback_, OnMessage(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&done_task));

  InitReader();
  message_loop_.RunAllPending();

  Mock::VerifyAndClearExpectations(&callback_);
  Mock::VerifyAndClearExpectations(&socket_);

  EXPECT_TRUE(CompareResult(messages_[0], kTestMessage1));

  // Verify that the reader starts reading again only after we've
  // finished processing the previous message.
  EXPECT_FALSE(socket_.read_pending());

  done_task.Run();
  message_loop_.RunAllPending();

  EXPECT_TRUE(socket_.read_pending());

  // Write another message and verify that we receive it.
  EXPECT_CALL(callback_, OnMessage(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&done_task));
  AddMessage(kTestMessage2);
  message_loop_.RunAllPending();

  EXPECT_TRUE(CompareResult(messages_[1], kTestMessage2));

  // Verify that the reader starts reading again only after we've
  // finished processing the previous message.
  EXPECT_FALSE(socket_.read_pending());

  done_task.Run();

  EXPECT_TRUE(socket_.read_pending());
}

// Read() returns error.
TEST_F(MessageReaderTest, ReadError) {
  socket_.set_next_read_error(net::ERR_FAILED);

  // Add a message. It should never be read after the error above.
  AddMessage(kTestMessage1);

  EXPECT_CALL(callback_, OnMessage(_))
      .Times(0);

  InitReader();
}

// Verify that we the OnMessage callback is not reentered.
TEST_F(MessageReaderTest, ReadFromCallback) {
  AddMessage(kTestMessage1);

  EXPECT_CALL(callback_, OnMessage(_))
      .Times(2)
      .WillOnce(Invoke(this, &MessageReaderTest::AddSecondMessage))
      .WillOnce(Invoke(this, &MessageReaderTest::OnSecondMessage));

  InitReader();
  message_loop_.RunAllPending();

  EXPECT_TRUE(socket_.read_pending());
}

// Verify that we stop getting callbacks after deleting MessageReader.
TEST_F(MessageReaderTest, DeleteFromCallback) {
  base::Closure done_task1;
  base::Closure done_task2;

  AddMessage(kTestMessage1);
  AddMessage(kTestMessage2);

  // OnMessage() should never be called for the second message.
  EXPECT_CALL(callback_, OnMessage(_))
      .Times(1)
      .WillOnce(Invoke(this, &MessageReaderTest::DeleteReader));

  InitReader();
  message_loop_.RunAllPending();
}

}  // namespace protocol
}  // namespace remoting
