// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/buffered_socket_writer.h"

#include <stdlib.h>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

const int kTestBufferSize = 10000;
const size_t kWriteChunkSize = 1024U;

class SocketDataProvider: public net::SocketDataProvider {
 public:
  SocketDataProvider()
      : write_limit_(-1), async_write_(false), next_write_error_(net::OK) {}

  net::MockRead OnRead() override {
    return net::MockRead(net::ASYNC, net::ERR_IO_PENDING);
  }

  net::MockWriteResult OnWrite(const std::string& data) override {
    if (next_write_error_ != net::OK) {
      int r = next_write_error_;
      next_write_error_ = net::OK;
      return net::MockWriteResult(async_write_ ? net::ASYNC : net::SYNCHRONOUS,
                                  r);
    }
    int size = data.size();
    if (write_limit_ > 0)
      size = std::min(write_limit_, size);
    written_data_.append(data, 0, size);
    return net::MockWriteResult(async_write_ ? net::ASYNC : net::SYNCHRONOUS,
                                size);
  }

  bool AllReadDataConsumed() const override {
    return true;
  }

  bool AllWriteDataConsumed() const override {
    return true;
  }

  void Reset() override {}

  std::string written_data() { return written_data_; }

  void set_write_limit(int limit) { write_limit_ = limit; }
  void set_async_write(bool async_write) { async_write_ = async_write; }
  void set_next_write_error(int error) { next_write_error_ = error; }

 private:
  std::string written_data_;
  int write_limit_;
  bool async_write_;
  int next_write_error_;
};

}  // namespace

class BufferedSocketWriterTest : public testing::Test {
 public:
  BufferedSocketWriterTest()
      : write_error_(0) {
  }

  void DestroyWriter() {
    writer_.reset();
    socket_.reset();
  }

  void Unexpected() {
    EXPECT_TRUE(false);
  }

 protected:
  void SetUp() override {
    socket_.reset(new net::MockTCPClientSocket(net::AddressList(), &net_log_,
                                               &socket_data_provider_));
    socket_data_provider_.set_connect_data(
        net::MockConnect(net::SYNCHRONOUS, net::OK));
    EXPECT_EQ(net::OK, socket_->Connect(net::CompletionCallback()));

    writer_ = BufferedSocketWriter::CreateForSocket(
        socket_.get(), base::Bind(&BufferedSocketWriterTest::OnWriteFailed,
                                  base::Unretained(this)));
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
    writer_->Write(test_buffer_, base::Closure());
    writer_->Write(test_buffer_2_, base::Closure());
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(static_cast<size_t>(test_buffer_->size() +
                                  test_buffer_2_->size()),
              socket_data_provider_.written_data().size());
    EXPECT_EQ(0, memcmp(test_buffer_->data(),
                        socket_data_provider_.written_data().data(),
                        test_buffer_->size()));
    EXPECT_EQ(0, memcmp(test_buffer_2_->data(),
                        socket_data_provider_.written_data().data() +
                            test_buffer_->size(),
                        test_buffer_2_->size()));
  }

  void TestAppendInCallback() {
    writer_->Write(test_buffer_, base::Bind(
        base::IgnoreResult(&BufferedSocketWriter::Write),
        base::Unretained(writer_.get()), test_buffer_2_,
        base::Closure()));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(static_cast<size_t>(test_buffer_->size() +
                                  test_buffer_2_->size()),
              socket_data_provider_.written_data().size());
    EXPECT_EQ(0, memcmp(test_buffer_->data(),
                        socket_data_provider_.written_data().data(),
                        test_buffer_->size()));
    EXPECT_EQ(0, memcmp(test_buffer_2_->data(),
                        socket_data_provider_.written_data().data() +
                            test_buffer_->size(),
                        test_buffer_2_->size()));
  }

  base::MessageLoop message_loop_;
  net::NetLog net_log_;
  SocketDataProvider socket_data_provider_;
  scoped_ptr<net::StreamSocket> socket_;
  scoped_ptr<BufferedSocketWriter> writer_;
  scoped_refptr<net::IOBufferWithSize> test_buffer_;
  scoped_refptr<net::IOBufferWithSize> test_buffer_2_;
  int write_error_;
};

// Test synchronous write.
TEST_F(BufferedSocketWriterTest, WriteFull) {
  TestWrite();
}

// Test synchronous write in 1k chunks.
TEST_F(BufferedSocketWriterTest, WriteChunks) {
  socket_data_provider_.set_write_limit(kWriteChunkSize);
  TestWrite();
}

// Test asynchronous write.
TEST_F(BufferedSocketWriterTest, WriteAsync) {
  socket_data_provider_.set_async_write(true);
  socket_data_provider_.set_write_limit(kWriteChunkSize);
  TestWrite();
}

// Make sure we can call Write() from the done callback.
TEST_F(BufferedSocketWriterTest, AppendInCallbackSync) {
  TestAppendInCallback();
}

// Make sure we can call Write() from the done callback.
TEST_F(BufferedSocketWriterTest, AppendInCallbackAsync) {
  socket_data_provider_.set_async_write(true);
  socket_data_provider_.set_write_limit(kWriteChunkSize);
  TestAppendInCallback();
}

// Test that the writer can be destroyed from callback.
TEST_F(BufferedSocketWriterTest, DestroyFromCallback) {
  socket_data_provider_.set_async_write(true);
  writer_->Write(test_buffer_, base::Bind(
      &BufferedSocketWriterTest::DestroyWriter,
      base::Unretained(this)));
  writer_->Write(test_buffer_2_, base::Bind(
      &BufferedSocketWriterTest::Unexpected,
      base::Unretained(this)));
  socket_data_provider_.set_async_write(false);
  base::RunLoop().RunUntilIdle();
  ASSERT_GE(socket_data_provider_.written_data().size(),
            static_cast<size_t>(test_buffer_->size()));
  EXPECT_EQ(0, memcmp(test_buffer_->data(),
                      socket_data_provider_.written_data().data(),
                      test_buffer_->size()));
}

// Verify that it stops writing after the first error.
TEST_F(BufferedSocketWriterTest, TestWriteErrorSync) {
  socket_data_provider_.set_write_limit(kWriteChunkSize);
  writer_->Write(test_buffer_, base::Closure());
  socket_data_provider_.set_async_write(true);
  socket_data_provider_.set_next_write_error(net::ERR_FAILED);
  writer_->Write(test_buffer_2_,
                 base::Bind(&BufferedSocketWriterTest::Unexpected,
                            base::Unretained(this)));
  socket_data_provider_.set_async_write(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::ERR_FAILED, write_error_);
  EXPECT_EQ(static_cast<size_t>(test_buffer_->size()),
            socket_data_provider_.written_data().size());
}

// Verify that it stops writing after the first error.
TEST_F(BufferedSocketWriterTest, TestWriteErrorAsync) {
  socket_data_provider_.set_write_limit(kWriteChunkSize);
  writer_->Write(test_buffer_, base::Closure());
  socket_data_provider_.set_async_write(true);
  socket_data_provider_.set_next_write_error(net::ERR_FAILED);
  writer_->Write(test_buffer_2_,
                 base::Bind(&BufferedSocketWriterTest::Unexpected,
                            base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::ERR_FAILED, write_error_);
  EXPECT_EQ(static_cast<size_t>(test_buffer_->size()),
            socket_data_provider_.written_data().size());
}

}  // namespace remoting
