// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_STREAM_H_
#define NET_WEBSOCKETS_WEBSOCKET_STREAM_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/websockets/websocket_stream_base.h"

class GURL;

namespace net {

class BoundNetLog;
class HttpRequestHeaders;
class HttpResponseInfo;
class URLRequestContext;
struct WebSocketFrameChunk;

// WebSocketStreamRequest is the caller's handle to the process of creation of a
// WebSocketStream. Deleting the object before the OnSuccess or OnFailure
// callbacks are called will cancel the request (and neither callback will be
// called). After OnSuccess or OnFailure have been called, this object may be
// safely deleted without side-effects.
class NET_EXPORT_PRIVATE WebSocketStreamRequest {
 public:
  virtual ~WebSocketStreamRequest();
};

// WebSocketStream is a transport-agnostic interface for reading and writing
// WebSocket frames. This class provides an abstraction for WebSocket streams
// based on various transport layers, such as normal WebSocket connections
// (WebSocket protocol upgraded from HTTP handshake), SPDY transports, or
// WebSocket connections with multiplexing extension. Subtypes of
// WebSocketStream are responsible for managing the underlying transport
// appropriately.
//
// All functions except Close() can be asynchronous. If an operation cannot
// be finished synchronously, the function returns ERR_IO_PENDING, and
// |callback| will be called when the operation is finished. Non-null |callback|
// must be provided to these functions.

class NET_EXPORT_PRIVATE WebSocketStream : public WebSocketStreamBase {
 public:
  // A concrete object derived from ConnectDelegate is supplied by the caller to
  // CreateAndConnectStream() to receive the result of the connection.
  class ConnectDelegate {
   public:
    virtual ~ConnectDelegate();
    // Called on successful connection. The parameter is an object derived from
    // WebSocketStream.
    virtual void OnSuccess(scoped_ptr<WebSocketStream> stream) = 0;

    // Called on failure to connect. The parameter is either one of the values
    // defined in net::WebSocketError, or an error defined by some WebSocket
    // extension protocol that we implement.
    virtual void OnFailure(unsigned short websocket_error) = 0;
  };

  // Create and connect a WebSocketStream of an appropriate type. The actual
  // concrete type returned depends on whether multiplexing or SPDY are being
  // used to communicate with the remote server. If the handshake completed
  // successfully, then connect_delegate->OnSuccess() is called with a
  // WebSocketStream instance. If it failed, then connect_delegate->OnFailure()
  // is called with a WebSocket result code corresponding to the error. Deleting
  // the returned WebSocketStreamRequest object will cancel the connection, in
  // which case the |connect_delegate| object that the caller passed will be
  // deleted without any of its methods being called. Unless cancellation is
  // required, the caller should keep the WebSocketStreamRequest object alive
  // until connect_delegate->OnSuccess() or OnFailure() have been called, then
  // it is safe to delete.
  static scoped_ptr<WebSocketStreamRequest> CreateAndConnectStream(
      const GURL& socket_url,
      const std::vector<std::string>& requested_subprotocols,
      const GURL& origin,
      URLRequestContext* url_request_context,
      const BoundNetLog& net_log,
      scoped_ptr<ConnectDelegate> connect_delegate);

  // Derived classes must make sure Close() is called when the stream is not
  // closed on destruction.
  virtual ~WebSocketStream();

