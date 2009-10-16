// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSCLIENT_IMPL_H_
#define WEBKIT_GLUE_WEBDEVTOOLSCLIENT_IMPL_H_

#include <string>

#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

#include "v8.h"
#include "webkit/glue/devtools/devtools_rpc.h"
#include "webkit/glue/webdevtoolsclient.h"

namespace WebCore {
class Node;
class Page;
class String;
}

class BoundObject;
class JsDebuggerAgentBoundObj;
class JsNetAgentBoundObj;
class JsToolsAgentBoundObj;
class ToolsAgentNativeDelegateImpl;
class WebDevToolsClientDelegate;
class WebViewImpl;

class WebDevToolsClientImpl : public WebDevToolsClient,
                              public DevToolsRpc::Delegate {
 public:
  WebDevToolsClientImpl(
      WebViewImpl* web_view_impl,
      WebDevToolsClientDelegate* delegate,
      const String& application_locale);
  virtual ~WebDevToolsClientImpl();

  // DevToolsRpc::Delegate implementation.
  virtual void SendRpcMessage(const String& class_name,
                              const String& method_name,
                              const String& param1,
                              const String& param2,
                              const String& param3);

  // WebDevToolsClient implementation.
  virtual void DispatchMessageFromAgent(const WebKit::WebString& class_name,
                                        const WebKit::WebString& method_name,
                                        const WebKit::WebString& param1,
                                        const WebKit::WebString& param2,
                                        const WebKit::WebString& param3);

 private:
  void AddResourceSourceToFrame(int resource_id,
                                String mime_type,
                                WebCore::Node* frame);

  void ExecuteScript(const Vector<String>& v);
  static v8::Handle<v8::Value> JsReset(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsAddSourceToFrame(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsAddResourceSourceToFrame(
      const v8::Arguments& args);
  static v8::Handle<v8::Value> JsLoaded(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsGetPlatform(const v8::Arguments& args);

  static v8::Handle<v8::Value> JsActivateWindow(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsCloseWindow(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsDockWindow(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsUndockWindow(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsToggleInspectElementMode(
      const v8::Arguments& args);
  static v8::Handle<v8::Value> JsGetApplicationLocale(
      const v8::Arguments& args);
  static v8::Handle<v8::Value> JsHiddenPanels(
      const v8::Arguments& args);
  static v8::Handle<v8::Value> JsDebuggerCommand(
      const v8::Arguments& args);

  WebViewImpl* web_view_impl_;
  WebDevToolsClientDelegate* delegate_;
  String application_locale_;
  OwnPtr<BoundObject> debugger_command_executor_obj_;
  OwnPtr<JsDebuggerAgentBoundObj> debugger_agent_obj_;
  OwnPtr<JsToolsAgentBoundObj> tools_agent_obj_;
  bool loaded_;
  Vector<Vector<String> > pending_incoming_messages_;
  OwnPtr<BoundObject> dev_tools_host_;
  OwnPtr<ToolsAgentNativeDelegateImpl> tools_agent_native_delegate_impl_;
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsClientImpl);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSCLIENT_IMPL_H_
