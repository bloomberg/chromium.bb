// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBSOCKET_CONNECTION_H_
#define REMOTING_HOST_WEBSOCKET_CONNECTION_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/socket_reader.h"
#include "remoting/protocol/buffered_socket_writer.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace remoting {

class WebSocketConnection {
 public:
  typedef base::Callback<void (bool connected)> ConnectedCallback;
  class Delegate {
   public:
    virtual void OnWebSocketMessage(const std::string& message) = 0;
    virtual void OnWebSocketClosed() = 0;
  };

  WebSocketConnection();
  virtual ~WebSocketConnection();

  // Initialize WebSocket connection for the specified |socket|.
  // |connected_callback| is called with |connected|=true after handshake
  // headers have been received from the client and parsed or with
  // |connected|=false if the connection failed (e.g. headers were invalid).
  void Start(scoped_ptr<net::StreamSocket> socket,
             ConnectedCallback connected_callback);

  // Connection parameters received in the header. Valid only once Start() has
  // successfully completed.
  const std::string& request_path() { return request_path_; }
  const std::string& request_host() { return request_host_; }
  const std::string& origin() { return origin_; }

  // Accept or Reject new connection. Accept() or Reject() must be called once
  // after Start() completes successfully.
  void Accept(Delegate* delegate);
  void Reject();

  // Sets the maximum incoming message size to allow.  The connection will be
  // closed if a message exceeding this size is received. |size| may be 0 to
  // allow any message size. By default size of incoming messages is unlimited.
  void set_maximum_message_size(uint64 size);

  // Send the specified text message. Sending data messages is not supported.
  void SendText(const std::string& text);

  // Closes the connection and sends Close frame to the client if necessary.
  void Close();

 private:
  enum State {
    READING_HEADERS,
    HEADERS_READ,
    ACCEPTED,
    CLOSED,
  };

  enum WebsocketOpcode {
    OPCODE_CONTINUATION = 0,
    OPCODE_TEXT_FRAME = 1,
    OPCODE_BINARY_FRAME = 2,
    OPCODE_CLOSE = 8,
    OPCODE_PING = 9,
    OPCODE_PONG = 10,
  };

  // Closes the socket in response to a protocol error, and notifies
  // Delegate::OnWebSocketClosed().
  void CloseOnError();

  // Result handler for |reader_|.
  void OnSocketReadResult(scoped_refptr<net::IOBuffer> data, int size);

  // Parses websocket headers in |header_| and returns false when headers are
  // invalid.
  bool ParseHeaders();

  // Parses incoming data in |received_data_| and dispatches delegate calls when
  // a new message is received.
  void ProcessData();

  // Error handler for |writer_|.
  void OnSocketWriteError(int error);

  // Sends outgoing frame with the specified |opcode| and |payload|.
  void SendFragment(WebsocketOpcode opcode, const std::string& payload);

  // Unmasks fragment |payload| using specified |mask|
  void UnmaskPayload(const char* mask, char* payload, int payload_length);

  scoped_ptr<net::StreamSocket> socket_;
  ConnectedCallback connected_callback_;
  Delegate* delegate_;

  uint64 maximum_message_size_;

  SocketReader reader_;
  protocol::BufferedSocketWriter writer_;

  State state_;

  std::string headers_;

  // Header fields. Set in ParseHeaders().
  std::string request_path_;
  std::string request_host_;
  std::string origin_;
  std::string websocket_key_;

  // Raw data that has been received but hasn't been parsed.
  std::string received_data_;

  // When receiving a fragmented message |receiving_message_| is set to true and
  // |current_message_| contains the fragments that we've already received.
  bool receiving_message_;
  std::string current_message_;

  base::WeakPtrFactory<WebSocketConnection> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketConnection);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WEBSOCKET_CONNECTION_H_
