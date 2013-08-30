// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_H_
#define NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "net/websockets/websocket_frame_parser.h"
#include "net/websockets/websocket_stream.h"

namespace net {

class ClientSocketHandle;
class DrainableIOBuffer;
class GrowableIOBuffer;
class HttpRequestHeaders;
class HttpResponseInfo;
class IOBufferWithSize;
struct WebSocketFrameChunk;

// Implementation of WebSocketStream for non-multiplexed ws:// connections (or
// the physical side of a multiplexed ws:// connection).
class NET_EXPORT_PRIVATE WebSocketBasicStream : public WebSocketStream {
 public:
  typedef WebSocketMaskingKey (*WebSocketMaskingKeyGeneratorFunction)();

  // This class should not normally be constructed directly; see
  // WebSocketStream::CreateAndConnectStream.
  explicit WebSocketBasicStream(scoped_ptr<ClientSocketHandle> connection);

  // The destructor has to make sure the connection is closed when we finish so
  // that it does not get returned to the pool.
  virtual ~WebSocketBasicStream();

  // WebSocketStream implementation.
  virtual int ReadFrames(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                         const CompletionCallback& callback) OVERRIDE;

  virtual int WriteFrames(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                          const CompletionCallback& callback) OVERRIDE;

  virtual void Close() OVERRIDE;

  virtual std::string GetSubProtocol() const OVERRIDE;

  virtual std::string GetExtensions() const OVERRIDE;

  // Writes WebSocket handshake request HTTP-style to the connection. Adds
  // "Sec-WebSocket-Key" header; this should not be included in |headers|.
  virtual int SendHandshakeRequest(const GURL& url,
                                   const HttpRequestHeaders& headers,
                                   HttpResponseInfo* response_info,
                                   const CompletionCallback& callback) OVERRIDE;

  virtual int ReadHandshakeResponse(
      const CompletionCallback& callback) OVERRIDE;

  ////////////////////////////////////////////////////////////////////////////
  // Methods for testing only.

  static scoped_ptr<WebSocketBasicStream> CreateWebSocketBasicStreamForTesting(
      scoped_ptr<ClientSocketHandle> connection,
      const scoped_refptr<GrowableIOBuffer>& http_read_buffer,
      const std::string& sub_protocol,
      const std::string& extensions,
      WebSocketMaskingKeyGeneratorFunction key_generator_function);

 private:
  // Returns OK or calls |callback| when the |buffer| is fully drained or
  // something has failed.
  int WriteEverything(const scoped_refptr<DrainableIOBuffer>& buffer,
                      const CompletionCallback& callback);

  // Wraps the |callback| to continue writing until everything has been written.
  void OnWriteComplete(const scoped_refptr<DrainableIOBuffer>& buffer,
                       const CompletionCallback& callback,
                       int result);

  // Attempts to parse the output of a read as WebSocket frames. On success,
  // returns OK and places the frame(s) in frame_chunks.
  int HandleReadResult(int result,
                       ScopedVector<WebSocketFrameChunk>* frame_chunks);

  // Called when a read completes. Parses the result and (unless no complete
  // header has been received) calls |callback|.
  void OnReadComplete(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                      const CompletionCallback& callback,
                      int result);

  // Storage for pending reads. All active WebSockets spend all the time with a
  // call to ReadFrames() pending, so there is no benefit in trying to share
  // this between sockets.
  scoped_refptr<IOBufferWithSize> read_buffer_;

  // The connection, wrapped in a ClientSocketHandle so that we can prevent it
  // from being returned to the pool.
  scoped_ptr<ClientSocketHandle> connection_;

  // Only used during handshake. Some data may be left in this buffer after the
  // handshake, in which case it will be picked up during the first call to
  // ReadFrames(). The type is GrowableIOBuffer for compatibility with
  // net::HttpStreamParser, which is used to parse the handshake.
  scoped_refptr<GrowableIOBuffer> http_read_buffer_;

  // This keeps the current parse state (including any incomplete headers) and
  // parses frames.
  WebSocketFrameParser parser_;

  // The negotated sub-protocol, or empty for none.
  std::string sub_protocol_;

  // The extensions negotiated with the remote server.
  std::string extensions_;

  // This can be overridden in tests to make the output deterministic. We don't
  // use a Callback here because a function pointer is faster and good enough
  // for our purposes.
  WebSocketMaskingKeyGeneratorFunction generate_websocket_masking_key_;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_H_
