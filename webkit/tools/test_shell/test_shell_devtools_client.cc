// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsFrontend.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

#undef LOG
#include "webkit/tools/test_shell/test_shell_devtools_agent.h"
#include "webkit/tools/test_shell/test_shell_devtools_callargs.h"
#include "webkit/tools/test_shell/test_shell_devtools_client.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"

using WebKit::WebDevToolsAgent;
using WebKit::WebDevToolsFrontend;
using WebKit::WebDevToolsMessageData;
using WebKit::WebString;
using WebKit::WebView;

TestShellDevToolsClient::TestShellDevToolsClient(TestShellDevToolsAgent *agent,
                                                 WebView* web_view)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      dev_tools_agent_(agent),
      web_view_(web_view) {
  web_tools_frontend_.reset(WebDevToolsFrontend::create(web_view_, this,
      WebString::fromUTF8("en-US")));
  dev_tools_agent_->attach(this);
}

TestShellDevToolsClient::~TestShellDevToolsClient() {
  // It is a chance that page will be destroyed at detach step of
  // dev_tools_agent_ and we should clean pending requests a bit earlier.
  weak_factory_.InvalidateWeakPtrs();
  if (dev_tools_agent_)
    dev_tools_agent_->detach();
}

void TestShellDevToolsClient::sendMessageToBackend(
     const WebString& data) {
  if (dev_tools_agent_)
    dev_tools_agent_->AsyncCall(TestShellDevToolsCallArgs(data));
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
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&TestShellDevToolsClient::Call, weak_factory_.GetWeakPtr(),
                 args));
}

void TestShellDevToolsClient::Call(const TestShellDevToolsCallArgs &args) {
  web_tools_frontend_->dispatchOnInspectorFrontend(args.data_);
  if (TestShellDevToolsCallArgs::calls_count() == 1)
    all_messages_processed();
}

void TestShellDevToolsClient::all_messages_processed() {
  web_view_->mainFrame()->executeScript(WebKit::WebScriptSource(
      WebString::fromUTF8("if (window.WebInspector && "
      "WebInspector.queuesAreEmpty) WebInspector.queuesAreEmpty();")));
}
