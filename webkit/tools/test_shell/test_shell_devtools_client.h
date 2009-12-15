// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_SHELL_DEVTOOLS_CLIENT_H_
#define TEST_SHELL_DEVTOOLS_CLIENT_H_

#include "base/scoped_ptr.h"
#include "base/task.h"

#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsFrontendClient.h"

namespace WebKit {

class WebDevToolsFrontend;
struct WebDevToolsMessageData;

} // namespace WebKit

class TestShellDevToolsCallArgs;
class TestShellDevToolsAgent;

class TestShellDevToolsClient: public WebKit::WebDevToolsFrontendClient {

 public:
  TestShellDevToolsClient(TestShellDevToolsAgent* agent,
                          WebKit::WebView* web_view);
  virtual ~TestShellDevToolsClient();

  // WebDevToolsFrontendClient implementation
  virtual void sendMessageToAgent(const WebKit::WebDevToolsMessageData& data);
  virtual void sendDebuggerCommandToAgent(const WebKit::WebString& command);

  virtual void activateWindow();
  virtual void closeWindow();
  virtual void dockWindow();
  virtual void undockWindow();

  void AsyncCall(const TestShellDevToolsCallArgs& args);

  void all_messages_processed();

 private:
  void Call(const TestShellDevToolsCallArgs& args);

  ScopedRunnableMethodFactory<TestShellDevToolsClient> call_method_factory_;
  TestShellDevToolsAgent* dev_tools_agent_;
  WebKit::WebView* web_view_;
  scoped_ptr<WebKit::WebDevToolsFrontend> web_tools_frontend_;

  DISALLOW_COPY_AND_ASSIGN(TestShellDevToolsClient);
};

#endif  // TEST_SHELL_DEVTOOLS_CLIENT_H_
