// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_DEVTOOLS_AGENT_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_DEVTOOLS_AGENT_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgentClient.h"

namespace WebKit {

class WebDevToolsAgent;
class WebView;

}  // namespace WebKit

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

  virtual WebKit::WebDevToolsAgentClient::WebKitClientMessageLoop*
      createClientMessageLoop();

  void AsyncCall(const TestShellDevToolsCallArgs& args);

  void attach(TestShellDevToolsClient* client);
  void detach();
  void frontendLoaded();

  bool evaluateInWebInspector(long call_id, const std::string& script);

 private:
  void Call(const TestShellDevToolsCallArgs& args);
  void DelayedFrontendLoaded();
  WebKit::WebDevToolsAgent* GetWebAgent();

  base::WeakPtrFactory<TestShellDevToolsAgent> weak_factory_;
  TestShellDevToolsClient* dev_tools_client_;
  int routing_id_;
  WebKit::WebDevToolsAgent* web_dev_tools_agent_;
  WebKit::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(TestShellDevToolsAgent);
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_DEVTOOLS_AGENT_H_
