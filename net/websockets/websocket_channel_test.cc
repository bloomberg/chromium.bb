// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_channel.h"

#include <iostream>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_piece.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_mux.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Printing helpers to allow GoogleMock to print frame chunks. These are
// explicitly designed to look like the static initialisation format we use in
// these tests. They have to live in the net namespace in order to be found by
// GoogleMock; a nested anonymous namespace will not work.

std::ostream& operator<<(std::ostream& os, const WebSocketFrameHeader& header) {
  return os << "{" << (header.final ? "FINAL_FRAME" : "NOT_FINAL_FRAME") << ", "
            << header.opcode << ", "
            << (header.masked ? "MASKED" : "NOT_MASKED") << ", "
            << header.payload_length << "}";
}

std::ostream& operator<<(std::ostream& os, const WebSocketFrameChunk& chunk) {
  os << "{";
  if (chunk.header) {
    os << *chunk.header;
  } else {
    os << "{NO_HEADER}";
  }
  return os << ", " << (chunk.final_chunk ? "FINAL_CHUNK" : "NOT_FINAL_CHUNK")
            << ", \"" << base::StringPiece(chunk.data->data(),
                                           chunk.data->size()) << "\"}";
}

namespace {

using ::testing::AnyNumber;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

// This mock is for testing expectations about how the EventInterface is used.
class MockWebSocketEventInterface : public WebSocketEventInterface {
 public:
  MOCK_METHOD2(OnAddChannelResponse, void(bool, const std::string&));
  MOCK_METHOD3(OnDataFrame,
               void(bool, WebSocketMessageType, const std::vector<char>&));
  MOCK_METHOD1(OnFlowControl, void(int64));
  MOCK_METHOD0(OnClosingHandshake, void(void));
  MOCK_METHOD2(OnDropChannel, void(uint16, const std::string&));
};

// This fake EventInterface is for tests which need a WebSocketEventInterface
// implementation but are not verifying how it is used.
class FakeWebSocketEventInterface : public WebSocketEventInterface {
  virtual void OnAddChannelResponse(
      bool fail,
      const std::string& selected_protocol) OVERRIDE {}
  virtual void OnDataFrame(bool fin,
                           WebSocketMessageType type,
                           const std::vector<char>& data) OVERRIDE {}
  virtual void OnFlowControl(int64 quota) OVERRIDE {}
  virtual void OnClosingHandshake() OVERRIDE {}
  virtual void OnDropChannel(uint16 code, const std::string& reason) OVERRIDE {}
};

// This fake WebSocketStream is for tests that require a WebSocketStream but are
// not testing the way it is used. It has minimal functionality to return
// the |protocol| and |extensions| that it was constructed with.
class FakeWebSocketStream : public WebSocketStream {
 public:
  // Constructs with empty protocol and extensions.
  FakeWebSocketStream() {}

  // Constructs with specified protocol and extensions.
  FakeWebSocketStream(const std::string& protocol,
                      const std::string& extensions)
      : protocol_(protocol), extensions_(extensions) {}

  virtual int SendHandshakeRequest(
      const GURL& url,
      const HttpRequestHeaders& headers,
      HttpResponseInfo* response_info,
      const CompletionCallback& callback) OVERRIDE {
    return ERR_IO_PENDING;
  }

  virtual int ReadHandshakeResponse(
      const CompletionCallback& callback) OVERRIDE {
    return ERR_IO_PENDING;
  }

  virtual int ReadFrames(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                         const CompletionCallback& callback) OVERRIDE {
    return ERR_IO_PENDING;
  }

  virtual int WriteFrames(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                          const CompletionCallback& callback) OVERRIDE {
    return ERR_IO_PENDING;
  }

  virtual void Close() OVERRIDE {}

  // Returns the string passed to the constructor.
  virtual std::string GetSubProtocol() const OVERRIDE { return protocol_; }

  // Returns the string passed to the constructor.
  virtual std::string GetExtensions() const OVERRIDE { return extensions_; }

 private:
  // The string to return from GetSubProtocol().
  std::string protocol_;

  // The string to return from GetExtensions().
  std::string extensions_;
};

// To make the static initialisers easier to read, we use enums rather than
// bools.

// NO_HEADER means there shouldn't be a header included in the generated
// WebSocketFrameChunk. The static initialiser always has a header, but we can
// avoid specifying the rest of the fields.
enum IsFinal {
  NO_HEADER,
  NOT_FINAL_FRAME,
  FINAL_FRAME
};

enum IsMasked {
  NOT_MASKED,
  MASKED
};

enum IsFinalChunk {
  NOT_FINAL_CHUNK,
  FINAL_CHUNK
};

// This is used to initialise a WebSocketFrameChunk but is statically
// initialisable.
struct InitFrameChunk {
  struct FrameHeader {
    IsFinal final;
    // Reserved fields omitted for now. Add them if you need them.
    WebSocketFrameHeader::OpCode opcode;
    IsMasked masked;
    // payload_length is the length of the whole frame. The length of the data
    // members from every chunk in the frame must add up to the payload_length.
    uint64 payload_length;
  };
  FrameHeader header;

