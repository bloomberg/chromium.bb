// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <string>

#include "Document.h"
#include "EventListener.h"
#include "InspectorController.h"
#include "InspectorFrontend.h"
#include "InspectorResource.h"
#include "Node.h"
#include "Page.h"
#include "PlatformString.h"
#include "ScriptObject.h"
#include "ScriptState.h"
#include "ScriptValue.h"
#include "V8Binding.h"
#include "V8Proxy.h"
#include <wtf/OwnPtr.h>
#undef LOG

#include "base/values.h"
#include "webkit/api/public/WebDataSource.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/glue/devtools/bound_object.h"
#include "webkit/glue/devtools/debugger_agent_impl.h"
#include "webkit/glue/devtools/debugger_agent_manager.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdevtoolsagent_delegate.h"
#include "webkit/glue/webdevtoolsagent_impl.h"
#include "webkit/glue/webview_impl.h"

using WebCore::Document;
using WebCore::InspectorController;
using WebCore::InspectorFrontend;
using WebCore::InspectorResource;
using WebCore::Node;
using WebCore::Page;
using WebCore::ScriptObject;
using WebCore::ScriptState;
using WebCore::ScriptValue;
using WebCore::String;
using WebCore::V8ClassIndex;
using WebCore::V8DOMWrapper;
using WebCore::V8Proxy;
using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebURLRequest;

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebViewImpl* web_view_impl,
    WebDevToolsAgentDelegate* delegate)
    : host_id_(delegate->GetHostId()),
      delegate_(delegate),
      web_view_impl_(web_view_impl),
      attached_(false) {
  debugger_agent_delegate_stub_.set(new DebuggerAgentDelegateStub(this));
  tools_agent_delegate_stub_.set(new ToolsAgentDelegateStub(this));
  tools_agent_native_delegate_stub_.set(new ToolsAgentNativeDelegateStub(this));
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl() {
  DebuggerAgentManager::OnWebViewClosed(web_view_impl_);
  DisposeUtilityContext();
}

void WebDevToolsAgentImpl::DisposeUtilityContext() {
  if (!utility_context_.IsEmpty()) {
    utility_context_.Dispose();
    utility_context_.Clear();
  }
}

void WebDevToolsAgentImpl::UnhideResourcesPanelIfNecessary() {
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  ic->ensureResourceTrackingSettingsLoaded();
  String command = String::format("[\"setResourcesPanelEnabled\", %s]",
      ic->resourceTrackingEnabled() ? "true" : "false");
  tools_agent_delegate_stub_->DispatchOnClient(command);
}

void WebDevToolsAgentImpl::Attach() {
  if (attached_) {
    return;
  }
  debugger_agent_impl_.set(
      new DebuggerAgentImpl(web_view_impl_,
                            debugger_agent_delegate_stub_.get(),
                            this));
  Page* page = web_view_impl_->page();
  debugger_agent_impl_->CreateUtilityContext(page->mainFrame(), &utility_context_);
  InitDevToolsAgentHost();

  UnhideResourcesPanelIfNecessary();
  v8::HandleScope scope;
  v8::Context::Scope context_scope(utility_context_);

  ScriptState* state = scriptStateFromPage(web_view_impl_->page());
  v8::Handle<v8::Object> injected_script = v8::Local<v8::Object>::Cast(
      utility_context_->Global()->Get(v8::String::New("InjectedScript")));
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  ic->setFrontendProxyObject(
      state,
      ScriptObject(state, utility_context_->Global()),
      ScriptObject(state, injected_script));
  // Allow controller to send messages to the frontend.
  ic->setWindowVisible(true, false);
  attached_ = true;
}

void WebDevToolsAgentImpl::Detach() {
  // Prevent controller from sending messages to the frontend.
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  ic->setWindowVisible(false, false);
  DisposeUtilityContext();
  devtools_agent_host_.set(NULL);
  debugger_agent_impl_.set(NULL);
  attached_ = false;
}

void WebDevToolsAgentImpl::OnNavigate() {
  DebuggerAgentManager::OnNavigate();
}

void WebDevToolsAgentImpl::DidCommitLoadForFrame(
    WebViewImpl* webview,
    WebFrame* frame,
    bool is_new_navigation) {
  if (!attached_) {
    return;
  }
  WebDataSource* ds = frame->dataSource();
  const WebURLRequest& request = ds->request();
  GURL url = ds->hasUnreachableURL() ?
      ds->unreachableURL() :
      request.url();
  if (webview->GetMainFrame() == frame) {
    DisposeUtilityContext();
    debugger_agent_impl_->CreateUtilityContext(webview->page()->mainFrame(),
                                               &utility_context_);
    InitDevToolsAgentHost();
    tools_agent_delegate_stub_->FrameNavigate(
        url.possibly_invalid_spec());
  }
  UnhideResourcesPanelIfNecessary();
}

void WebDevToolsAgentImpl::WindowObjectCleared(WebFrameImpl* webframe) {
  DebuggerAgentManager::SetHostId(webframe, host_id_);
  if (attached_) {
    // Push context id into the client if it is already attached.
    debugger_agent_delegate_stub_->SetContextId(host_id_);
  }
}

void WebDevToolsAgentImpl::ForceRepaint() {
  delegate_->ForceRepaint();
}

void WebDevToolsAgentImpl::ExecuteUtilityFunction(
      int call_id,
      const String& function_name,
      const String& json_args) {
  String result;
  String exception;
  result = debugger_agent_impl_->ExecuteUtilityFunction(utility_context_,
      function_name, json_args, &exception);
  tools_agent_delegate_stub_->DidExecuteUtilityFunction(call_id,
      result, exception);
}

void WebDevToolsAgentImpl::GetResourceContent(
    int call_id,
    int identifier) {
  String content;
  Page* page = web_view_impl_->page();
  if (page) {
    RefPtr<InspectorResource> resource =
        page->inspectorController()->resources().get(identifier);
    if (resource.get()) {
      content = resource->sourceString();
    }
  }
  tools_agent_native_delegate_stub_->DidGetResourceContent(call_id, content);
}

void WebDevToolsAgentImpl::DispatchMessageFromClient(
    const std::string& class_name,
    const std::string& method_name,
    const std::string& raw_msg) {
  OwnPtr<ListValue> message(
      static_cast<ListValue*>(DevToolsRpc::ParseMessage(raw_msg)));
  if (ToolsAgentDispatch::Dispatch(
      this, class_name, method_name, *message.get())) {
    return;
  }

  if (!attached_) {
    return;
  }

  if (debugger_agent_impl_.get() &&
      DebuggerAgentDispatch::Dispatch(
          debugger_agent_impl_.get(), class_name, method_name,
          *message.get())) {
    return;
  }
}

void WebDevToolsAgentImpl::InspectElement(int x, int y) {
  Node* node = web_view_impl_->GetNodeForWindowPos(x, y);
  if (!node) {
    return;
  }
}

void WebDevToolsAgentImpl::SendRpcMessage(
    const std::string& class_name,
    const std::string& method_name,
    const std::string& raw_msg) {
  delegate_->SendMessageToClient(class_name, method_name, raw_msg);
}

void WebDevToolsAgentImpl::InitDevToolsAgentHost() {
  devtools_agent_host_.set(
      new BoundObject(utility_context_, this, "DevToolsAgentHost"));
  devtools_agent_host_->AddProtoFunction(
      "dispatch",
      WebDevToolsAgentImpl::JsDispatchOnClient);
  devtools_agent_host_->Build();

  v8::HandleScope scope;
  v8::Context::Scope utility_scope(utility_context_);
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  utility_context_->Global()->Set(
      v8::String::New("InspectorController"),
      V8DOMWrapper::convertToV8Object(V8ClassIndex::INSPECTORBACKEND,
                                      ic->inspectorBackend()));
}

// static
v8::Handle<v8::Value> WebDevToolsAgentImpl::JsDispatchOnClient(
    const v8::Arguments& args) {
  v8::TryCatch exception_catcher;
  String message = WebCore::toWebCoreStringWithNullCheck(args[0]);
  if (message.isEmpty() || exception_catcher.HasCaught()) {
    return v8::Undefined();
  }
  WebDevToolsAgentImpl* agent = static_cast<WebDevToolsAgentImpl*>(
      v8::External::Cast(*args.Data())->Value());
  agent->tools_agent_delegate_stub_->DispatchOnClient(message);
  return v8::Undefined();
}

// static
void WebDevToolsAgent::ExecuteDebuggerCommand(
    const std::string& command,
    int caller_id) {
  DebuggerAgentManager::ExecuteDebuggerCommand(command, caller_id);
}

// static
void WebDevToolsAgent::SetMessageLoopDispatchHandler(
    MessageLoopDispatchHandler handler) {
  DebuggerAgentManager::SetMessageLoopDispatchHandler(handler);
}
