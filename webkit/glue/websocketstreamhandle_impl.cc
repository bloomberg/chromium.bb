// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of WebSocketStreamHandle.

#include "webkit/glue/websocketstreamhandle_impl.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSocketStreamHandleClient.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
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

  void Connect(const WebKit::WebURL& url, WebKitPlatformSupportImpl* platform);
  bool Send(const WebKit::WebData& data);
  void Close();

  // Must be called before |handle_| or |client_| is deleted.
  // Once detached, it never calls |client_| back.
  void Detach();

  // WebSocketStreamHandleDelegate methods:
  virtual void DidOpenStream(WebKit::WebSocketStreamHandle*, int);
  virtual void DidSendData(WebKit::WebSocketStreamHandle*, int);
  virtual void DidReceiveData(
      WebKit::WebSocketStreamHandle*, const char*, int);
  virtual void DidClose(WebKit::WebSocketStreamHandle*);

 private:
  friend class base::RefCounted<Context>;
  ~Context() {
    DCHECK(!handle_);
    DCHECK(!client_);
    DCHECK(!bridge_);
  }

  WebSocketStreamHandleImpl* handle_;
  WebKit::WebSocketStreamHandleClient* client_;
  // |bridge_| is alive from Connect to DidClose, so Context must be alive
  // in the time period.
  scoped_refptr<WebSocketStreamHandleBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

WebSocketStreamHandleImpl::Context::Context(WebSocketStreamHandleImpl* handle)
    : handle_(handle),
      client_(NULL),
      bridge_(NULL) {
}

void WebSocketStreamHandleImpl::Context::Connect(
    const WebKit::WebURL& url,
    WebKitPlatformSupportImpl* platform) {
  VLOG(1) << "Connect url=" << url;
  DCHECK(!bridge_);
  bridge_ = platform->CreateWebSocketBridge(handle_, this);
  AddRef();  // Will be released by DidClose().
  bridge_->Connect(url);
}

bool WebSocketStreamHandleImpl::Context::Send(const WebKit::WebData& data) {
  VLOG(1) << "Send data.size=" << data.size();
  DCHECK(bridge_);
  return bridge_->Send(
      std::vector<char>(data.data(), data.data() + data.size()));
}

void WebSocketStreamHandleImpl::Context::Close() {
  VLOG(1) << "Close";
  if (bridge_)
    bridge_->Close();
}

void WebSocketStreamHandleImpl::Context::Detach() {
  handle_ = NULL;
  client_ = NULL;
  // If Connect was called, |bridge_| is not NULL, so that this Context closes
  // the |bridge_| here.  Then |bridge_| will call back DidClose, and will
  // be released by itself.
  // Otherwise, |bridge_| is NULL.
  if (bridge_)
    bridge_->Close();
}

void WebSocketStreamHandleImpl::Context::DidOpenStream(
    WebKit::WebSocketStreamHandle* web_handle, int max_amount_send_allowed) {
  VLOG(1) << "DidOpen";
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
  VLOG(1) << "DidClose";
  bridge_ = NULL;
  WebSocketStreamHandleImpl* handle = handle_;
  handle_ = NULL;
  if (client_) {
    WebKit::WebSocketStreamHandleClient* client = client_;
    client_ = NULL;
    client->didClose(handle);
  }
  Release();
}

// WebSocketStreamHandleImpl ------------------------------------------------

WebSocketStreamHandleImpl::WebSocketStreamHandleImpl(
    WebKitPlatformSupportImpl* platform)
    : ALLOW_THIS_IN_INITIALIZER_LIST(context_(new Context(this))),
      platform_(platform) {
}

WebSocketStreamHandleImpl::~WebSocketStreamHandleImpl() {
  // We won't receive any events from |context_|.
  // |context_| is ref counted, and will be released when it received
  // DidClose.
  context_->Detach();
}

void WebSocketStreamHandleImpl::connect(
    const WebKit::WebURL& url, WebKit::WebSocketStreamHandleClient* client) {
  VLOG(1) << "connect url=" << url;
  DCHECK(!context_->client());
  context_->set_client(client);

  context_->Connect(url, platform_);
}

bool WebSocketStreamHandleImpl::send(const WebKit::WebData& data) {
  return context_->Send(data);
}

void WebSocketStreamHandleImpl::close() {
  context_->Close();
}

}  // namespace webkit_glue