  // Directly equivalent to WebSocketFrameChunk::final_chunk
  IsFinalChunk final_chunk;

  // Will be used to create the IOBuffer member. Can be NULL for NULL data. Is a
  // nul-terminated string for ease-of-use. This means it is not 8-bit clean,
  // but this is not an issue for test data.
  const char* const data;
};

// Convert a const array of InitFrameChunks to the format used at
// runtime. Templated on the size of the array to save typing.
template <size_t N>
ScopedVector<WebSocketFrameChunk> CreateFrameChunkVector(
    const InitFrameChunk (&source_chunks)[N]) {
  ScopedVector<WebSocketFrameChunk> result_chunks;
  result_chunks.reserve(N);
  for (size_t i = 0; i < N; ++i) {
    scoped_ptr<WebSocketFrameChunk> result_chunk(new WebSocketFrameChunk);
    size_t chunk_length =
        source_chunks[i].data ? strlen(source_chunks[i].data) : 0;
    if (source_chunks[i].header.final != NO_HEADER) {
      const InitFrameChunk::FrameHeader& source_header =
          source_chunks[i].header;
      scoped_ptr<WebSocketFrameHeader> result_header(
          new WebSocketFrameHeader(source_header.opcode));
      result_header->final = (source_header.final == FINAL_FRAME);
      result_header->opcode = source_header.opcode;
      result_header->masked = (source_header.masked == MASKED);
      result_header->payload_length = source_header.payload_length;
      DCHECK(chunk_length <= source_header.payload_length);
      result_chunk->header.swap(result_header);
    }
    result_chunk->final_chunk = (source_chunks[i].final_chunk == FINAL_CHUNK);
    if (source_chunks[i].data) {
      result_chunk->data = new IOBufferWithSize(chunk_length);
      memcpy(result_chunk->data->data(), source_chunks[i].data, chunk_length);
    }
    result_chunks.push_back(result_chunk.release());
  }
  return result_chunks.Pass();
}

// A GoogleMock action which can be used to respond to call to ReadFrames with
// some frames. Use like ReadFrames(_, _).WillOnce(ReturnChunks(chunks));
ACTION_P(ReturnChunks, source_chunks) {
  *arg0 = CreateFrameChunkVector(source_chunks);
  return OK;
}

// A FakeWebSocketStream whose ReadFrames() function returns data.
class ReadableFakeWebSocketStream : public FakeWebSocketStream {
 public:
  enum IsSync {
    SYNC,
    ASYNC
  };

  // After constructing the object, call PrepareReadFrames() once for each
  // time you wish it to return from the test.
  ReadableFakeWebSocketStream() : index_(0), read_frames_pending_(false) {}

  // Check that all the prepared responses have been consumed.
  virtual ~ReadableFakeWebSocketStream() {
    CHECK(index_ >= responses_.size());
    CHECK(!read_frames_pending_);
  }

  // Prepares a fake responses. Fake responses will be returned from
  // ReadFrames() in the same order they were prepared with PrepareReadFrames()
  // and PrepareReadFramesError(). If |async| is ASYNC, then ReadFrames() will
  // return ERR_IO_PENDING and the callback will be scheduled to run on the
  // message loop. This requires the test case to run the message loop. If
  // |async| is SYNC, the response will be returned synchronously. |error| is
  // returned directly from ReadFrames() in the synchronous case, or passed to
  // the callback in the asynchronous case. |chunks| will be converted to a
  // ScopedVector<WebSocketFrameChunks> and copied to the pointer that was
  // passed to ReadFrames().
  template <size_t N>
  void PrepareReadFrames(IsSync async,
                         int error,
                         const InitFrameChunk (&chunks)[N]) {
    responses_.push_back(
        new Response(async, error, CreateFrameChunkVector(chunks)));
  }

  // Prepares a fake error response (ie. there is no data).
  void PrepareReadFramesError(IsSync async, int error) {
    responses_.push_back(
        new Response(async, error, ScopedVector<WebSocketFrameChunk>()));
  }

