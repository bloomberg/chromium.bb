// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_CHANNEL_H_
#define NET_WEBSOCKETS_WEBSOCKET_CHANNEL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "net/base/net_export.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_stream.h"
#include "url/gurl.h"

namespace net {

class URLRequestContext;
class WebSocketEventInterface;

// Transport-independent implementation of WebSockets. Implements protocol
// semantics that do not depend on the underlying transport. Provides the
// interface to the content layer. Some WebSocket concepts are used here without
// definition; please see the RFC at http://tools.ietf.org/html/rfc6455 for
// clarification.
class NET_EXPORT WebSocketChannel {
 public:
  // The type of a WebSocketStream factory callback. Must match the signature of
  // WebSocketStream::CreateAndConnectStream().
  typedef base::Callback<scoped_ptr<WebSocketStreamRequest>(
      const GURL&,
      const std::vector<std::string>&,
      const GURL&,
      URLRequestContext*,
      const BoundNetLog&,
      scoped_ptr<WebSocketStream::ConnectDelegate>)> WebSocketStreamFactory;

  // Creates a new WebSocketChannel with the specified parameters.
  // SendAddChannelRequest() must be called immediately afterwards to start the
  // connection process.
  WebSocketChannel(const GURL& socket_url,
                   scoped_ptr<WebSocketEventInterface> event_interface);
  virtual ~WebSocketChannel();

  // Starts the connection process.
  void SendAddChannelRequest(
      const std::vector<std::string>& requested_protocols,
      const GURL& origin,
      URLRequestContext* url_request_context);

  // Sends a data frame to the remote side. The frame should usually be no
  // larger than 32KB to prevent the time required to copy the buffers from from
  // unduly delaying other tasks that need to run on the IO thread. This method
  // has a hard limit of 2GB. It is the responsibility of the caller to ensure
  // that they have sufficient send quota to send this data, otherwise the
  // connection will be closed without sending. |fin| indicates the last frame
  // in a message, equivalent to "FIN" as specified in section 5.2 of
  // RFC6455. |data| is the "Payload Data". If |op_code| is kOpCodeText, or it
  // is kOpCodeContinuation and the type the message is Text, then |data| must
  // be a chunk of a valid UTF-8 message, however there is no requirement for
  // |data| to be split on character boundaries.
  void SendFrame(bool fin,
                 WebSocketFrameHeader::OpCode op_code,
                 const std::vector<char>& data);

  // Sends |quota| units of flow control to the remote side. If the underlying
  // transport has a concept of |quota|, then it permits the remote server to
  // send up to |quota| units of data.
  void SendFlowControl(int64 quota);

  // Start the closing handshake for a client-initiated shutdown of the
  // connection. There is no API to close the connection without a closing
  // handshake, but destroying the WebSocketChannel object while connected will
  // effectively do that. |code| must be in the range 1000-4999. |reason| should
  // be a valid UTF-8 string or empty.
  //
  // This does *not* trigger the event OnClosingHandshake(). The caller should
  // assume that the closing handshake has started and perform the equivalent
  // processing to OnClosingHandshake() if necessary.
  void StartClosingHandshake(uint16 code, const std::string& reason);

  // Starts the connection process, using a specified factory function rather
  // than the default. This is exposed for testing.
  void SendAddChannelRequestForTesting(
      const std::vector<std::string>& requested_protocols,
      const GURL& origin,
      URLRequestContext* url_request_context,
      const WebSocketStreamFactory& factory);

 private:
  // We have a simple linear progression of states from FRESHLY_CONSTRUCTED to
  // CLOSED, except that the SEND_CLOSED and RECV_CLOSED states may be skipped
  // in case of error.
  enum State {
    FRESHLY_CONSTRUCTED,
    CONNECTING,
    CONNECTED,
    SEND_CLOSED,  // We have sent a Close frame but not received a Close frame.
    RECV_CLOSED,  // Used briefly between receiving a Close frame and sending
                  // the response. Once we have responded, the state changes
                  // to CLOSED.
    CLOSE_WAIT,   // The Closing Handshake has completed, but the remote server
                  // has not yet closed the connection.
    CLOSED,       // The Closing Handshake has completed and the connection
                  // has been closed; or the connection is failed.
  };

  // When failing a channel, we may or may not want to send the real reason for
  // failing to the remote server. This enum is used by FailChannel() to
  // choose.
  enum ExposeError {
    SEND_REAL_ERROR,
    SEND_GOING_AWAY,
  };

  // Our implementation of WebSocketStream::ConnectDelegate. We do not inherit
  // from WebSocketStream::ConnectDelegate directly to avoid cluttering our
  // public interface with the implementation of those methods, and because the
  // lifetime of a WebSocketChannel is longer than the lifetime of the
  // connection process.
  class ConnectDelegate;

  // Starts the connection progress, using a specified factory function.
  void SendAddChannelRequestWithFactory(
      const std::vector<std::string>& requested_protocols,
      const GURL& origin,
      URLRequestContext* url_request_context,
      const WebSocketStreamFactory& factory);

  // Success callback from WebSocketStream::CreateAndConnectStream(). Reports
  // success to the event interface.
  void OnConnectSuccess(scoped_ptr<WebSocketStream> stream);

  // Failure callback from WebSocketStream::CreateAndConnectStream(). Reports
  // failure to the event interface.
  void OnConnectFailure(uint16 websocket_error);

  // Returns true if state_ is SEND_CLOSED, CLOSE_WAIT or CLOSED.
  bool InClosingState() const;

