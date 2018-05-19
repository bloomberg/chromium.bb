// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/quartc/quartc_stream.h"

#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/quic_data_writer.h"
#include "net/third_party/quic/core/quic_session.h"
#include "net/third_party/quic/core/quic_simple_buffer_allocator.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/platform/api/quic_test_mem_slice_vector.h"
#include "net/third_party/quic/quartc/quartc_clock_interface.h"
#include "net/third_party/quic/quartc/quartc_factory.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "net/third_party/spdy/core/spdy_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

static const QuicStreamId kStreamId = 5;
static const QuartcStreamInterface::WriteParameters kDefaultParam;

// MockQuicSession that does not create streams and writes data from
// QuicStream to a string.
class MockQuicSession : public QuicSession {
 public:
  MockQuicSession(QuicConnection* connection,
                  const QuicConfig& config,
                  std::string* write_buffer)
      : QuicSession(connection, nullptr /*visitor*/, config),
        write_buffer_(write_buffer) {}

  ~MockQuicSession() override {}

  // Writes outgoing data from QuicStream to a string.
  QuicConsumedData WritevData(QuicStream* stream,
                              QuicStreamId id,
                              size_t write_length,
                              QuicStreamOffset offset,
                              StreamSendingState state) override {
    if (!writable_) {
      return QuicConsumedData(0, false);
    }

    // WritevData does not pass down a iovec, data is saved in stream before
    // data is consumed. Retrieve data from stream.
    char* buf = new char[write_length];
    QuicDataWriter writer(write_length, buf, NETWORK_BYTE_ORDER);
    if (write_length > 0) {
      stream->WriteStreamData(stream->stream_bytes_written(), write_length,
                              &writer);
    }
    write_buffer_->append(buf, write_length);
    delete[] buf;
    return QuicConsumedData(write_length, state != StreamSendingState::NO_FIN);
  }

  QuartcStream* CreateIncomingDynamicStream(QuicStreamId id) override {
    return nullptr;
  }

  QuartcStream* CreateOutgoingDynamicStream() override { return nullptr; }

  const QuicCryptoStream* GetCryptoStream() const override { return nullptr; }
  QuicCryptoStream* GetMutableCryptoStream() override { return nullptr; }

  // Called by QuicStream when they want to close stream.
  void SendRstStream(QuicStreamId id,
                     QuicRstStreamErrorCode error,
                     QuicStreamOffset bytes_written) override {}

  // Sets whether data is written to buffer, or else if this is write blocked.
  void set_writable(bool writable) { writable_ = writable; }

  // Tracks whether the stream is write blocked and its priority.
  void RegisterReliableStream(QuicStreamId stream_id,
                              spdy::SpdyPriority priority) {
    write_blocked_streams()->RegisterStream(stream_id,
                                            /*is_static_stream=*/false,
                                            priority);
  }

  // The session take ownership of the stream.
  void ActivateReliableStream(std::unique_ptr<QuicStream> stream) {
    ActivateStream(std::move(stream));
  }

 private:
  // Stores written data from ReliableQuicStreamAdapter.
  std::string* write_buffer_;
  // Whether data is written to write_buffer_.
  bool writable_ = true;
};

// Packet writer that does nothing. This is required for QuicConnection but
// isn't used for writing data.
class DummyPacketWriter : public QuicPacketWriter {
 public:
  DummyPacketWriter() {}

  // QuicPacketWriter overrides.
  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const QuicIpAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          PerPacketOptions* options) override {
    return WriteResult(WRITE_STATUS_ERROR, 0);
  }

  bool IsWriteBlockedDataBuffered() const override { return false; }

  bool IsWriteBlocked() const override { return false; };

  void SetWritable() override {}

  QuicByteCount GetMaxPacketSize(
      const QuicSocketAddress& peer_address) const override {
    return 0;
  }
};

class MockQuartcStreamDelegate : public QuartcStreamInterface::Delegate {
 public:
  MockQuartcStreamDelegate(int id, std::string* read_buffer)
      : id_(id), read_buffer_(read_buffer) {}

  void OnBufferChanged(QuartcStreamInterface* stream) override {
    last_bytes_buffered_ = stream->bytes_buffered();
  }