  // Reads WebSocket frame data. This operation finishes when new frame data
  // becomes available. Each frame message might be chopped off in the middle
  // as specified in the description of WebSocketFrameChunk struct.
  // |frame_chunks| remains owned by the caller and must be valid until the
  // operation completes or Close() is called. |frame_chunks| must be empty on
  // calling.
  //
  // This function should not be called while the previous call of ReadFrames()
  // is still pending.
  //
  // Returns net::OK or one of the net::ERR_* codes.
  //
  // frame_chunks->size() >= 1 if the result is OK.
  //
  // A frame with an incomplete header will never be inserted into
  // |frame_chunks|. If the currently available bytes of a new frame do not form
  // a complete frame header, then the implementation will buffer them until all
  // the fields in the WebSocketFrameHeader object can be filled. If
  // ReadFrames() is freshly called in this situation, it will return
  // ERR_IO_PENDING exactly as if no data was available.
  //
  // Every WebSocketFrameChunk in the vector except the first and last is
  // guaranteed to be a complete frame. The first chunk may be the final part
  // of the previous frame. The last chunk may be the first part of a new
  // frame. If there is only one chunk, then it may consist of data from the
  // middle part of a frame.
  //
  // When the socket is closed on the remote side, this method will return
  // ERR_CONNECTION_CLOSED. It will not return OK with an empty vector.
  //
  // If the connection is closed in the middle of receiving an incomplete frame,
  // ReadFrames may discard the incomplete frame. Since the renderer will
  // discard any incomplete messages when the connection is closed, this makes
  // no difference to the overall semantics.
  virtual int ReadFrames(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                         const CompletionCallback& callback) = 0;

  // Writes WebSocket frame data. |frame_chunks| must obey the rule specified
  // in the documentation of WebSocketFrameChunk struct: the first chunk of
  // a WebSocket frame must contain non-NULL |header|, and the last chunk must
  // have |final_chunk| field set to true. Series of chunks representing a
  // WebSocket frame must be consistent (the total length of |data| fields must
  // match |header->payload_length|). |frame_chunks| must be valid until the
  // operation completes or Close() is called.
  //
  // This function should not be called while previous call of WriteFrames() is
  // still pending.
  //
  // Support for incomplete frames is not guaranteed and may be removed from
  // future iterations of the API.
  //
  // This method will only return OK if all frames were written completely.
  // Otherwise it will return an appropriate net error code.
  virtual int WriteFrames(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                          const CompletionCallback& callback) = 0;

  // Closes the stream. All pending I/O operations (if any) are cancelled
  // at this point, so |frame_chunks| can be freed.
  virtual void Close() = 0;

  // The subprotocol that was negotiated for the stream. If no protocol was
  // negotiated, then the empty string is returned.
  virtual std::string GetSubProtocol() const = 0;

  // The extensions that were negotiated for the stream. Since WebSocketStreams
  // can be layered, this may be different from what this particular
  // WebSocketStream implements. The primary purpose of this accessor is to make
  // the data available to Javascript. The format of the string is identical to
  // the contents of the Sec-WebSocket-Extensions header supplied by the server,
  // with some canonicalisations applied (leading and trailing whitespace
  // removed, multiple headers concatenated into one comma-separated list). See
  // RFC6455 section 9.1 for the exact format specification. If no
  // extensions were negotiated, the empty string is returned.
  virtual std::string GetExtensions() const = 0;

  // TODO(yutak): Add following interfaces:
  // - RenewStreamForAuth for authentication (is this necessary?)
  // - GetSSLInfo, GetSSLCertRequestInfo for SSL

  // WebSocketStreamBase derived functions
  virtual WebSocketStream* AsWebSocketStream() OVERRIDE;

  ////////////////////////////////////////////////////////////////////////////
  // Methods used during the stream handshake. These must not be called once a
  // WebSocket protocol stream has been established (ie. after the
  // SuccessCallback or FailureCallback has been called.)

  // Writes WebSocket handshake request to the underlying socket. Must be called
  // before ReadHandshakeResponse().
  //
  // |callback| will only be called if this method returns ERR_IO_PENDING.
  //
  // |response_info| must remain valid until the callback from
  // ReadHandshakeResponse has been called.
  //
  // TODO(ricea): This function is only used during the handshake and is
  // probably only applicable to certain subclasses of WebSocketStream. Move it
  // somewhere else? Also applies to ReadHandshakeResponse.
  virtual int SendHandshakeRequest(const GURL& url,
                                   const HttpRequestHeaders& headers,
                                   HttpResponseInfo* response_info,
                                   const CompletionCallback& callback) = 0;

  // Reads WebSocket handshake response from the underlying socket. Must be
  // called after SendHandshakeRequest() completes.
  //
  // |callback| will only be called if this method returns ERR_IO_PENDING.
  virtual int ReadHandshakeResponse(const CompletionCallback& callback) = 0;

 protected:
  WebSocketStream();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocketStream);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_STREAM_H_