  virtual int ReadFrames(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                         const CompletionCallback& callback) OVERRIDE {
    CHECK(!read_frames_pending_);
    if (index_ >= responses_.size())
      return ERR_IO_PENDING;
    if (responses_[index_]->async == ASYNC) {
      read_frames_pending_ = true;
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ReadableFakeWebSocketStream::DoCallback,
                     base::Unretained(this),
                     frame_chunks,
                     callback));
      return ERR_IO_PENDING;
    } else {
      frame_chunks->swap(responses_[index_]->chunks);
      return responses_[index_++]->error;
    }
  }

 private:
  void DoCallback(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                  const CompletionCallback& callback) {
    read_frames_pending_ = false;
    frame_chunks->swap(responses_[index_]->chunks);
    callback.Run(responses_[index_++]->error);
    return;
  }

  struct Response {
    Response(IsSync async, int error, ScopedVector<WebSocketFrameChunk> chunks)
        : async(async), error(error), chunks(chunks.Pass()) {}

    IsSync async;
    int error;
    ScopedVector<WebSocketFrameChunk> chunks;

   private:
    // Bad things will happen if we attempt to copy or assign "chunks".
    DISALLOW_COPY_AND_ASSIGN(Response);
  };
  ScopedVector<Response> responses_;

  // The index into the responses_ array of the next response to be returned.
  size_t index_;

  // True when an async response from ReadFrames() is pending. This only applies
  // to "real" async responses. Once all the prepared responses have been
  // returned, ReadFrames() returns ERR_IO_PENDING but read_frames_pending_ is
  // not set to true.
  bool read_frames_pending_;
};

// A FakeWebSocketStream where writes always complete successfully and
// synchronously.
class WriteableFakeWebSocketStream : public FakeWebSocketStream {
 public:
  virtual int WriteFrames(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                          const CompletionCallback& callback) OVERRIDE {
    return OK;
  }
};

// A FakeWebSocketStream where writes always fail.
class UnWriteableFakeWebSocketStream : public FakeWebSocketStream {
 public:
  virtual int WriteFrames(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                          const CompletionCallback& callback) OVERRIDE {
    return ERR_CONNECTION_RESET;
  }
};

// A FakeWebSocketStream which echoes any frames written back. Clears the
// "masked" header bit, but makes no other checks for validity. Tests using this
// must run the MessageLoop to receive the callback(s). If a message with opcode
// Close is echoed, then an ERR_CONNECTION_CLOSED is returned in the next
// callback. The test must do something to cause WriteFrames() to be called,
// otherwise the ReadFrames() callback will never be called.
class EchoeyFakeWebSocketStream : public FakeWebSocketStream {
 public:
  EchoeyFakeWebSocketStream() : read_frame_chunks_(NULL), done_(false) {}

  virtual int WriteFrames(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                          const CompletionCallback& callback) OVERRIDE {
    // Users of WebSocketStream will not expect the ReadFrames() callback to be
    // called from within WriteFrames(), so post it to the message loop instead.
    stored_frame_chunks_.insert(
        stored_frame_chunks_.end(), frame_chunks->begin(), frame_chunks->end());
    frame_chunks->weak_clear();
    PostCallback();
    return OK;
  }

  virtual int ReadFrames(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                         const CompletionCallback& callback) OVERRIDE {
    read_callback_ = callback;
    read_frame_chunks_ = frame_chunks;
    if (done_)
      PostCallback();
    return ERR_IO_PENDING;
  }

 private:
  void PostCallback() {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&EchoeyFakeWebSocketStream::DoCallback,
                   base::Unretained(this)));
  }

  void DoCallback() {
    if (done_) {
      read_callback_.Run(ERR_CONNECTION_CLOSED);
    } else if (!stored_frame_chunks_.empty()) {
      done_ = MoveFrameChunks(read_frame_chunks_);
      read_frame_chunks_ = NULL;
      read_callback_.Run(OK);
    }
  }

  // Copy the chunks stored in stored_frame_chunks_ to |out|, while clearing the
  // "masked" header bit. Returns true if a Close Frame was seen, false
  // otherwise.
  bool MoveFrameChunks(ScopedVector<WebSocketFrameChunk>* out) {
    bool seen_close = false;
    *out = stored_frame_chunks_.Pass();
    for (ScopedVector<WebSocketFrameChunk>::iterator it = out->begin();
         it != out->end();
         ++it) {
      WebSocketFrameHeader* header = (*it)->header.get();
      if (header) {
        header->masked = false;
        if (header->opcode == WebSocketFrameHeader::kOpCodeClose)
          seen_close = true;
      }
    }
    return seen_close;
  }

  ScopedVector<WebSocketFrameChunk> stored_frame_chunks_;
  CompletionCallback read_callback_;
  // Owned by the caller of ReadFrames().
  ScopedVector<WebSocketFrameChunk>* read_frame_chunks_;
  // True if we should close the connection.
  bool done_;
};

// This mock is for verifying that WebSocket protocol semantics are obeyed (to
// the extent that they are implemented in WebSocketCommon).
class MockWebSocketStream : public WebSocketStream {
 public:
  MOCK_METHOD2(ReadFrames,
               int(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                   const CompletionCallback& callback));
  MOCK_METHOD2(WriteFrames,
               int(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                   const CompletionCallback& callback));
  MOCK_METHOD0(Close, void());
  MOCK_CONST_METHOD0(GetSubProtocol, std::string());
  MOCK_CONST_METHOD0(GetExtensions, std::string());
  MOCK_METHOD0(AsWebSocketStream, WebSocketStream*());
  MOCK_METHOD4(SendHandshakeRequest,
               int(const GURL& url,
                   const HttpRequestHeaders& headers,
                   HttpResponseInfo* response_info,
                   const CompletionCallback& callback));
  MOCK_METHOD1(ReadHandshakeResponse, int(const CompletionCallback& callback));
};