  void OnReceived(QuartcStreamInterface* stream,
                  const char* data,
                  size_t size) override {
    EXPECT_EQ(id_, stream->stream_id());
    read_buffer_->append(data, size);
  }

  void OnClose(QuartcStreamInterface* stream) override { closed_ = true; }

  bool closed() { return closed_; }

  uint64_t last_bytes_buffered() { return last_bytes_buffered_; }

 protected:
  uint32_t id_;
  // Data read by the QuicStream.
  std::string* read_buffer_;
  // Whether the QuicStream is closed.
  bool closed_ = false;

  // Last amount of data observed as buffered.
  uint64_t last_bytes_buffered_ = 0;
};

class QuartcStreamTest : public QuicTest, public QuicConnectionHelperInterface {
 public:
  void CreateReliableQuicStream() {
    // Arbitrary values for QuicConnection.
    Perspective perspective = Perspective::IS_SERVER;
    QuicIpAddress ip;
    ip.FromString("0.0.0.0");
    bool owns_writer = true;

    // We only use QuartcFactory for its role as an alarm factory.
    QuartcFactoryConfig config;
    alarm_factory_ = QuicMakeUnique<QuartcFactory>(config);

    connection_ = QuicMakeUnique<QuicConnection>(
        0, QuicSocketAddress(ip, 0), this /*QuicConnectionHelperInterface*/,
        alarm_factory_.get(), new DummyPacketWriter(), owns_writer, perspective,
        CurrentSupportedVersions());

    session_ = QuicMakeUnique<MockQuicSession>(connection_.get(), QuicConfig(),
                                               &write_buffer_);
    mock_stream_delegate_ =
        QuicMakeUnique<MockQuartcStreamDelegate>(kStreamId, &read_buffer_);
    stream_ = new QuartcStream(kStreamId, session_.get());
    stream_->SetDelegate(mock_stream_delegate_.get());
    session_->ActivateReliableStream(std::unique_ptr<QuartcStream>(stream_));
  }

  ~QuartcStreamTest() override {}

  const QuicClock* GetClock() const override { return &clock_; }

  QuicRandom* GetRandomGenerator() override {
    return QuicRandom::GetInstance();
  }

  QuicBufferAllocator* GetStreamSendBufferAllocator() override {
    return &buffer_allocator_;
  }

 protected:
  // The QuicSession will take the ownership.
  QuartcStream* stream_;
  std::unique_ptr<MockQuartcStreamDelegate> mock_stream_delegate_;
  std::unique_ptr<MockQuicSession> session_;
  // Data written by the ReliableQuicStreamAdapterTest.
  std::string write_buffer_;
  // Data read by the ReliableQuicStreamAdapterTest.
  std::string read_buffer_;
  std::unique_ptr<QuicAlarmFactory> alarm_factory_;
  std::unique_ptr<QuicConnection> connection_;
  // Used to implement the QuicConnectionHelperInterface.
  SimpleBufferAllocator buffer_allocator_;
  MockClock clock_;
};

// Write an entire string.
TEST_F(QuartcStreamTest, WriteDataWhole) {
  CreateReliableQuicStream();
  char message[] = "Foo bar";
  test::QuicTestMemSliceVector data({std::make_pair(message, 7)});
  stream_->Write(data.span(), kDefaultParam);
  EXPECT_EQ("Foo bar", write_buffer_);
}

// Write part of a string.
TEST_F(QuartcStreamTest, WriteDataPartial) {
  CreateReliableQuicStream();
  char message[] = "Foo bar";
  test::QuicTestMemSliceVector data({std::make_pair(message, 5)});
  stream_->Write(data.span(), kDefaultParam);
  EXPECT_EQ("Foo b", write_buffer_);
}

