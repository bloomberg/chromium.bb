// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_URL_REQUEST_CONTEXT_H_
#define REMOTING_HOST_URL_REQUEST_CONTEXT_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "net/proxy/proxy_config_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context_storage.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {

// Subclass of net::URLRequestContext which can be used to store extra
// information for requests. This subclass is meant to be used in the
// remoting Me2Me host process where the profile is not available.
class URLRequestContext : public net::URLRequestContext {
 public:
  explicit URLRequestContext(
      scoped_ptr<net::ProxyConfigService> proxy_config_service);

 private:
  virtual ~URLRequestContext();

  net::URLRequestContextStorage storage_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContext);
};

class URLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  URLRequestContextGetter(base::MessageLoopProxy* ui_message_loop,
                          MessageLoop* io_message_loop,
                          MessageLoopForIO* file_message_loop);

  // Overridden from net::URLRequestContextGetter:
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy>
      GetIOMessageLoopProxy() const OVERRIDE;

 protected:
  virtual ~URLRequestContextGetter();

 private:
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_ptr<net::URLRequestContext> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextGetter);
};

}  // namespace remoting

#endif  // REMOTING_HOST_URL_REQUEST_CONTEXT_H_
