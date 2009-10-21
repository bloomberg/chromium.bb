// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of WebSocketStreamHandle.

#include "webkit/glue/websocketstreamhandle_impl.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebSocketStreamHandleClient.h"
#include "webkit/glue/websocketstreamhandle_bridge.h"
#include "webkit/glue/websocketstreamhandle_delegate.h"

namespace webkit_glue {

// WebSocketStreamHandleImpl::Context -----------------------------------------

class WebSocketStreamHandleImpl::Context
    : public base::RefCounted<Context>,
      public WebSocketStreamHandleDelegate {
 public:
  explicit Context(WebSocketStreamHandleImpl* handle);

  WebKit::WebSocketStreamHandleClient* client() const { return client_; }
  void set_client(WebKit::WebSocketStreamHandleClient* client) {
    client_ = client;
  }

  void Connect(const WebKit::WebURL& url);
  bool Send(const WebKit::WebData& data);
  void Close();

  // WebSocketStreamHandleDelegate methods:
  virtual void WillOpenStream(WebKit::WebSocketStreamHandle*, const GURL&);
  virtual void DidOpenStream(WebKit::WebSocketStreamHandle*, int);
  virtual void DidSendData(WebKit::WebSocketStreamHandle*, int);
  virtual void DidReceiveData(
      WebKit::WebSocketStreamHandle*, const char*, int);
  virtual void DidClose(WebKit::WebSocketStreamHandle*);

 private:
  friend class base::RefCounted<Context>;
  ~Context() {}

  WebSocketStreamHandleImpl* handle_;
  WebKit::WebSocketStreamHandleClient* client_;
  WebSocketStreamHandleBridge* bridge_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

WebSocketStreamHandleImpl::Context::Context(WebSocketStreamHandleImpl* handle)
    : handle_(handle),
      client_(NULL),
      bridge_(ALLOW_THIS_IN_INITIALIZER_LIST(
          WebSocketStreamHandleBridge::Create(handle_, this))) {
}

void WebSocketStreamHandleImpl::Context::Connect(const WebKit::WebURL& url) {
  LOG(INFO) << "Connect url=" << url;
  DCHECK(bridge_);
  bridge_->Connect(url);
}

bool WebSocketStreamHandleImpl::Context::Send(const WebKit::WebData& data) {
  LOG(INFO) << "Send data.size=" << data.size();
  DCHECK(bridge_);
  return bridge_->Send(
      std::vector<char>(data.data(), data.data() + data.size()));
}

void WebSocketStreamHandleImpl::Context::Close() {
  LOG(INFO) << "Close";
  if (bridge_)
    bridge_->Close();
}

void WebSocketStreamHandleImpl::Context::WillOpenStream(
    WebKit::WebSocketStreamHandle* web_handle, const GURL& url) {
  LOG(INFO) << "WillOpenStream url=" << url;
  if (client_)
    client_->willOpenStream(handle_, url);
}

void WebSocketStreamHandleImpl::Context::DidOpenStream(
    WebKit::WebSocketStreamHandle* web_handle, int max_amount_send_allowed) {
  LOG(INFO) << "DidOpen";
  if (client_)
    client_->didOpenStream(handle_, max_amount_send_allowed);
}

void WebSocketStreamHandleImpl::Context::DidSendData(
    WebKit::WebSocketStreamHandle* web_handle, int amount_sent) {
  if (client_)
    client_->didSendData(handle_, amount_sent);
}

void WebSocketStreamHandleImpl::Context::DidReceiveData(
    WebKit::WebSocketStreamHandle* web_handle, const char* data, int size) {
  if (client_)
    client_->didReceiveData(handle_, WebKit::WebData(data, size));
}

void WebSocketStreamHandleImpl::Context::DidClose(
    WebKit::WebSocketStreamHandle* web_handle) {
  LOG(INFO) << "DidClose";
  bridge_ = 0;
  WebSocketStreamHandleImpl* handle = handle_;
  handle_ = 0;
  if (client_) {
    WebKit::WebSocketStreamHandleClient* client = client_;
    client_ = NULL;
    client->didClose(handle);
  }
}

// WebSocketStreamHandleImpl ------------------------------------------------

WebSocketStreamHandleImpl::WebSocketStreamHandleImpl()
    : ALLOW_THIS_IN_INITIALIZER_LIST(context_(new Context(this))) {
}

WebSocketStreamHandleImpl::~WebSocketStreamHandleImpl() {
  close();
}

void WebSocketStreamHandleImpl::connect(
    const WebKit::WebURL& url, WebKit::WebSocketStreamHandleClient* client) {
  LOG(INFO) << "connect url=" << url;
  DCHECK(!context_->client());
  context_->set_client(client);

  context_->Connect(url);
}

bool WebSocketStreamHandleImpl::send(const WebKit::WebData& data) {
  return context_->Send(data);
}

void WebSocketStreamHandleImpl::close() {
  context_->Close();
}

}  // namespace webkit_glue