struct ArgumentCopyingWebSocketFactory {
  scoped_ptr<WebSocketStreamRequest> Factory(
      const GURL& socket_url,
      const std::vector<std::string>& requested_subprotocols,
      const GURL& origin,
      URLRequestContext* url_request_context,
      const BoundNetLog& net_log,
      scoped_ptr<WebSocketStream::ConnectDelegate> connect_delegate) {
    this->socket_url = socket_url;
    this->requested_subprotocols = requested_subprotocols;
    this->origin = origin;
    this->url_request_context = url_request_context;
    this->net_log = net_log;
    this->connect_delegate = connect_delegate.Pass();
    return make_scoped_ptr(new WebSocketStreamRequest);
  }

  GURL socket_url;
  GURL origin;
  std::vector<std::string> requested_subprotocols;
  URLRequestContext* url_request_context;
  BoundNetLog net_log;
  scoped_ptr<WebSocketStream::ConnectDelegate> connect_delegate;
};

// Converts a std::string to a std::vector<char>. For test purposes, it is
// convenient to be able to specify data as a string, but the
// WebSocketEventInterface requires the vector<char> type.
std::vector<char> AsVector(const std::string& s) {
  return std::vector<char>(s.begin(), s.end());
}

// Base class for all test fixtures.
class WebSocketChannelTest : public ::testing::Test {
 protected:
  WebSocketChannelTest() : stream_(new FakeWebSocketStream) {}

  // Creates a new WebSocketChannel and connects it, using the settings stored
  // in |connect_data_|.
  void CreateChannelAndConnect() {
    channel_.reset(
        new WebSocketChannel(connect_data_.url, CreateEventInterface()));
    channel_->SendAddChannelRequestForTesting(
        connect_data_.requested_subprotocols,
        connect_data_.origin,
        &connect_data_.url_request_context,
        base::Bind(&ArgumentCopyingWebSocketFactory::Factory,
                   base::Unretained(&connect_data_.factory)));
  }

  // Same as CreateChannelAndConnect(), but calls the on_success callback as
  // well. This method is virtual so that subclasses can also set the stream.
  virtual void CreateChannelAndConnectSuccessfully() {
    CreateChannelAndConnect();
    connect_data_.factory.connect_delegate->OnSuccess(stream_.Pass());
  }

  // Returns a WebSocketEventInterface to be passed to the WebSocketChannel.
  // This implementation returns a newly-created fake. Subclasses may return a
  // mock instead.
  virtual scoped_ptr<WebSocketEventInterface> CreateEventInterface() {
    return scoped_ptr<WebSocketEventInterface>(new FakeWebSocketEventInterface);
  }

  // This method serves no other purpose than to provide a nice syntax for
  // assigning to stream_. class T must be a subclass of WebSocketStream or you
  // will have unpleasant compile errors.
  template <class T>
  void set_stream(scoped_ptr<T> stream) {
    // Since the definition of "PassAs" depends on the type T, the C++ standard
    // requires the "template" keyword to indicate that "PassAs" should be
    // parsed as a template method.
    stream_ = stream.template PassAs<WebSocketStream>();
  }

  // A struct containing the data that will be used to connect the channel.
  struct ConnectData {
    // URL to (pretend to) connect to.
    GURL url;
    // Origin of the request
    GURL origin;
    // Requested protocols for the request.
    std::vector<std::string> requested_subprotocols;
    // URLRequestContext object.
    URLRequestContext url_request_context;
    // A fake WebSocketFactory that just records its arguments.
    ArgumentCopyingWebSocketFactory factory;
  };
  ConnectData connect_data_;

  // The channel we are testing. Not initialised until SetChannel() is called.
  scoped_ptr<WebSocketChannel> channel_;

  // A mock or fake stream for tests that need one.
  scoped_ptr<WebSocketStream> stream_;
};

// Base class for tests which verify that EventInterface methods are called
// appropriately.
class WebSocketChannelEventInterfaceTest : public WebSocketChannelTest {
 protected:
  WebSocketChannelEventInterfaceTest()
      : event_interface_(new StrictMock<MockWebSocketEventInterface>) {}

  // Tests using this fixture must set expectations on the event_interface_ mock
  // object before calling CreateChannelAndConnect() or
  // CreateChannelAndConnectSuccessfully(). This will only work once per test
  // case, but once should be enough.
  virtual scoped_ptr<WebSocketEventInterface> CreateEventInterface() OVERRIDE {
    return scoped_ptr<WebSocketEventInterface>(event_interface_.release());
  }

