// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_RENDER_URLREQUEST_IMPL_H_
#define CEF_LIBCEF_RENDERER_RENDER_URLREQUEST_IMPL_H_

#include "include/cef_frame.h"
#include "include/cef_urlrequest.h"

#include "base/memory/ref_counted.h"

class CefRenderURLRequest : public CefURLRequest {
 public:
  class Context;

  // If |frame| is nullptr the default URLLoaderFactory will be used. That
  // factory only supports http(s) and blob requests that cannot be
  // intercepted in the browser process.
  CefRenderURLRequest(CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      CefRefPtr<CefURLRequestClient> client);
  ~CefRenderURLRequest() override;

  bool Start();

  // CefURLRequest methods.
  CefRefPtr<CefRequest> GetRequest() override;
  CefRefPtr<CefURLRequestClient> GetClient() override;
  Status GetRequestStatus() override;
  ErrorCode GetRequestError() override;
  CefRefPtr<CefResponse> GetResponse() override;
  bool ResponseWasCached() override;
  void Cancel() override;

 private:
  bool VerifyContext();

  scoped_refptr<Context> context_;

  IMPLEMENT_REFCOUNTING(CefRenderURLRequest);
};

#endif  // CEF_LIBCEF_RENDERER_RENDER_URLREQUEST_IMPL_H_
