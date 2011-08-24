// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
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
  arg1->Run();
  delete arg1;
}
}

class MockMessageReceivedCallback {
 public:
  MOCK_METHOD2(OnMessage, void(CompoundBuffer*, Task*));
};

class MessageReaderTest : public testing::Test {
 public:
  MessageReaderTest()
      : other_thread_("SecondTestThread"),
        run_task_finished_(false, false) {
  }

  void RunDoneTaskOnOtherThread(CompoundBuffer* buffer, Task* done_task) {
    other_thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&MessageReaderTest::RunAndDeleteTask,
                   base::Unretained(this),
                   done_task));
  }

 protected:
  virtual void SetUp() {
    reader_ = new MessageReader();
  }

  void InitReader() {
    reader_->Init(&socket_, NewCallback(
        &callback_, &MockMessageReceivedCallback::OnMessage));
  }

  void AddMessage(const std::string& message) {
    std::string data = std::string(4, ' ') + message;
    talk_base::SetBE32(const_cast<char*>(data.data()), message.size());

    socket_.AppendInputData(data.data(), data.size());
  }

  bool CompareResult(CompoundBuffer* buffer, const std::string& expected) {
    std::string result(buffer->total_bytes(), ' ');
    buffer->CopyTo(const_cast<char*>(result.data()), result.size());
    return result == expected;
  }

  void RunAndDeleteTask(Task* task) {
    task->Run();
    delete task;
    run_task_finished_.Signal();
  }

  MessageLoop message_loop_;
  base::Thread other_thread_;
  base::WaitableEvent run_task_finished_;
  scoped_refptr<MessageReader> reader_;
  FakeSocket socket_;
  MockMessageReceivedCallback callback_;
};

// Receive one message and process it with delay
TEST_F(MessageReaderTest, OneMessage_Delay) {
  CompoundBuffer* buffer;
  Task* done_task;

  AddMessage(kTestMessage1);

  EXPECT_CALL(callback_, OnMessage(_, _))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&buffer),
                      SaveArg<1>(&done_task)));

  InitReader();

  Mock::VerifyAndClearExpectations(&callback_);
  Mock::VerifyAndClearExpectations(&socket_);

  EXPECT_TRUE(CompareResult(buffer, kTestMessage1));

  // Verify that the reader starts reading again only after we've
  // finished processing the previous message.
  EXPECT_FALSE(socket_.read_pending());

  RunAndDeleteTask(done_task);

  EXPECT_TRUE(socket_.read_pending());
}

// Receive one message and process it instantly.
TEST_F(MessageReaderTest, OneMessage_Instant) {
  AddMessage(kTestMessage1);

  EXPECT_CALL(callback_, OnMessage(_, _))
      .Times(1)
      .WillOnce(CallDoneTask());

  InitReader();

  EXPECT_TRUE(socket_.read_pending());
}

// Receive two messages in one packet.
TEST_F(MessageReaderTest, TwoMessages_Together) {
  CompoundBuffer* buffer1;
  Task* done_task1;
  CompoundBuffer* buffer2;
  Task* done_task2;

  AddMessage(kTestMessage1);
  AddMessage(kTestMessage2);

  EXPECT_CALL(callback_, OnMessage(_, _))
      .Times(2)
      .WillOnce(DoAll(SaveArg<0>(&buffer1),
                      SaveArg<1>(&done_task1)))
      .WillOnce(DoAll(SaveArg<0>(&buffer2),
                      SaveArg<1>(&done_task2)));

  InitReader();

  Mock::VerifyAndClearExpectations(&callback_);
  Mock::VerifyAndClearExpectations(&socket_);

  EXPECT_TRUE(CompareResult(buffer1, kTestMessage1));
  EXPECT_TRUE(CompareResult(buffer2, kTestMessage2));

  // Verify that the reader starts reading again only after we've
  // finished processing the previous message.
  EXPECT_FALSE(socket_.read_pending());

  RunAndDeleteTask(done_task1);

  EXPECT_FALSE(socket_.read_pending());

  RunAndDeleteTask(done_task2);

  EXPECT_TRUE(socket_.read_pending());
}

