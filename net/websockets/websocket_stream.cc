// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_stream.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_handshake_constants.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "net/websockets/websocket_handshake_stream_create_helper.h"
#include "net/websockets/websocket_test_util.h"
#include "url/gurl.h"

namespace net {
namespace {

class StreamRequestImpl;

class Delegate : public URLRequest::Delegate {
 public:
  explicit Delegate(StreamRequestImpl* owner) : owner_(owner) {}
  virtual ~Delegate() {}

  // Implementation of URLRequest::Delegate methods.
  virtual void OnResponseStarted(URLRequest* request) OVERRIDE;

  virtual void OnAuthRequired(URLRequest* request,
                              AuthChallengeInfo* auth_info) OVERRIDE;

  virtual void OnCertificateRequested(URLRequest* request,
                                      SSLCertRequestInfo* cert_request_info)
      OVERRIDE;

  virtual void OnSSLCertificateError(URLRequest* request,
                                     const SSLInfo& ssl_info,
                                     bool fatal) OVERRIDE;

  virtual void OnReadCompleted(URLRequest* request, int bytes_read) OVERRIDE;

 private:
  StreamRequestImpl* owner_;
};

class StreamRequestImpl : public WebSocketStreamRequest {
 public:
  StreamRequestImpl(
      const GURL& url,
      const URLRequestContext* context,
      scoped_ptr<WebSocketStream::ConnectDelegate> connect_delegate,
      WebSocketHandshakeStreamCreateHelper* create_helper)
      : delegate_(new Delegate(this)),
        url_request_(url, DEFAULT_PRIORITY, delegate_.get(), context),
        connect_delegate_(connect_delegate.Pass()),
        create_helper_(create_helper) {}

  // Destroying this object destroys the URLRequest, which cancels the request
  // and so terminates the handshake if it is incomplete.
  virtual ~StreamRequestImpl() {}

  URLRequest* url_request() { return &url_request_; }

  void PerformUpgrade() {
    connect_delegate_->OnSuccess(create_helper_->stream()->Upgrade());
  }

  void ReportFailure() {
    connect_delegate_->OnFailure(kWebSocketErrorAbnormalClosure);
  }

 private:
  // |delegate_| needs to be declared before |url_request_| so that it gets
  // initialised first.
  scoped_ptr<Delegate> delegate_;

  // Deleting the StreamRequestImpl object deletes this URLRequest object,
  // cancelling the whole connection.
  URLRequest url_request_;

  scoped_ptr<WebSocketStream::ConnectDelegate> connect_delegate_;

  // Owned by the URLRequest.
  WebSocketHandshakeStreamCreateHelper* create_helper_;
};

void Delegate::OnResponseStarted(URLRequest* request) {
  switch (request->GetResponseCode()) {
    case HTTP_SWITCHING_PROTOCOLS:
      owner_->PerformUpgrade();
      return;

    case HTTP_UNAUTHORIZED:
    case HTTP_PROXY_AUTHENTICATION_REQUIRED:
      return;

    default:
      owner_->ReportFailure();
  }
}

void Delegate::OnAuthRequired(URLRequest* request,
                              AuthChallengeInfo* auth_info) {
  request->CancelAuth();
}

void Delegate::OnCertificateRequested(URLRequest* request,
                                      SSLCertRequestInfo* cert_request_info) {
  request->ContinueWithCertificate(NULL);
}

void Delegate::OnSSLCertificateError(URLRequest* request,
                                     const SSLInfo& ssl_info,
                                     bool fatal) {
  request->Cancel();
}

void Delegate::OnReadCompleted(URLRequest* request, int bytes_read) {
  NOTREACHED();
}

// Internal implementation of CreateAndConnectStream and
// CreateAndConnectStreamForTesting.
scoped_ptr<WebSocketStreamRequest> CreateAndConnectStreamWithCreateHelper(
    const GURL& socket_url,
    scoped_ptr<WebSocketHandshakeStreamCreateHelper> create_helper,
    const GURL& origin,
    URLRequestContext* url_request_context,
    const BoundNetLog& net_log,
    scoped_ptr<WebSocketStream::ConnectDelegate> connect_delegate) {
  scoped_ptr<StreamRequestImpl> request(
      new StreamRequestImpl(socket_url,
                            url_request_context,
                            connect_delegate.Pass(),
                            create_helper.get()));
  HttpRequestHeaders headers;
  headers.SetHeader(websockets::kUpgrade, websockets::kWebSocketLowercase);
  headers.SetHeader(HttpRequestHeaders::kConnection, websockets::kUpgrade);
  headers.SetHeader(HttpRequestHeaders::kOrigin, origin.spec());
  // TODO(ricea): Move the version number to websocket_handshake_constants.h
  headers.SetHeader(websockets::kSecWebSocketVersion,
                    websockets::kSupportedVersion);
  request->url_request()->SetExtraRequestHeaders(headers);
  request->url_request()->SetUserData(
      WebSocketHandshakeStreamBase::CreateHelper::DataKey(),
      create_helper.release());
  request->url_request()->SetLoadFlags(LOAD_DISABLE_CACHE |
                                       LOAD_DO_NOT_PROMPT_FOR_LOGIN);
  request->url_request()->Start();
  return request.PassAs<WebSocketStreamRequest>();
}

}  // namespace

WebSocketStreamRequest::~WebSocketStreamRequest() {}

WebSocketStream::WebSocketStream() {}
WebSocketStream::~WebSocketStream() {}

WebSocketStream::ConnectDelegate::~ConnectDelegate() {}

scoped_ptr<WebSocketStreamRequest> WebSocketStream::CreateAndConnectStream(
    const GURL& socket_url,
    const std::vector<std::string>& requested_subprotocols,
    const GURL& origin,
    URLRequestContext* url_request_context,
    const BoundNetLog& net_log,
    scoped_ptr<ConnectDelegate> connect_delegate) {
  scoped_ptr<WebSocketHandshakeStreamCreateHelper> create_helper(
      new WebSocketHandshakeStreamCreateHelper(requested_subprotocols));
  return CreateAndConnectStreamWithCreateHelper(socket_url,
                                                create_helper.Pass(),
                                                origin,
                                                url_request_context,
                                                net_log,
                                                connect_delegate.Pass());
}

// This is declared in websocket_test_util.h.
scoped_ptr<WebSocketStreamRequest> CreateAndConnectStreamForTesting(
      const GURL& socket_url,
      scoped_ptr<WebSocketHandshakeStreamCreateHelper> create_helper,
      const GURL& origin,
      URLRequestContext* url_request_context,
      const BoundNetLog& net_log,
      scoped_ptr<WebSocketStream::ConnectDelegate> connect_delegate) {
  return CreateAndConnectStreamWithCreateHelper(socket_url,
                                                create_helper.Pass(),
                                                origin,
                                                url_request_context,
                                                net_log,
                                                connect_delegate.Pass());
}

}  // namespace net
