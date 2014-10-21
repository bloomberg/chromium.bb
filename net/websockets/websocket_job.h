// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_JOB_H_
#define NET_WEBSOCKETS_WEBSOCKET_JOB_H_

#include <deque>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/socket_stream/socket_stream_job.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_websocket_stream.h"

class GURL;

namespace net {

class DrainableIOBuffer;
class SSLInfo;
class WebSocketHandshakeRequestHandler;
class WebSocketHandshakeResponseHandler;

// WebSocket protocol specific job on SocketStream.
// It captures WebSocket handshake message and handles cookie operations.
// Chrome security policy doesn't allow renderer process (except dev tools)
// see HttpOnly cookies, so it injects cookie header in handshake request and
// strips set-cookie headers in handshake response.
// TODO(ukai): refactor websocket.cc to use this.
class NET_EXPORT WebSocketJob
    : public SocketStreamJob,
      public SocketStream::Delegate,
      public SpdyWebSocketStream::Delegate {
 public:
  // This is state of WebSocket, not SocketStream.
  enum State {
    INITIALIZED = -1,
    CONNECTING = 0,
    OPEN = 1,
    CLOSING = 2,
    CLOSED = 3,
  };

  explicit WebSocketJob(SocketStream::Delegate* delegate);

  static void EnsureInit();

  State state() const { return state_; }
  void Connect() override;
  bool SendData(const char* data, int len) override;
  void Close() override;
  void RestartWithAuth(const AuthCredentials& credentials) override;
  void DetachDelegate() override;

  // SocketStream::Delegate methods.
  int OnStartOpenConnection(SocketStream* socket,
                            const CompletionCallback& callback) override;
  void OnConnected(SocketStream* socket, int max_pending_send_allowed) override;
  void OnSentData(SocketStream* socket, int amount_sent) override;
  void OnReceivedData(SocketStream* socket, const char* data, int len) override;
  void OnClose(SocketStream* socket) override;
  void OnAuthRequired(SocketStream* socket,
                      AuthChallengeInfo* auth_info) override;
  void OnSSLCertificateError(SocketStream* socket,
                             const SSLInfo& ssl_info,
                             bool fatal) override;
  void OnError(const SocketStream* socket, int error) override;

  // SpdyWebSocketStream::Delegate methods.
  void OnCreatedSpdyStream(int status) override;
  void OnSentSpdyHeaders() override;
  void OnSpdyResponseHeadersUpdated(
      const SpdyHeaderBlock& response_headers) override;
  void OnSentSpdyData(size_t bytes_sent) override;
  void OnReceivedSpdyData(scoped_ptr<SpdyBuffer> buffer) override;
  void OnCloseSpdyStream() override;

 private:
  friend class WebSocketThrottle;
  friend class WebSocketJobTest;
  ~WebSocketJob() override;

  bool SendHandshakeRequest(const char* data, int len);
  void AddCookieHeaderAndSend();
  void LoadCookieCallback(const std::string& cookie);

  void OnSentHandshakeRequest(SocketStream* socket, int amount_sent);
  // Parses received data into handshake_response_. When finished receiving the
  // response, calls SaveCookiesAndNotifyHeadersComplete().
  void OnReceivedHandshakeResponse(
      SocketStream* socket, const char* data, int len);
  // Saves received cookies to the cookie store, and then notifies the
  // delegate_ of completion of handshake.
  void SaveCookiesAndNotifyHeadersComplete();
  void SaveNextCookie();
  void OnCookieSaved(bool cookie_status);
  // Clears variables for handling cookies, rebuilds handshake string excluding
  // cookies, and then pass the handshake string to delegate_.
  void NotifyHeadersComplete();
  void DoSendData();

  GURL GetURLForCookies() const;

  const AddressList& address_list() const;
  int TrySpdyStream();
  void SetWaiting();
  bool IsWaiting() const;
  void Wakeup();
  void RetryPendingIO();
  void CompleteIO(int result);

  bool SendDataInternal(const char* data, int length);
  void CloseInternal();
  void SendPending();

  SocketStream::Delegate* delegate_;
  State state_;
  bool waiting_;
  AddressList addresses_;
  CompletionCallback callback_;  // for throttling.

  scoped_ptr<WebSocketHandshakeRequestHandler> handshake_request_;
  scoped_ptr<WebSocketHandshakeResponseHandler> handshake_response_;

  bool started_to_send_handshake_request_;
  size_t handshake_request_sent_;

  std::vector<std::string> response_cookies_;
  size_t response_cookies_save_index_;

  std::deque<scoped_refptr<IOBufferWithSize> > send_buffer_queue_;
  scoped_refptr<DrainableIOBuffer> current_send_buffer_;
  std::vector<char> received_data_after_handshake_;

  int spdy_protocol_version_;
  scoped_ptr<SpdyWebSocketStream> spdy_websocket_stream_;
  std::string challenge_;

  bool save_next_cookie_running_;
  bool callback_pending_;

  base::WeakPtrFactory<WebSocketJob> weak_ptr_factory_;
  base::WeakPtrFactory<WebSocketJob> weak_ptr_factory_for_send_pending_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketJob);
};

}  // namespace

#endif  // NET_WEBSOCKETS_WEBSOCKET_JOB_H_