// Receive two messages in one packet, and process the first one
// instantly.
TEST_F(MessageReaderTest, TwoMessages_Instant) {
  CompoundBuffer* buffer2;
  Task* done_task2;

  AddMessage(kTestMessage1);
  AddMessage(kTestMessage2);

  EXPECT_CALL(callback_, OnMessage(_, _))
      .Times(2)
      .WillOnce(CallDoneTask())
      .WillOnce(DoAll(SaveArg<0>(&buffer2),
                      SaveArg<1>(&done_task2)));

  InitReader();

  Mock::VerifyAndClearExpectations(&callback_);
  Mock::VerifyAndClearExpectations(&socket_);

  EXPECT_TRUE(CompareResult(buffer2, kTestMessage2));

  // Verify that the reader starts reading again only after we've
  // finished processing the second message.
  EXPECT_FALSE(socket_.read_pending());

  RunAndDeleteTask(done_task2);

  EXPECT_TRUE(socket_.read_pending());
}

// Receive two messages in one packet, and process both of them
// instantly.
TEST_F(MessageReaderTest, TwoMessages_Instant2) {
  AddMessage(kTestMessage1);
  AddMessage(kTestMessage2);

  EXPECT_CALL(callback_, OnMessage(_, _))
      .Times(2)
      .WillOnce(CallDoneTask())
      .WillOnce(CallDoneTask());

  InitReader();

  EXPECT_TRUE(socket_.read_pending());
}

// Receive two messages in separate packets.
TEST_F(MessageReaderTest, TwoMessages_Separately) {
  CompoundBuffer* buffer;
  Task* done_task;

  AddMessage(kTestMessage1);

  EXPECT_CALL(callback_, OnMessage(_, _))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&buffer),
                      SaveArg<1>(&done_task)));

  InitReader();

  Mock::VerifyAndClearExpectations(&callback_);
  Mock::VerifyAndClearExpectations(&socket_);

  EXPECT_TRUE(CompareResult(buffer, kTestMessage1));

  // Verify that the reader starts reading again only after we've
  // finished processing the previous message.
  EXPECT_FALSE(socket_.read_pending());

  RunAndDeleteTask(done_task);

  EXPECT_TRUE(socket_.read_pending());

  // Write another message and verify that we receive it.
  EXPECT_CALL(callback_, OnMessage(_, _))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&buffer),
                      SaveArg<1>(&done_task)));
  AddMessage(kTestMessage2);

  EXPECT_TRUE(CompareResult(buffer, kTestMessage2));

  // Verify that the reader starts reading again only after we've
  // finished processing the previous message.
  EXPECT_FALSE(socket_.read_pending());

  RunAndDeleteTask(done_task);

  EXPECT_TRUE(socket_.read_pending());
}

// Verify that socket operations occur on same thread, even when the OnMessage()
// callback triggers |done_task| to run on a different thread.
TEST_F(MessageReaderTest, UseSocketOnCorrectThread) {

  AddMessage(kTestMessage1);
  other_thread_.Start();

  EXPECT_CALL(callback_, OnMessage(_, _))
      .Times(1)
      .WillOnce(Invoke(this, &MessageReaderTest::RunDoneTaskOnOtherThread));

  InitReader();

  run_task_finished_.Wait();
  message_loop_.RunAllPending();

  // Write another message and verify that we receive it.
  CompoundBuffer* buffer;
  Task* done_task;
  EXPECT_CALL(callback_, OnMessage(_, _))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&buffer),
                      SaveArg<1>(&done_task)));
  AddMessage(kTestMessage2);
  EXPECT_TRUE(CompareResult(buffer, kTestMessage2));

  RunAndDeleteTask(done_task);
}

}  // namespace protocol
}  // namespace remoting
