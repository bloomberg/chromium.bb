// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_stream.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
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
#include "url/origin.h"

namespace net {
namespace {

class StreamRequestImpl;

class Delegate : public URLRequest::Delegate {
 public:
  enum HandshakeResult {
    INCOMPLETE,
    CONNECTED,
    FAILED,
    NUM_HANDSHAKE_RESULT_TYPES,
  };

  explicit Delegate(StreamRequestImpl* owner)
      : owner_(owner), result_(INCOMPLETE) {}
  virtual ~Delegate() {
    UMA_HISTOGRAM_ENUMERATION(
        "Net.WebSocket.HandshakeResult", result_, NUM_HANDSHAKE_RESULT_TYPES);
  }

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
  HandshakeResult result_;
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
    std::string failure_message;
    if (create_helper_->stream()) {
      failure_message = create_helper_->stream()->GetFailureMessage();
    } else {
      switch (url_request_.status().status()) {
        case URLRequestStatus::SUCCESS:
        case URLRequestStatus::IO_PENDING:
          break;
        case URLRequestStatus::CANCELED:
          failure_message = "WebSocket opening handshake was canceled";
          break;
        case URLRequestStatus::FAILED:
          failure_message =
              std::string("Error in connection establishment: ") +
              ErrorToString(url_request_.status().error());
          break;
      }
    }
    connect_delegate_->OnFailure(failure_message);
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
      result_ = CONNECTED;
      owner_->PerformUpgrade();
      return;

    case HTTP_UNAUTHORIZED:
    case HTTP_PROXY_AUTHENTICATION_REQUIRED:
      return;

    default:
      result_ = FAILED;
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
    const url::Origin& origin,
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
  headers.SetHeader(HttpRequestHeaders::kOrigin, origin.string());
  headers.SetHeader(websockets::kSecWebSocketVersion,
                    websockets::kSupportedVersion);
  request->url_request()->SetExtraRequestHeaders(headers);
  request->url_request()->SetUserData(
      WebSocketHandshakeStreamBase::CreateHelper::DataKey(),
      create_helper.release());
  request->url_request()->SetLoadFlags(LOAD_DISABLE_CACHE |
                                       LOAD_BYPASS_CACHE |
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
    const url::Origin& origin,
    URLRequestContext* url_request_context,
    const BoundNetLog& net_log,
    scoped_ptr<ConnectDelegate> connect_delegate) {
  scoped_ptr<WebSocketHandshakeStreamCreateHelper> create_helper(
      new WebSocketHandshakeStreamCreateHelper(connect_delegate.get(),
                                               requested_subprotocols));
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
    const url::Origin& origin,
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