  scoped_ptr<MockWebSocketEventInterface> event_interface_;
};

// Base class for tests which verify that WebSocketStream methods are called
// appropriately by using a MockWebSocketStream.
class WebSocketChannelStreamTest : public WebSocketChannelTest {
 protected:
  WebSocketChannelStreamTest()
      : mock_stream_(new StrictMock<MockWebSocketStream>) {}

  virtual void CreateChannelAndConnectSuccessfully() OVERRIDE {
    set_stream(mock_stream_.Pass());
    WebSocketChannelTest::CreateChannelAndConnectSuccessfully();
  }

  scoped_ptr<MockWebSocketStream> mock_stream_;
};

// Simple test that everything that should be passed to the factory function is
// passed to the factory function.
TEST_F(WebSocketChannelTest, EverythingIsPassedToTheFactoryFunction) {
  connect_data_.url = GURL("ws://example.com/test");
  connect_data_.origin = GURL("http://example.com/test");
  connect_data_.requested_subprotocols.push_back("Sinbad");

  CreateChannelAndConnect();

  EXPECT_EQ(connect_data_.url, connect_data_.factory.socket_url);
  EXPECT_EQ(connect_data_.origin, connect_data_.factory.origin);
  EXPECT_EQ(connect_data_.requested_subprotocols,
            connect_data_.factory.requested_subprotocols);
  EXPECT_EQ(&connect_data_.url_request_context,
            connect_data_.factory.url_request_context);
}

TEST_F(WebSocketChannelEventInterfaceTest, ConnectSuccessReported) {
  // false means success.
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, ""));
  // OnFlowControl is always called immediately after connect to provide initial
  // quota to the renderer.
  EXPECT_CALL(*event_interface_, OnFlowControl(_));

  CreateChannelAndConnect();

  connect_data_.factory.connect_delegate->OnSuccess(stream_.Pass());
}

TEST_F(WebSocketChannelEventInterfaceTest, ConnectFailureReported) {
  // true means failure.
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(true, ""));

  CreateChannelAndConnect();

  connect_data_.factory.connect_delegate
      ->OnFailure(kWebSocketErrorNoStatusReceived);
}

TEST_F(WebSocketChannelEventInterfaceTest, ProtocolPassed) {
  EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, "Bob"));
  EXPECT_CALL(*event_interface_, OnFlowControl(_));

  CreateChannelAndConnect();

  connect_data_.factory.connect_delegate->OnSuccess(
      scoped_ptr<WebSocketStream>(new FakeWebSocketStream("Bob", "")));
}

// The first frames from the server can arrive together with the handshake, in
// which case they will be available as soon as ReadFrames() is called the first
// time.
TEST_F(WebSocketChannelEventInterfaceTest, DataLeftFromHandshake) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  static const InitFrameChunk chunks[] = {
    {{FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, 5},
     FINAL_CHUNK, "HELLO"},
  };
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, chunks);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrame(
            true, WebSocketFrameHeader::kOpCodeText, AsVector("HELLO")));
  }

  CreateChannelAndConnectSuccessfully();
}

// A remote server could accept the handshake, but then immediately send a
// Close frame.
TEST_F(WebSocketChannelEventInterfaceTest, CloseAfterHandshake) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  static const InitFrameChunk chunks[] = {
    {{FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, NOT_MASKED, 23},
     FINAL_CHUNK, "\x03\xf3Internal Server Error"},
  };
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, chunks);
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::SYNC,
                                 ERR_CONNECTION_CLOSED);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(*event_interface_, OnClosingHandshake());
    EXPECT_CALL(*event_interface_,
                OnDropChannel(kWebSocketErrorInternalServerError,
                              "Internal Server Error"));
  }

  CreateChannelAndConnectSuccessfully();
}

// A remote server could close the connection immediately after sending the
// handshake response (most likely a bug in the server).
TEST_F(WebSocketChannelEventInterfaceTest, ConnectionCloseAfterHandshake) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::SYNC,
                                 ERR_CONNECTION_CLOSED);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(kWebSocketErrorAbnormalClosure, _));
  }

  CreateChannelAndConnectSuccessfully();
}

TEST_F(WebSocketChannelEventInterfaceTest, NormalAsyncRead) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  static const InitFrameChunk chunks[] = {
    {{FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, 5},
     FINAL_CHUNK, "HELLO"},
  };
  // We use this checkpoint object to verify that the callback isn't called
  // until we expect it to be.
  MockFunction<void(int)> checkpoint;
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrame(
            true, WebSocketFrameHeader::kOpCodeText, AsVector("HELLO")));
    EXPECT_CALL(checkpoint, Call(2));
  }

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);
  base::MessageLoop::current()->RunUntilIdle();
  checkpoint.Call(2);
}

