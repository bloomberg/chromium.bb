// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/buffered_socket_writer.h"

#include <stdlib.h>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "remoting/protocol/fake_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

namespace {
const int kTestBufferSize = 10 * 1024; // 10k;
const size_t kWriteChunkSize = 1024U;
}  // namespace

class BufferedSocketWriterTest : public testing::Test {
 public:
  BufferedSocketWriterTest()
      : write_error_(0) {
  }

  void OnDone() {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
  }

  void DestroyWriterAndQuit() {
    written_data_ = socket_->written_data();
    writer_.reset();
    socket_.reset();
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
  }

  void Unexpected() {
    EXPECT_TRUE(false);
  }

 protected:
  virtual void SetUp() OVERRIDE {
    socket_.reset(new FakeStreamSocket());
    writer_.reset(new BufferedSocketWriter());
    writer_->Init(socket_.get(), base::Bind(
        &BufferedSocketWriterTest::OnWriteFailed, base::Unretained(this)));
    test_buffer_ = new net::IOBufferWithSize(kTestBufferSize);
    test_buffer_2_ = new net::IOBufferWithSize(kTestBufferSize);
    for (int i = 0; i< kTestBufferSize; ++i) {
      test_buffer_->data()[i] = rand() % 256;
      test_buffer_2_->data()[i] = rand() % 256;
    }
  }

  void OnWriteFailed(int error) {
    write_error_ = error;
  }

  void TestWrite() {
    writer_->Write(test_buffer_, base::Bind(&BufferedSocketWriterTest::OnDone,
                                            base::Unretained(this)));
    writer_->Write(test_buffer_2_, base::Bind(&BufferedSocketWriterTest::OnDone,
                                              base::Unretained(this)));
    message_loop_.Run();
    ASSERT_EQ(static_cast<size_t>(test_buffer_->size() +
                                  test_buffer_2_->size()),
              socket_->written_data().size());
    EXPECT_EQ(0, memcmp(test_buffer_->data(), socket_->written_data().data(),
                        test_buffer_->size()));
    EXPECT_EQ(0, memcmp(test_buffer_2_->data(),
                        socket_->written_data().data() + test_buffer_->size(),
                        test_buffer_2_->size()));
  }

  void TestAppendInCallback() {
    writer_->Write(test_buffer_, base::Bind(
        base::IgnoreResult(&BufferedSocketWriterBase::Write),
        base::Unretained(writer_.get()), test_buffer_2_,
        base::Bind(&BufferedSocketWriterTest::OnDone,
                  base::Unretained(this))));
    message_loop_.Run();
    ASSERT_EQ(static_cast<size_t>(test_buffer_->size() +
                                  test_buffer_2_->size()),
              socket_->written_data().size());
    EXPECT_EQ(0, memcmp(test_buffer_->data(), socket_->written_data().data(),
                        test_buffer_->size()));
    EXPECT_EQ(0, memcmp(test_buffer_2_->data(),
                        socket_->written_data().data() + test_buffer_->size(),
                        test_buffer_2_->size()));
  }

  base::MessageLoop message_loop_;
  scoped_ptr<FakeStreamSocket> socket_;
  scoped_ptr<BufferedSocketWriter> writer_;
  scoped_refptr<net::IOBufferWithSize> test_buffer_;
  scoped_refptr<net::IOBufferWithSize> test_buffer_2_;
  std::string written_data_;
  int write_error_;
};

// Test synchronous write.
TEST_F(BufferedSocketWriterTest, WriteFull) {
  TestWrite();
}

// Test synchronous write in 1k chunks.
TEST_F(BufferedSocketWriterTest, WriteChunks) {
  socket_->set_write_limit(kWriteChunkSize);
  TestWrite();
}

// Test asynchronous write.
TEST_F(BufferedSocketWriterTest, WriteAsync) {
  socket_->set_async_write(true);
  socket_->set_write_limit(kWriteChunkSize);
  TestWrite();
}

// Make sure we can call Write() from the done callback.
TEST_F(BufferedSocketWriterTest, AppendInCallbackSync) {
  TestAppendInCallback();
}

// Make sure we can call Write() from the done callback.
TEST_F(BufferedSocketWriterTest, AppendInCallbackAsync) {
  socket_->set_async_write(true);
  socket_->set_write_limit(kWriteChunkSize);
  TestAppendInCallback();
}

// Test that the writer can be destroyed from callback.
TEST_F(BufferedSocketWriterTest, DestroyFromCallback) {
  socket_->set_async_write(true);
  writer_->Write(test_buffer_, base::Bind(
      &BufferedSocketWriterTest::DestroyWriterAndQuit,
      base::Unretained(this)));
  writer_->Write(test_buffer_2_, base::Bind(
      &BufferedSocketWriterTest::Unexpected,
      base::Unretained(this)));
  socket_->set_async_write(false);
  message_loop_.Run();
  ASSERT_GE(written_data_.size(),
            static_cast<size_t>(test_buffer_->size()));
  EXPECT_EQ(0, memcmp(test_buffer_->data(), written_data_.data(),
                      test_buffer_->size()));
}

// Verify that it stops writing after the first error.
TEST_F(BufferedSocketWriterTest, TestWriteErrorSync) {
  socket_->set_write_limit(kWriteChunkSize);
  writer_->Write(test_buffer_, base::Closure());
  socket_->set_async_write(true);
  writer_->Write(test_buffer_2_,
                 base::Bind(&BufferedSocketWriterTest::Unexpected,
                            base::Unretained(this)));
  socket_->set_next_write_error(net::ERR_FAILED);
  socket_->set_async_write(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::ERR_FAILED, write_error_);
  EXPECT_EQ(static_cast<size_t>(test_buffer_->size()),
            socket_->written_data().size());
}

// Verify that it stops writing after the first error.
TEST_F(BufferedSocketWriterTest, TestWriteErrorAsync) {
  socket_->set_write_limit(kWriteChunkSize);
  writer_->Write(test_buffer_, base::Closure());
  socket_->set_async_write(true);
  writer_->Write(test_buffer_2_,
                 base::Bind(&BufferedSocketWriterTest::Unexpected,
                            base::Unretained(this)));
  socket_->set_next_write_error(net::ERR_FAILED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::ERR_FAILED, write_error_);
  EXPECT_EQ(static_cast<size_t>(test_buffer_->size()),
            socket_->written_data().size());
}

}  // namespace protocol
}  // namespace remoting