// Test that a QuartcStream buffers writes correctly.
TEST_F(QuartcStreamTest, StreamBuffersData) {
  CreateReliableQuicStream();

  char message[] = "Foo bar";
  test::QuicTestMemSliceVector data({std::make_pair(message, 7)});

  // The stream is not yet writable, so data will be buffered.
  session_->set_writable(false);
  stream_->Write(data.span(), kDefaultParam);

  // Check that data is buffered.
  EXPECT_TRUE(stream_->HasBufferedData());
  EXPECT_EQ(7u, stream_->bytes_buffered());

  // Check that the stream told its delegate about the buffer change.
  EXPECT_EQ(7u, mock_stream_delegate_->last_bytes_buffered());

  // Check that none of the data was written yet.
  // Note that |write_buffer_| actually holds data written by the QuicSession
  // (not data buffered by the stream).
  EXPECT_EQ(0ul, write_buffer_.size());

  char message1[] = "xyzzy";
  test::QuicTestMemSliceVector data1({std::make_pair(message1, 5)});

  // More writes go into the buffer.
  stream_->Write(data1.span(), kDefaultParam);

  EXPECT_TRUE(stream_->HasBufferedData());
  EXPECT_EQ(12u, stream_->bytes_buffered());
  EXPECT_EQ(12u, mock_stream_delegate_->last_bytes_buffered());
  EXPECT_EQ(0ul, write_buffer_.size());

  // The stream becomes writable, so it sends the buffered data.
  session_->set_writable(true);
  stream_->OnCanWrite();

  EXPECT_FALSE(stream_->HasBufferedData());
  EXPECT_EQ(0u, stream_->bytes_buffered());
  EXPECT_EQ(0u, mock_stream_delegate_->last_bytes_buffered());
  EXPECT_EQ("Foo barxyzzy", write_buffer_);
}

// Finish writing to a stream.
// It delivers the fin bit and closes the write-side as soon as possible.
TEST_F(QuartcStreamTest, FinishWriting) {
  CreateReliableQuicStream();

  session_->set_writable(false);
  stream_->FinishWriting();
  EXPECT_FALSE(stream_->fin_sent());

  // Fin is sent as soon as the stream becomes writable.
  session_->set_writable(true);
  stream_->OnCanWrite();
  EXPECT_TRUE(stream_->fin_sent());
  EXPECT_TRUE(stream_->write_side_closed());
}

// Read an entire string.
TEST_F(QuartcStreamTest, ReadDataWhole) {
  CreateReliableQuicStream();
  QuicStreamFrame frame(kStreamId, false, 0, "Hello, World!");
  stream_->OnStreamFrame(frame);

  EXPECT_EQ("Hello, World!", read_buffer_);
}

// Read part of a string.
TEST_F(QuartcStreamTest, ReadDataPartial) {
  CreateReliableQuicStream();
  QuicStreamFrame frame(kStreamId, false, 0, "Hello, World!");
  frame.data_length = 5;
  stream_->OnStreamFrame(frame);

  EXPECT_EQ("Hello", read_buffer_);
}

// Streams do not call OnReceived() after FinishReading().
TEST_F(QuartcStreamTest, FinishReading) {
  CreateReliableQuicStream();
  stream_->FinishReading();

  QuicStreamFrame frame(kStreamId, false, 0, "Hello, World!");
  stream_->OnStreamFrame(frame);

  EXPECT_EQ(0ul, read_buffer_.size());

  QuicStreamFrame frame2(kStreamId, true, 0, "Hello, World!");
  stream_->OnStreamFrame(frame2);

  EXPECT_EQ(0ul, read_buffer_.size());
  EXPECT_TRUE(stream_->fin_received());
}

// Test that closing the stream results in a callback.
TEST_F(QuartcStreamTest, CloseStream) {
  CreateReliableQuicStream();
  EXPECT_FALSE(mock_stream_delegate_->closed());
  stream_->OnClose();
  EXPECT_TRUE(mock_stream_delegate_->closed());
}

// Both sending and receiving fin automatically closes a stream.
TEST_F(QuartcStreamTest, CloseOnFins) {
  CreateReliableQuicStream();
  QuicStreamFrame frame(kStreamId, true, 0, 0);
  stream_->OnStreamFrame(frame);

  QuartcStreamInterface::WriteParameters param;
  param.fin = true;
  test::QuicTestMemSliceVector data({});
  stream_->Write(data.span(), param);

  // Check that the OnClose() callback occurred.
  EXPECT_TRUE(mock_stream_delegate_->closed());
}

}  // namespace

}  // namespace net