// Extra data can arrive while a read is being processed, resulting in the next
// read completing synchronously.
TEST_F(WebSocketChannelEventInterfaceTest, AsyncThenSyncRead) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  static const InitFrameChunk chunks1[] = {
    {{FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, 5},
     FINAL_CHUNK, "HELLO"},
  };
  static const InitFrameChunk chunks2[] = {
    {{FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, 5},
     FINAL_CHUNK, "WORLD"},
  };
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks1);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::SYNC, OK, chunks2);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrame(
            true, WebSocketFrameHeader::kOpCodeText, AsVector("HELLO")));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrame(
            true, WebSocketFrameHeader::kOpCodeText, AsVector("WORLD")));
  }

  CreateChannelAndConnectSuccessfully();
  base::MessageLoop::current()->RunUntilIdle();
}

// Data frames that arrive in fragments are turned into individual frames
TEST_F(WebSocketChannelEventInterfaceTest, FragmentedFrames) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  // Here we have one message split into 3 frames which arrive in 3 chunks. The
  // first frame is entirely in the first chunk, the second frame is split
  // across all the chunks, and the final frame is entirely in the final
  // chunk. The frame fragments are converted to separate frames so that they
  // can be delivered immediatedly. So the EventInterface should see a Text
  // message with 5 frames.
  static const InitFrameChunk chunks1[] = {
    {{NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, 5},
     FINAL_CHUNK, "THREE"},
    {{NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation, NOT_MASKED,
      7},
     NOT_FINAL_CHUNK, " "},
  };
  static const InitFrameChunk chunks2[] = {{{NO_HEADER}, NOT_FINAL_CHUNK,
                                            "SMALL"}};
  static const InitFrameChunk chunks3[] = {
    {{NO_HEADER}, FINAL_CHUNK, " "},
    {{FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation, NOT_MASKED, 6},
     FINAL_CHUNK, "FRAMES"},
  };
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks1);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks2);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks3);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrame(
            false, WebSocketFrameHeader::kOpCodeText, AsVector("THREE")));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrame(
            false, WebSocketFrameHeader::kOpCodeContinuation, AsVector(" ")));
    EXPECT_CALL(*event_interface_,
                OnDataFrame(false,
                            WebSocketFrameHeader::kOpCodeContinuation,
                            AsVector("SMALL")));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrame(
            false, WebSocketFrameHeader::kOpCodeContinuation, AsVector(" ")));
    EXPECT_CALL(*event_interface_,
                OnDataFrame(true,
                            WebSocketFrameHeader::kOpCodeContinuation,
                            AsVector("FRAMES")));
  }

  CreateChannelAndConnectSuccessfully();
  base::MessageLoop::current()->RunUntilIdle();
}

// In the case when a single-frame message because fragmented, it must be
// correctly transformed to multiple frames.
TEST_F(WebSocketChannelEventInterfaceTest, MessageFragmentation) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  // A single-frame Text message arrives in three chunks. This should be
  // delivered as three frames.
  static const InitFrameChunk chunks1[] = {
    {{FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, 12},
     NOT_FINAL_CHUNK, "TIME"},
  };
  static const InitFrameChunk chunks2[] = {{{NO_HEADER}, NOT_FINAL_CHUNK,
                                            " FOR "}};
  static const InitFrameChunk chunks3[] = {{{NO_HEADER}, FINAL_CHUNK, "TEA"}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks1);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks2);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks3);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrame(
            false, WebSocketFrameHeader::kOpCodeText, AsVector("TIME")));
    EXPECT_CALL(*event_interface_,
                OnDataFrame(false,
                            WebSocketFrameHeader::kOpCodeContinuation,
                            AsVector(" FOR ")));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrame(
            true, WebSocketFrameHeader::kOpCodeContinuation, AsVector("TEA")));
  }

  CreateChannelAndConnectSuccessfully();
  base::MessageLoop::current()->RunUntilIdle();
}

// If a control message is fragmented, it must be re-assembled before being
// delivered. A control message can only be fragmented at the network level; it
// is not permitted to be split into multiple frames.
TEST_F(WebSocketChannelEventInterfaceTest, FragmentedControlMessage) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  static const InitFrameChunk chunks1[] = {
    {{FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, NOT_MASKED, 7},
     NOT_FINAL_CHUNK, "\x03\xe8"},
  };
  static const InitFrameChunk chunks2[] = {{{NO_HEADER}, NOT_FINAL_CHUNK,
                                            "Clo"}};
  static const InitFrameChunk chunks3[] = {{{NO_HEADER}, FINAL_CHUNK, "se"}};
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks1);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks2);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks3);
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::ASYNC,
                                 ERR_CONNECTION_CLOSED);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(*event_interface_, OnClosingHandshake());
    EXPECT_CALL(*event_interface_,
                OnDropChannel(kWebSocketNormalClosure, "Close"));
  }

  CreateChannelAndConnectSuccessfully();
  base::MessageLoop::current()->RunUntilIdle();
}

