// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WebSocket protocol implementation in chromium.
// It is intended to be used for live experiment of WebSocket connectivity
// metrics.
// Note that it is not used for WebKit's WebSocket communication.
// See third_party/WebKit/Source/WebCore/websockets/ instead.

#ifndef NET_WEBSOCKETS_WEBSOCKET_H_
#define NET_WEBSOCKETS_WEBSOCKET_H_
#pragma once

#include <deque>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/socket_stream/socket_stream.h"
#include "net/url_request/url_request_context.h"

class MessageLoop;

namespace net {

class ClientSocketFactory;
class HostResolver;

class WebSocket;
class WebSocketHandshake;

// Delegate methods will be called on the same message loop as
// WebSocket is constructed.
class WebSocketDelegate {
 public:
  virtual ~WebSocketDelegate() {}

  // Called when WebSocket connection has been established.
  virtual void OnOpen(WebSocket* socket) = 0;

  // Called when |msg| is received at |socket|.
  // |msg| should be in UTF-8.
  virtual void OnMessage(WebSocket* socket, const std::string& msg) = 0;

  // Called when WebSocket error has been detected.
  virtual void OnError(WebSocket* socket) {}

  // Called when |socket| is closed.
  virtual void OnClose(WebSocket* socket, bool was_clean) = 0;

  // Called when an error occured on |socket|.
  virtual void OnSocketError(const WebSocket* socket, int error) {}
};

class WebSocket : public base::RefCountedThreadSafe<WebSocket>,
                  public SocketStream::Delegate {
 public:
  enum State {
    INITIALIZED = -1,
    CONNECTING = 0,
    OPEN = 1,
    CLOSING = 2,
    CLOSED = 3,
  };
  enum ProtocolVersion {
    DEFAULT_VERSION = 0,
    DRAFT75 = 1,
  };
  class Request {
   public:
    Request(const GURL& url, const std::string protocol,
            const std::string origin, const std::string location,
            ProtocolVersion version,
            net::URLRequestContext* context)
        : url_(url),
          protocol_(protocol),
          origin_(origin),
          location_(location),
          version_(version),
          context_(context),
          host_resolver_(NULL),
          client_socket_factory_(NULL) {}
    ~Request() {}

    const GURL& url() const { return url_; }
    const std::string& protocol() const { return protocol_; }
    const std::string& origin() const { return origin_; }
    const std::string& location() const { return location_; }
    ProtocolVersion version() const { return version_; }
    net::URLRequestContext* context() const { return context_; }

    // Sets an alternative HostResolver. For testing purposes only.
    void SetHostResolver(HostResolver* host_resolver) {
      host_resolver_ = host_resolver;
    }
    HostResolver* host_resolver() const { return host_resolver_; }

    // Sets an alternative ClientSocketFactory.  Doesn't take ownership of
    // |factory|.  For testing purposes only.
    void SetClientSocketFactory(ClientSocketFactory* factory) {
      client_socket_factory_ = factory;
    }
    ClientSocketFactory* client_socket_factory() const {
      return client_socket_factory_;
    }

   private:
    GURL url_;
    std::string protocol_;
    std::string origin_;
    std::string location_;
    ProtocolVersion version_;
    scoped_refptr<net::URLRequestContext> context_;

    HostResolver* host_resolver_;
    ClientSocketFactory* client_socket_factory_;

    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  // Constructs new WebSocket.
  // It takes ownership of |req|.
  // |delegate| must be alive while this object is alive.
  WebSocket(Request* req, WebSocketDelegate* delegate);

  const Request* request() const { return request_.get(); }
  WebSocketDelegate* delegate() const { return delegate_; }

  State ready_state() const { return ready_state_; }

  // Connects new WebSocket.
  void Connect();

  // Sends |msg| on the WebSocket connection.
  // |msg| should be in UTF-8.
  void Send(const std::string& msg);

  // Closes the WebSocket connection.
  void Close();

  // Detach delegate.  Call before delegate is deleted.
  // Once delegate is detached, close the WebSocket connection and never call
  // delegate back.
  void DetachDelegate();

  // SocketStream::Delegate methods.
  // Called on IO thread.
  virtual void OnConnected(SocketStream* socket_stream,
                           int max_pending_send_allowed);
  virtual void OnSentData(SocketStream* socket_stream, int amount_sent);
  virtual void OnReceivedData(SocketStream* socket_stream,
                              const char* data, int len);
  virtual void OnClose(SocketStream* socket);
  virtual void OnError(const SocketStream* socket, int error);

 private:
  typedef std::deque< scoped_refptr<IOBufferWithSize> > PendingDataQueue;

  friend class WebSocketTest;

  friend class base::RefCountedThreadSafe<WebSocket>;
  virtual ~WebSocket();

  // Sends pending data in |current_write_buf_| and/or |pending_write_bufs_|.
  void SendPending();

  // Handles received data.
  void DoReceivedData();

  // Processes frame data in |current_read_buf_|.
  void ProcessFrameData();

  // Adds |len| bytes of |data| to |current_read_buf_|.
  void AddToReadBuffer(const char* data, int len);

  // Skips |len| bytes in |current_read_buf_|.
  void SkipReadBuffer(int len);

  void StartClosingHandshake();
  void DoForceCloseConnection();
  void FailConnection();
  // Handles closed connection.
  void DoClose();

  // Handles socket error report.
  void DoSocketError(int error);

  State ready_state_;
  scoped_ptr<Request> request_;
  scoped_ptr<WebSocketHandshake> handshake_;
  WebSocketDelegate* delegate_;
  MessageLoop* origin_loop_;

  scoped_refptr<SocketStream> socket_stream_;
  int max_pending_send_allowed_;

  // [0..offset) is received data from |socket_stream_|.
  // [0..read_consumed_len_) is already processed.
  // [read_consumed_len_..offset) is unprocessed data.
  // [offset..capacity) is free space.
  scoped_refptr<GrowableIOBuffer> current_read_buf_;
  int read_consumed_len_;

  // Drainable IOBuffer on the front of |pending_write_bufs_|.
  // [0..offset) is already sent to |socket_stream_|.
  // [offset..size) is being sent to |socket_stream_|, waiting OnSentData.
  scoped_refptr<DrainableIOBuffer> current_write_buf_;

  // Deque of IOBuffers in pending.
  // Front IOBuffer is being sent via |current_write_buf_|.
  PendingDataQueue pending_write_bufs_;

  // True when the 0xFF frame with length 0x00 is received.
  bool server_closing_handshake_;
  // True when trying to send 0xFF and 0x00 bytes.
  bool client_closing_handshake_;
  // True when send 0xFF and 0x00 bytes.
  bool closing_handshake_started_;
  // Task to close the connection after closing handshake has started and
  // |closing_handshake_timeout_|.
  CancelableTask* force_close_task_;
  int64 closing_handshake_timeout_;

  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_H_
