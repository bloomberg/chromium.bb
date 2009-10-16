// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSAGENT_DELEGATE_H_
#define WEBKIT_GLUE_WEBDEVTOOLSAGENT_DELEGATE_H_

#include "base/basictypes.h"

namespace WebKit {
class WebString;
}

class WebDevToolsAgentDelegate {
 public:
  WebDevToolsAgentDelegate() {}
  virtual ~WebDevToolsAgentDelegate() {}

  virtual void SendMessageToClient(const WebKit::WebString& class_name,
                                   const WebKit::WebString& method_name,
                                   const WebKit::WebString& param1,
                                   const WebKit::WebString& param2,
                                   const WebKit::WebString& param3) = 0;

  // Invalidates widget which leads to the repaint.
  virtual void ForceRepaint() = 0;

  // Returns the id of the entity hosting this agent.
  virtual int GetHostId() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsAgentDelegate);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSAGENT_DELEGATE_H_