  // Calls WebSocketStream::WriteFrames() with the appropriate arguments
  void WriteFrames();

  // Callback from WebSocketStream::WriteFrames. Sends pending data or adjusts
  // the send quota of the renderer channel as appropriate. |result| is a net
  // error code, usually OK. If |synchronous| is true, then OnWriteDone() is
  // being called from within the WriteFrames() loop and does not need to call
  // WriteFrames() itself.
  void OnWriteDone(bool synchronous, int result);

  // Calls WebSocketStream::ReadFrames() with the appropriate arguments.
  void ReadFrames();

  // Callback from WebSocketStream::ReadFrames. Handles any errors and processes
  // the returned chunks appropriately to their type. |result| is a net error
  // code. If |synchronous| is true, then OnReadDone() is being called from
  // within the ReadFrames() loop and does not need to call ReadFrames() itself.
  void OnReadDone(bool synchronous, int result);

  // Processes a single chunk that has been read from the stream.
  void ProcessFrameChunk(scoped_ptr<WebSocketFrameChunk> chunk);

  // Handle a frame that we have received enough of to process. May call
  // event_interface_ methods, send responses to the server, and change the
  // value of state_.
  void HandleFrame(const WebSocketFrameHeader::OpCode opcode,
                   bool is_first_chunk,
                   bool is_final_chunk,
                   const scoped_refptr<IOBufferWithSize>& data_buffer);

  // Low-level method to send a single frame. Used for both data and control
  // frames. Either sends the frame immediately or buffers it to be scheduled
  // when the current write finishes. |fin| and |op_code| are defined as for
  // SendFrame() above, except that |op_code| may also be a control frame
  // opcode.
  void SendIOBufferWithSize(bool fin,
                            WebSocketFrameHeader::OpCode op_code,
                            const scoped_refptr<IOBufferWithSize>& buffer);

  // Perform the "Fail the WebSocket Connection" operation as defined in
  // RFC6455. The supplied code and reason are sent back to the renderer in an
  // OnDropChannel message. If state_ is CONNECTED then a Close message is sent
  // to the remote host. If |expose| is SEND_REAL_ERROR then the remote host is
  // given the same status code we gave the renderer; otherwise it is sent a
  // fixed "Going Away" code.  Resets current_frame_header_, closes the
  // stream_, and sets state_ to CLOSED.
  void FailChannel(ExposeError expose, uint16 code, const std::string& reason);

  // Sends a Close frame to Start the WebSocket Closing Handshake, or to respond
  // to a Close frame from the server. As a special case, setting |code| to
  // kWebSocketErrorNoStatusReceived will create a Close frame with no payload;
  // this is symmetric with the behaviour of ParseClose.
  void SendClose(uint16 code, const std::string& reason);

  // Parses a Close frame. If no status code is supplied, then |code| is set to
  // 1005 (No status code) with empty |reason|. If the supplied code is
  // outside the valid range, then 1002 (Protocol error) is set instead. If the
  // reason text is not valid UTF-8, then |reason| is set to an empty string
  // instead.
  void ParseClose(const scoped_refptr<IOBufferWithSize>& buffer,
                  uint16* code,
                  std::string* reason);

  // The URL to which we connect.
  const GURL socket_url_;

  // The object receiving events.
  const scoped_ptr<WebSocketEventInterface> event_interface_;

  // The WebSocketStream to which we are sending/receiving data.
  scoped_ptr<WebSocketStream> stream_;

  // A data structure containing a vector of frames to be sent and the total
  // number of bytes contained in the vector.
  class SendBuffer;
  // Data that is currently pending write, or NULL if no write is pending.
  scoped_ptr<SendBuffer> data_being_sent_;
  // Data that is queued up to write after the current write completes.
  // Only non-NULL when such data actually exists.
  scoped_ptr<SendBuffer> data_to_send_next_;

  // Destination for the current call to WebSocketStream::ReadFrames
  ScopedVector<WebSocketFrameChunk> read_frame_chunks_;
  // Frame header for the frame currently being received. Only non-NULL while we
  // are processing the frame. If the frame arrives in multiple chunks, can
  // remain non-NULL while we wait for additional chunks to arrive. If the
  // header of the frame was invalid, this is set to NULL, the channel is
  // failed, and subsequent chunks of the same frame will be ignored.
  scoped_ptr<WebSocketFrameHeader> current_frame_header_;
  // Handle to an in-progress WebSocketStream creation request. Only non-NULL
  // during the connection process.
  scoped_ptr<WebSocketStreamRequest> stream_request_;
  // Although it will almost never happen in practice, we can be passed an
  // incomplete control frame, in which case we need to keep the data around
  // long enough to reassemble it. This variable will be NULL the rest of the
  // time.
  scoped_refptr<IOBufferWithSize> incomplete_control_frame_body_;
  // The point at which we give the renderer a quota refresh (quota units).
  // "quota units" are currently bytes. TODO(ricea): Update the definition of
  // quota units when necessary.
  int send_quota_low_water_mark_;
  // The amount which we refresh the quota to when it reaches the
  // low_water_mark (quota units).
  int send_quota_high_water_mark_;
  // The current amount of quota that the renderer has available for sending
  // on this logical channel (quota units).
  int current_send_quota_;

  // Storage for the status code and reason from the time we receive the Close
  // frame until the connection is closed and we can call OnDropChannel().
  uint16 closing_code_;
  std::string closing_reason_;

  // The current state of the channel. Mainly used for sanity checking, but also
  // used to track the close state.
  State state_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketChannel);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_CHANNEL_H_
