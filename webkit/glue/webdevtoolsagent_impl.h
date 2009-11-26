// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_
#define WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_

#include <string>

#include <wtf/OwnPtr.h>

#include "v8.h"
#include "third_party/WebKit/WebKit/chromium/src/WebDevToolsAgentPrivate.h"
#include "webkit/glue/devtools/devtools_rpc.h"
#include "webkit/glue/devtools/apu_agent_delegate.h"
#include "webkit/glue/devtools/tools_agent.h"

namespace WebCore {
class Document;
class InspectorController;
class Node;
class ScriptState;
class String;
}

namespace WebKit {
class WebDevToolsAgentClient;
class WebFrame;
class WebFrameImpl;
class WebString;
class WebURLRequest;
class WebURLResponse;
class WebViewImpl;
struct WebURLError;
}

class BoundObject;
class DebuggerAgentDelegateStub;
class DebuggerAgentImpl;
class Value;

class WebDevToolsAgentImpl : public WebKit::WebDevToolsAgentPrivate,
                             public ToolsAgent,
                             public DevToolsRpc::Delegate {
 public:
  WebDevToolsAgentImpl(WebKit::WebViewImpl* web_view_impl,
                       WebKit::WebDevToolsAgentClient* client);
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

  // WebDevToolsAgentPrivate implementation.
  virtual void didClearWindowObject(WebKit::WebFrameImpl* frame);
  virtual void didCommitProvisionalLoad(
      WebKit::WebFrameImpl* frame, bool is_new_navigation);

  // WebDevToolsAgent implementation.
  virtual void attach();
  virtual void detach();
  virtual void didNavigate();
  virtual void dispatchMessageFromFrontend(
      const WebKit::WebString& class_name,
      const WebKit::WebString& method_name,
      const WebKit::WebString& param1,
      const WebKit::WebString& param2,
      const WebKit::WebString& param3);
  virtual void inspectElementAt(const WebKit::WebPoint& point);
  virtual void setRuntimeFeatureEnabled(const WebKit::WebString& feature,
                                        bool enabled);
  virtual void identifierForInitialRequest(
      unsigned long resourceId,
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request);
  virtual void willSendRequest(unsigned long resourceId,
                               const WebKit::WebURLRequest& request);
  virtual void didReceiveData(unsigned long resourceId, int length);
  virtual void didReceiveResponse(
      unsigned long resourceId,
      const WebKit::WebURLResponse& response);
  virtual void didFinishLoading(unsigned long resourceId);
  virtual void didFailLoading(
      unsigned long resourceId,
      const WebKit::WebURLError& error);

  // DevToolsRpc::Delegate implementation.
  void SendRpcMessage(const WebCore::String& class_name,
                      const WebCore::String& method_name,
                      const WebCore::String& param1,
                      const WebCore::String& param2,
                      const WebCore::String& param3);

  void ForceRepaint();

  int host_id() { return host_id_; }

 private:
  static v8::Handle<v8::Value> JsDispatchOnClient(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsDispatchToApu(const v8::Arguments& args);
  static v8::Handle<v8::Value> JsOnRuntimeFeatureStateChanged(
      const v8::Arguments& args);

  void DisposeUtilityContext();
  void UnhideResourcesPanelIfNecessary();

  void InitDevToolsAgentHost();
  void ResetInspectorFrontendProxy();
  void setApuAgentEnabled(bool enabled);

  WebCore::InspectorController* GetInspectorController();

  // Creates InspectorBackend v8 wrapper in the utility context so that it's
  // methods prototype is Function.protoype object from the utility context.
  // Otherwise some useful methods  defined on Function.prototype(such as bind)
  // are missing for InspectorController native methods.
  v8::Local<v8::Object> CreateInspectorBackendV8Wrapper();

  int host_id_;
  WebKit::WebDevToolsAgentClient* client_;
  WebKit::WebViewImpl* web_view_impl_;
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
};

#endif  // WEBKIT_GLUE_WEBDEVTOOLSAGENT_IMPL_H_
