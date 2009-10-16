// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_
#define WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_

#include <string>

#include <wtf/OwnPtr.h>

#include "v8.h"
#include "webkit/glue/devtools/devtools_rpc.h"
#include "webkit/glue/devtools/apu_agent_delegate.h"
#include "webkit/glue/devtools/tools_agent.h"
#include "webkit/glue/webdevtoolsagent.h"

namespace WebCore {
class Document;
class Node;
class ScriptState;
class String;
}

namespace WebKit {
class WebFrame;
}

class BoundObject;
class DebuggerAgentDelegateStub;
class DebuggerAgentImpl;
class Value;
class WebDevToolsAgentDelegate;
class WebFrameImpl;
class WebViewImpl;

class WebDevToolsAgentImpl
    : public WebDevToolsAgent,
      public ToolsAgent,
      public DevToolsRpc::Delegate {
 public:
  WebDevToolsAgentImpl(WebViewImpl* web_view_impl,
      WebDevToolsAgentDelegate* delegate);
  virtual ~WebDevToolsAgentImpl();

  // ToolsAgent implementation.
  virtual void DispatchOnInspectorController(
      int call_id,
      const WebCore::String& function_name,
      const WebCore::String& json_args);
  virtual void DispatchOnInjectedScript(
      int call_id,
      const WebCore::String& function_name,
      const WebCore::String& json_args);
  virtual void ExecuteVoidJavaScript();
  virtual void GetResourceContent(
      int call_id,
      int identifier);
  virtual void SetApuAgentEnabled(bool enable);

  // WebDevToolsAgent implementation.
  virtual void Attach();
  virtual void Detach();
  virtual void OnNavigate();
  virtual void DispatchMessageFromClient(const WebKit::WebString& class_name,
                                         const WebKit::WebString& method_name,
                                         const WebKit::WebString& param1,
                                         const WebKit::WebString& param2,
                                         const WebKit::WebString& param3);
  virtual void InspectElement(int x, int y);

  // DevToolsRpc::Delegate implementation.
  void SendRpcMessage(const WebCore::String& class_name,
                      const WebCore::String& method_name,
                      const WebCore::String& param1,
                      const WebCore::String& param2,
                      const WebCore::String& param3);

  // Methods called by the glue.
  void SetMainFrameDocumentReady(bool ready);
  void DidCommitLoadForFrame(WebViewImpl* webview,
                             WebKit::WebFrame* frame,
                             bool is_new_navigation);

  void WindowObjectCleared(WebFrameImpl* webframe);

  void ForceRepaint();

  int host_id() { return host_id_; }

 private:
  static v8::Handle<v8::Value> JsDispatchOnClient(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsDispatchToApu(const v8::Arguments& args);
  void DisposeUtilityContext();
  void UnhideResourcesPanelIfNecessary();

  void InitDevToolsAgentHost();
  void ResetInspectorFrontendProxy();

  // Creates InspectorBackend v8 wrapper in the utility context so that it's
  // methods prototype is Function.protoype object from the utility context.
  // Otherwise some useful methods  defined on Function.prototype(such as bind)
  // are missing for InspectorController native methods.
  v8::Local<v8::Object> CreateInspectorBackendV8Wrapper();

  int host_id_;
  WebDevToolsAgentDelegate* delegate_;
  WebViewImpl* web_view_impl_;
  OwnPtr<DebuggerAgentDelegateStub> debugger_agent_delegate_stub_;
  OwnPtr<ToolsAgentDelegateStub> tools_agent_delegate_stub_;
  OwnPtr<ToolsAgentNativeDelegateStub> tools_agent_native_delegate_stub_;
  OwnPtr<DebuggerAgentImpl> debugger_agent_impl_;
  OwnPtr<ApuAgentDelegateStub> apu_agent_delegate_stub_;
  bool apu_agent_enabled_;
  bool resource_tracking_was_enabled_;
  bool attached_;
  // TODO(pfeldman): This should not be needed once GC styles issue is fixed
  // for matching rules.
  v8::Persistent<v8::Context> utility_context_;
  OwnPtr<BoundObject> devtools_agent_host_;
  OwnPtr<WebCore::ScriptState> inspector_frontend_script_state_;
  DISALLOW_COPY_AND_ASSIGN(WebDevToolsAgentImpl);
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_