// Connection closed by the remote host without a closing handshake.
TEST_F(WebSocketChannelEventInterfaceTest, AsyncAbnormalClosure) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::ASYNC,
                                 ERR_CONNECTION_CLOSED);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(kWebSocketErrorAbnormalClosure, _));
  }

  CreateChannelAndConnectSuccessfully();
  base::MessageLoop::current()->RunUntilIdle();
}

// A connection reset should produce the same event as an unexpected closure.
TEST_F(WebSocketChannelEventInterfaceTest, ConnectionReset) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::ASYNC,
                                 ERR_CONNECTION_RESET);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(kWebSocketErrorAbnormalClosure, _));
  }

  CreateChannelAndConnectSuccessfully();
  base::MessageLoop::current()->RunUntilIdle();
}

// Connection closed in the middle of a Close message (server bug, etc.)
TEST_F(WebSocketChannelEventInterfaceTest, ConnectionClosedInMessage) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  static const InitFrameChunk chunks[] = {
    {{FINAL_FRAME, WebSocketFrameHeader::kOpCodeClose, NOT_MASKED, 7},
     NOT_FINAL_CHUNK, "\x03\xe8"},
  };

  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks);
  stream->PrepareReadFramesError(ReadableFakeWebSocketStream::ASYNC,
                                 ERR_CONNECTION_CLOSED);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(kWebSocketErrorAbnormalClosure, _));
  }

  CreateChannelAndConnectSuccessfully();
  base::MessageLoop::current()->RunUntilIdle();
}

// RFC6455 5.1 "A client MUST close a connection if it detects a masked frame."
TEST_F(WebSocketChannelEventInterfaceTest, MaskedFramesAreRejected) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  static const InitFrameChunk chunks[] = {
    {{FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, MASKED, 5}, FINAL_CHUNK,
     "HELLO"}
  };

  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(kWebSocketErrorProtocolError, _));
  }

  CreateChannelAndConnectSuccessfully();
  base::MessageLoop::current()->RunUntilIdle();
}

// RFC6455 5.2 "If an unknown opcode is received, the receiving endpoint MUST
// _Fail the WebSocket Connection_."
TEST_F(WebSocketChannelEventInterfaceTest, UnknownOpCodeIsRejected) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  static const InitFrameChunk chunks[] = {{{FINAL_FRAME, 4, NOT_MASKED, 5},
                                           FINAL_CHUNK, "HELLO"}};

  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(kWebSocketErrorProtocolError, _));
  }

  CreateChannelAndConnectSuccessfully();
  base::MessageLoop::current()->RunUntilIdle();
}

// RFC6455 5.4 "Control frames ... MAY be injected in the middle of a
// fragmented message."
TEST_F(WebSocketChannelEventInterfaceTest, ControlFrameInDataMessage) {
  scoped_ptr<ReadableFakeWebSocketStream> stream(
      new ReadableFakeWebSocketStream);
  // We have one message of type Text split into two frames. In the middle is a
  // control message of type Pong.
  static const InitFrameChunk chunks1[] = {
    {{NOT_FINAL_FRAME, WebSocketFrameHeader::kOpCodeText, NOT_MASKED, 6},
     FINAL_CHUNK, "SPLIT "},
  };
  static const InitFrameChunk chunks2[] = {
    {{FINAL_FRAME, WebSocketFrameHeader::kOpCodePong, NOT_MASKED, 0},
     FINAL_CHUNK, ""}
  };
  static const InitFrameChunk chunks3[] = {
    {{FINAL_FRAME, WebSocketFrameHeader::kOpCodeContinuation, NOT_MASKED, 7},
     FINAL_CHUNK, "MESSAGE"}
  };
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks1);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks2);
  stream->PrepareReadFrames(ReadableFakeWebSocketStream::ASYNC, OK, chunks3);
  set_stream(stream.Pass());
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(
        *event_interface_,
        OnDataFrame(
            false, WebSocketFrameHeader::kOpCodeText, AsVector("SPLIT ")));
    EXPECT_CALL(*event_interface_,
                OnDataFrame(true,
                            WebSocketFrameHeader::kOpCodeContinuation,
                            AsVector("MESSAGE")));
  }

  CreateChannelAndConnectSuccessfully();
  base::MessageLoop::current()->RunUntilIdle();
}

// If the renderer sends lots of small writes, we don't want to update the quota
// for each one.
TEST_F(WebSocketChannelEventInterfaceTest, SmallWriteDoesntUpdateQuota) {
  set_stream(make_scoped_ptr(new WriteableFakeWebSocketStream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
  }

  CreateChannelAndConnectSuccessfully();
  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText, AsVector("B"));
}

