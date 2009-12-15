// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsFrontend.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"

#undef LOG
#include "webkit/tools/test_shell/test_shell_devtools_agent.h"
#include "webkit/tools/test_shell/test_shell_devtools_callargs.h"
#include "webkit/tools/test_shell/test_shell_devtools_client.h"

#include "base/command_line.h"
#include "base/message_loop.h"

using WebKit::WebDevToolsAgent;
using WebKit::WebDevToolsFrontend;
using WebKit::WebDevToolsMessageData;
using WebKit::WebString;
using WebKit::WebView;

// Warning at call_method_factory_(this) treated as error and is switched off.

#if defined(OS_WIN)
#pragma warning(disable : 4355)
#endif // defined(OS_WIN)

TestShellDevToolsClient::TestShellDevToolsClient(TestShellDevToolsAgent *agent,
                                                 WebView* web_view)
  : call_method_factory_(this),
    dev_tools_agent_(agent),
    web_view_(web_view) {
  web_tools_frontend_.reset(
    WebDevToolsFrontend::create(web_view_,
                                this,
                                WebString::fromUTF8("en-US")));
  dev_tools_agent_->attach(this);
}

TestShellDevToolsClient::~TestShellDevToolsClient() {
  // It is a chance that page will be destroyed at detach step of
  // dev_tools_agent_ and we should clean pending requests a bit earlier.
  call_method_factory_.RevokeAll();
  if (dev_tools_agent_)
    dev_tools_agent_->detach(this);
}

void TestShellDevToolsClient::sendMessageToAgent(
     const WebDevToolsMessageData& data) {
  if (dev_tools_agent_)
    dev_tools_agent_->AsyncCall(TestShellDevToolsCallArgs(data));
}

void TestShellDevToolsClient::sendDebuggerCommandToAgent(
     const WebString& command) {
  WebDevToolsAgent::executeDebuggerCommand(command, 1);
}

void TestShellDevToolsClient::activateWindow() {
  NOTIMPLEMENTED();
}

void TestShellDevToolsClient::closeWindow() {
  NOTIMPLEMENTED();
}

void TestShellDevToolsClient::dockWindow() {
  NOTIMPLEMENTED();
}

void TestShellDevToolsClient::undockWindow() {
  NOTIMPLEMENTED();
}

void TestShellDevToolsClient::AsyncCall(const TestShellDevToolsCallArgs &args) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      call_method_factory_.NewRunnableMethod(&TestShellDevToolsClient::Call,
                                             args),
      0);
}

void TestShellDevToolsClient::Call(const TestShellDevToolsCallArgs &args) {
  web_tools_frontend_->dispatchMessageFromAgent(args.data_);
  if (TestShellDevToolsCallArgs::calls_count() == 1)
    all_messages_processed();
}

void TestShellDevToolsClient::all_messages_processed() {
  web_view_->mainFrame()->executeScript(
    WebKit::WebScriptSource(WebString::fromUTF8(
      "if (window.WebInspector && WebInspector.queuesAreEmpty) WebInspector.queuesAreEmpty();")));
}
