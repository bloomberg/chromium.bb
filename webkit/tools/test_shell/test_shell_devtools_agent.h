// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_SHELL_DEVTOOLS_AGENT_H_
#define TEST_SHELL_DEVTOOLS_AGENT_H_

#include "base/task.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgentClient.h"

namespace WebKit {

class WebDevToolsAgent;
class WebView;
struct WebDevToolsMessageData;

} // namespace WebKit

class TestShellDevToolsCallArgs;
class TestShellDevToolsClient;

class TestShellDevToolsAgent : public WebKit::WebDevToolsAgentClient {
 public:
  TestShellDevToolsAgent();
  virtual ~TestShellDevToolsAgent();

  void SetWebView(WebKit::WebView* web_view);

  // WebDevToolsAgentClient implementation.
  virtual void sendMessageToInspectorFrontend(
      const WebKit::WebString& data);
  virtual int hostIdentifier();
  virtual void runtimePropertyChanged(const WebKit::WebString& name,
                                      const WebKit::WebString& value);
  virtual WebKit::WebCString debuggerScriptSource();

  virtual WebKit::WebDevToolsAgentClient::WebKitClientMessageLoop*
      createClientMessageLoop();

  void AsyncCall(const TestShellDevToolsCallArgs& args);

  void attach(TestShellDevToolsClient* client);
  void detach();
  void frontendLoaded();

  bool evaluateInWebInspector(long call_id, const std::string& script);
  bool setTimelineProfilingEnabled(bool enable);

 private:
  void Call(const TestShellDevToolsCallArgs& args);
  void DelayedFrontendLoaded();
  static void DispatchMessageLoop();
  WebKit::WebDevToolsAgent* GetWebAgent();

  ScopedRunnableMethodFactory<TestShellDevToolsAgent> call_method_factory_;
  TestShellDevToolsClient* dev_tools_client_;
  int routing_id_;
  WebKit::WebDevToolsAgent* web_dev_tools_agent_;
  WebKit::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(TestShellDevToolsAgent);
};

#endif  // TEST_SHELL_DEVTOOLS_AGENT_H_
