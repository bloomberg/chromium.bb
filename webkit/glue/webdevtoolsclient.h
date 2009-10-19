// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSCLIENT_H_
#define WEBKIT_GLUE_WEBDEVTOOLSCLIENT_H_

#include "base/basictypes.h"

class WebDevToolsClientDelegate;

namespace WebKit {
class WebString;
class WebView;
}

// WebDevToolsClient represents DevTools client sitting in the Glue. It provides
// direct and delegate Apis to the host.
class WebDevToolsClient {
 public:
  static WebDevToolsClient* Create(
      WebKit::WebView* view,
      WebDevToolsClientDelegate* delegate,
      const WebKit::WebString& application_locale);

  WebDevToolsClient() {}
  virtual ~WebDevToolsClient() {}

  virtual void DispatchMessageFromAgent(const WebKit::WebString& class_name,
                                        const WebKit::WebString& method_name,
                                        const WebKit::WebString& param1,
                                        const WebKit::WebString& param2,
                                        const WebKit::WebString& param3) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsClient);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSCLIENT_H_
