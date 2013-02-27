// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBURLLOADER_IMPL_H_
#define WEBKIT_GLUE_WEBURLLOADER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLLoader.h"
#include "webkit/glue/webkit_glue_export.h"

namespace webkit_glue {

class WebKitPlatformSupportImpl;

class WebURLLoaderImpl : public WebKit::WebURLLoader {
 public:
  explicit WebURLLoaderImpl(WebKitPlatformSupportImpl* platform);
  virtual ~WebURLLoaderImpl();

  // WebURLLoader methods:
  virtual void loadSynchronously(
      const WebKit::WebURLRequest& request,
      WebKit::WebURLResponse& response,
      WebKit::WebURLError& error,
      WebKit::WebData& data);
  virtual void loadAsynchronously(
      const WebKit::WebURLRequest& request,
      WebKit::WebURLLoaderClient* client);
  virtual void cancel();
  virtual void setDefersLoading(bool value);
  virtual void didChangePriority(WebKit::WebURLRequest::Priority new_priority);

 private:
  class Context;
  scoped_refptr<Context> context_;
  WebKitPlatformSupportImpl* platform_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBURLLOADER_IMPL_H_