// If we send enough to go below send_quota_low_water_mask_ we should get our
// quota refreshed.
TEST_F(WebSocketChannelEventInterfaceTest, LargeWriteUpdatesQuota) {
  set_stream(make_scoped_ptr(new WriteableFakeWebSocketStream));
  // We use this checkpoint object to verify that the quota update comes after
  // the write.
  MockFunction<void(int)> checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(checkpoint, Call(2));
  }

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);
  // TODO(ricea): If kDefaultSendQuotaHighWaterMark changes, then this value
  // will need to be updated.
  channel_->SendFrame(
      true, WebSocketFrameHeader::kOpCodeText, std::vector<char>(1 << 17, 'B'));
  checkpoint.Call(2);
}

// Verify that our quota actually is refreshed when we are told it is.
TEST_F(WebSocketChannelEventInterfaceTest, QuotaReallyIsRefreshed) {
  set_stream(make_scoped_ptr(new WriteableFakeWebSocketStream));
  MockFunction<void(int)> checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(checkpoint, Call(2));
    // If quota was not really refreshed, we would get an OnDropChannel()
    // message.
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(checkpoint, Call(3));
  }

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);
  // TODO(ricea): If kDefaultSendQuotaLowWaterMark and/or
  // kDefaultSendQuotaHighWaterMark change, then this value will need to be
  // updated.
  channel_->SendFrame(true,
                      WebSocketFrameHeader::kOpCodeText,
                      std::vector<char>((1 << 16) + 1, 'D'));
  checkpoint.Call(2);
  // We should have received more quota at this point.
  channel_->SendFrame(true,
                      WebSocketFrameHeader::kOpCodeText,
                      std::vector<char>((1 << 16) + 1, 'E'));
  checkpoint.Call(3);
}

// If we send more than the available quota then the connection will be closed
// with an error.
TEST_F(WebSocketChannelEventInterfaceTest, WriteOverQuotaIsRejected) {
  set_stream(make_scoped_ptr(new WriteableFakeWebSocketStream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    // TODO(ricea): Change this if kDefaultSendQuotaHighWaterMark changes.
    EXPECT_CALL(*event_interface_, OnFlowControl(1 << 17));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(kWebSocketMuxErrorSendQuotaViolation, _));
  }

  CreateChannelAndConnectSuccessfully();
  channel_->SendFrame(true,
                      WebSocketFrameHeader::kOpCodeText,
                      std::vector<char>((1 << 17) + 1, 'C'));
}

// If a write fails, the channel is dropped.
TEST_F(WebSocketChannelEventInterfaceTest, FailedWrite) {
  set_stream(make_scoped_ptr(new UnWriteableFakeWebSocketStream));
  MockFunction<void(int)> checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(kWebSocketErrorAbnormalClosure, _));
    EXPECT_CALL(checkpoint, Call(2));
  }

  CreateChannelAndConnectSuccessfully();
  checkpoint.Call(1);

  channel_->SendFrame(true, WebSocketFrameHeader::kOpCodeText, AsVector("H"));
  checkpoint.Call(2);
}

// OnDropChannel() is called exactly once when StartClosingHandshake() is used.
TEST_F(WebSocketChannelEventInterfaceTest, SendCloseDropsChannel) {
  set_stream(make_scoped_ptr(new EchoeyFakeWebSocketStream));
  {
    InSequence s;
    EXPECT_CALL(*event_interface_, OnAddChannelResponse(false, _));
    EXPECT_CALL(*event_interface_, OnFlowControl(_));
    EXPECT_CALL(*event_interface_,
                OnDropChannel(kWebSocketNormalClosure, "Fred"));
  }

  CreateChannelAndConnectSuccessfully();

  channel_->StartClosingHandshake(kWebSocketNormalClosure, "Fred");
  base::MessageLoop::current()->RunUntilIdle();
}

// RFC6455 5.1 "a client MUST mask all frames that it sends to the server".
// WebSocketChannel actually only sets the mask bit in the header, it doesn't
// perform masking itself (not all transports actually use masking).
TEST_F(WebSocketChannelStreamTest, SentFramesAreMasked) {
  EXPECT_CALL(*mock_stream_, GetSubProtocol()).Times(AnyNumber());
  EXPECT_CALL(*mock_stream_, ReadFrames(_, _)).WillOnce(Return(ERR_IO_PENDING));
  EXPECT_CALL(
      *mock_stream_,
      WriteFrames(Pointee(ElementsAre(Pointee(Field(
                      &WebSocketFrameChunk::header,
                      Pointee(Field(&WebSocketFrameHeader::masked, true)))))),
                  _)).WillOnce(Return(ERR_IO_PENDING));

  CreateChannelAndConnectSuccessfully();
  channel_->SendFrame(
      true, WebSocketFrameHeader::kOpCodeText, AsVector("NEEDS MASKING"));
}

}  // namespace
}  // namespace net
