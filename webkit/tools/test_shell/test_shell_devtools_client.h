// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_DEVTOOLS_CLIENT_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_DEVTOOLS_CLIENT_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsFrontendClient.h"

namespace WebKit {

class WebDevToolsFrontend;

}  // namespace WebKit

class TestShellDevToolsCallArgs;
class TestShellDevToolsAgent;

class TestShellDevToolsClient: public WebKit::WebDevToolsFrontendClient {
 public:
  TestShellDevToolsClient(TestShellDevToolsAgent* agent,
                          WebKit::WebView* web_view);
  virtual ~TestShellDevToolsClient();

  // WebDevToolsFrontendClient implementation
  virtual void sendFrontendLoaded();
  virtual void sendMessageToBackend(const WebKit::WebString& data);
  virtual void sendDebuggerCommandToAgent(const WebKit::WebString& command);

  virtual void activateWindow();
  virtual void closeWindow();
  virtual void dockWindow();
  virtual void undockWindow();

  void AsyncCall(const TestShellDevToolsCallArgs& args);

  void all_messages_processed();

 private:
  void Call(const TestShellDevToolsCallArgs& args);

  base::WeakPtrFactory<TestShellDevToolsClient> weak_factory_;
  TestShellDevToolsAgent* dev_tools_agent_;
  WebKit::WebView* web_view_;
  scoped_ptr<WebKit::WebDevToolsFrontend> web_tools_frontend_;

  DISALLOW_COPY_AND_ASSIGN(TestShellDevToolsClient);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_DEVTOOLS_CLIENT_H_
