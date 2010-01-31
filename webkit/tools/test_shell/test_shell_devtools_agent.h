// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_SHELL_DEVTOOLS_AGENT_H_
#define TEST_SHELL_DEVTOOLS_AGENT_H_

#include "base/task.h"

#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsAgentClient.h"

namespace WebKit {

class WebViewImpl;
struct WebDevToolsMessageData;

} // namespace WebKit

class TestShellDevToolsCallArgs;
class TestShellDevToolsClient;

class TestShellDevToolsAgent : public WebKit::WebDevToolsAgentClient {

 public:
  TestShellDevToolsAgent(WebKit::WebView* web_view);
  virtual ~TestShellDevToolsAgent() {}

  // WebDevToolsAgentClient implementation.
  virtual void sendMessageToFrontend(
      const WebKit::WebDevToolsMessageData& data);
  virtual int hostIdentifier() { return routing_id_; }
  virtual void forceRepaint();
  virtual void runtimeFeatureStateChanged(const WebKit::WebString& feature,
                                          bool enabled);
  virtual WebKit::WebCString injectedScriptSource();
  virtual WebKit::WebCString injectedScriptDispatcherSource();

  void AsyncCall(const TestShellDevToolsCallArgs& args);

  void attach(TestShellDevToolsClient* client);
  void detach(TestShellDevToolsClient* client);

  bool evaluateInWebInspector(long call_id, const std::string& script);
  bool setTimelineProfilingEnabled(bool enable);

 private:
  void Call(const TestShellDevToolsCallArgs& args);
  static void DispatchMessageLoop();
  WebKit::WebDevToolsAgent* GetWebAgent();

  ScopedRunnableMethodFactory<TestShellDevToolsAgent> call_method_factory_;
  TestShellDevToolsClient* dev_tools_client_;
  int routing_id_;
  WebKit::WebDevToolsAgent* web_dev_tools_agent_;
  WebKit::WebViewImpl* web_view_;

  DISALLOW_COPY_AND_ASSIGN(TestShellDevToolsAgent);
};

#endif  // TEST_SHELL_DEVTOOLS_AGENT_H_
