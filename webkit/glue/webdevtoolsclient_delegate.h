// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSCLIENT_DELEGATE_H_
#define WEBKIT_GLUE_WEBDEVTOOLSCLIENT_DELEGATE_H_

#include "base/basictypes.h"

namespace WebKit {
class WebString;
}

class WebDevToolsClientDelegate {
 public:
  WebDevToolsClientDelegate() {}
  virtual ~WebDevToolsClientDelegate() {}

  virtual void SendMessageToAgent(const WebKit::WebString& class_name,
                                  const WebKit::WebString& method_name,
                                  const WebKit::WebString& param1,
                                  const WebKit::WebString& param2,
                                  const WebKit::WebString& param3) = 0;
  virtual void SendDebuggerCommandToAgent(const WebKit::WebString& command) = 0;

  virtual void ActivateWindow() = 0;
  virtual void CloseWindow() = 0;
  virtual void DockWindow() = 0;
  virtual void UndockWindow() = 0;
  virtual void ToggleInspectElementMode(bool enabled) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsClientDelegate);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSAGENT_DELEGATE_H_
