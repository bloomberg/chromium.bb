// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <string>

#include "Document.h"
#include "EventListener.h"
#include "InjectedScriptHost.h"
#include "InspectorBackend.h"
#include "InspectorController.h"
#include "InspectorFrontend.h"
#include "InspectorResource.h"
#include "Node.h"
#include "Page.h"
#include "PlatformString.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptObject.h"
#include "ScriptState.h"
#include "ScriptValue.h"
#include "V8Binding.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include <wtf/OwnPtr.h>
#undef LOG

#include "third_party/WebKit/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsAgentClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsMessageData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/WebKit/chromium/src/WebFrameImpl.h"
#include "third_party/WebKit/WebKit/chromium/src/WebViewImpl.h"
#include "webkit/glue/devtools/bound_object.h"
#include "webkit/glue/devtools/debugger_agent_impl.h"
#include "webkit/glue/devtools/debugger_agent_manager.h"
#include "webkit/glue/devtools/profiler_agent_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdevtoolsagent_impl.h"

using WebCore::Document;
using WebCore::DocumentLoader;
using WebCore::FrameLoader;
using WebCore::InjectedScriptHost;
using WebCore::InspectorBackend;
using WebCore::InspectorController;
using WebCore::InspectorFrontend;
using WebCore::InspectorResource;
using WebCore::Node;
using WebCore::Page;
using WebCore::ResourceError;
using WebCore::ResourceRequest;
using WebCore::ResourceResponse;
using WebCore::SafeAllocation;
using WebCore::ScriptObject;
using WebCore::ScriptState;
using WebCore::ScriptValue;
using WebCore::String;
using WebCore::V8ClassIndex;
using WebCore::V8DOMWrapper;
using WebCore::V8Proxy;
using WebKit::WebDataSource;
using WebKit::WebDevToolsAgentClient;
using WebKit::WebDevToolsMessageData;
using WebKit::WebFrame;
using WebKit::WebFrameImpl;
using WebKit::WebPoint;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebViewImpl;

namespace {

void InjectedScriptHostWeakReferenceCallback(v8::Persistent<v8::Value> object,
                                             void* parameter) {
  InjectedScriptHost* host = static_cast<InjectedScriptHost*>(parameter);
  host->deref();
  object.Dispose();
}

void InspectorBackendWeakReferenceCallback(v8::Persistent<v8::Value> object,
                                           void* parameter) {
  InspectorBackend* backend = static_cast<InspectorBackend*>(parameter);
  backend->deref();
  object.Dispose();
}

void SetApuAgentEnabledInUtilityContext(v8::Handle<v8::Context> context,
                                        bool enabled) {
  v8::HandleScope handle_scope;
  v8::Context::Scope context_scope(context);
  v8::Handle<v8::Object> dispatcher = v8::Local<v8::Object>::Cast(
      context->Global()->Get(v8::String::New("ApuAgentDispatcher")));
  if (dispatcher.IsEmpty()) {
    return;
  }
  dispatcher->Set(v8::String::New("enabled"), v8::Boolean::New(enabled));
}

// TODO(pfeldman): Make this public in WebDevToolsAgent API.
static const char kApuAgentFeatureName[] = "apu-agent";

// Keep these in sync with the ones in inject_dispatch.js.
static const char kTimelineFeatureName[] = "timeline-profiler";
static const char kResourceTrackingFeatureName[] = "resource-tracking";

class IoRpcDelegate : public DevToolsRpc::Delegate {
 public:
  IoRpcDelegate() {}
  virtual ~IoRpcDelegate() {}
  virtual void SendRpcMessage(
      const WebKit::WebDevToolsMessageData& data) {
    WebDevToolsAgentClient::sendMessageToFrontendOnIOThread(data);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IoRpcDelegate);
};

} //  namespace

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebViewImpl* web_view_impl,
    WebDevToolsAgentClient* client)
    : host_id_(client->hostIdentifier()),
      client_(client),
      web_view_impl_(web_view_impl),
      apu_agent_enabled_(false),
      resource_tracking_was_enabled_(false),
      attached_(false) {
  debugger_agent_delegate_stub_.set(new DebuggerAgentDelegateStub(this));
  tools_agent_delegate_stub_.set(new ToolsAgentDelegateStub(this));
  tools_agent_native_delegate_stub_.set(new ToolsAgentNativeDelegateStub(this));
  apu_agent_delegate_stub_.set(new ApuAgentDelegateStub(this));
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

void WebDevToolsAgentImpl::attach() {
  if (attached_) {
    return;
  }
  debugger_agent_impl_.set(
      new DebuggerAgentImpl(web_view_impl_,
                            debugger_agent_delegate_stub_.get(),
                            this));
  ResetInspectorFrontendProxy();
  UnhideResourcesPanelIfNecessary();
  // Allow controller to send messages to the frontend.
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  ic->setWindowVisible(true, false);
  attached_ = true;
}

void WebDevToolsAgentImpl::detach() {
  // Prevent controller from sending messages to the frontend.
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  ic->hideHighlight();
  ic->close();
  DisposeUtilityContext();
  inspector_frontend_script_state_.clear();
  debugger_agent_impl_.set(NULL);
  attached_ = false;
  apu_agent_enabled_ = false;
}

void WebDevToolsAgentImpl::didNavigate() {
  DebuggerAgentManager::OnNavigate();
}

void WebDevToolsAgentImpl::didCommitProvisionalLoad(
    WebFrameImpl* webframe,
    bool is_new_navigation) {
  if (!attached_) {
    return;
  }
  WebDataSource* ds = webframe->dataSource();
  const WebURLRequest& request = ds->request();
  WebURL url = ds->hasUnreachableURL() ?
      ds->unreachableURL() :
      request.url();
  if (!webframe->parent()) {
    ResetInspectorFrontendProxy();
    tools_agent_delegate_stub_->FrameNavigate(
        webkit_glue::WebURLToKURL(url).string());
    SetApuAgentEnabledInUtilityContext(utility_context_, apu_agent_enabled_);
    UnhideResourcesPanelIfNecessary();
  }
}

void WebDevToolsAgentImpl::didClearWindowObject(WebFrameImpl* webframe) {
  DebuggerAgentManager::SetHostId(webframe, host_id_);
  if (attached_) {
    // Push context id into the client if it is already attached.
    debugger_agent_delegate_stub_->SetContextId(host_id_);
  }
}

void WebDevToolsAgentImpl::ForceRepaint() {
  client_->forceRepaint();
}

void WebDevToolsAgentImpl::DispatchOnInspectorController(
      int call_id,
      const String& function_name,
      const String& json_args) {
  String result;
  String exception;
  result = debugger_agent_impl_->ExecuteUtilityFunction(utility_context_,
      call_id, "InspectorControllerDispatcher", function_name, json_args,
         false /* is sync */, &exception);
  tools_agent_delegate_stub_->DidDispatchOn(call_id,
      result, exception);
}

void WebDevToolsAgentImpl::DispatchOnInjectedScript(
      int call_id,
      const String& function_name,
      const String& json_args) {
  String result;
  String exception;
  String fname = function_name;
  bool async = function_name.endsWith("_async");
  if (async) {
    fname = fname.substring(0, fname.length() - 6);
  }
  result = debugger_agent_impl_->ExecuteUtilityFunction(utility_context_,
      call_id, "InjectedScript", fname, json_args, async, &exception);
  if (!async) {
    tools_agent_delegate_stub_->DidDispatchOn(call_id,
        result, exception);
  }
}

void WebDevToolsAgentImpl::ExecuteVoidJavaScript() {
  debugger_agent_impl_->ExecuteVoidJavaScript(utility_context_);
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

void WebDevToolsAgentImpl::dispatchMessageFromFrontend(
    const WebDevToolsMessageData& data) {
  if (ToolsAgentDispatch::Dispatch(this, data)) {
    return;
  }

  if (!attached_) {
    return;
  }

  if (debugger_agent_impl_.get() &&
      DebuggerAgentDispatch::Dispatch(debugger_agent_impl_.get(), data)) {
    return;
  }
}

void WebDevToolsAgentImpl::inspectElementAt(const WebPoint& point) {
  web_view_impl_->inspectElementAt(point);
}

void WebDevToolsAgentImpl::setRuntimeFeatureEnabled(const WebString& wfeature,
                                                    bool enabled) {
  String feature = webkit_glue::WebStringToString(wfeature);
  if (feature == kApuAgentFeatureName) {
    setApuAgentEnabled(enabled);
  } else if (feature == kTimelineFeatureName) {
    setTimelineProfilingEnabled(enabled);
  } else if (feature == kResourceTrackingFeatureName) {
    InspectorController* ic = web_view_impl_->page()->inspectorController();
    if (enabled)
      ic->enableResourceTracking(false /* not sticky */, false /* no reload */);
    else
      ic->disableResourceTracking(false /* not sticky */);
  }
}

void WebDevToolsAgentImpl::SendRpcMessage(
    const WebKit::WebDevToolsMessageData& data) {
  client_->sendMessageToFrontend(data);
}

void WebDevToolsAgentImpl::InitDevToolsAgentHost() {
  BoundObject devtools_agent_host(utility_context_, this, "DevToolsAgentHost");
  devtools_agent_host.AddProtoFunction(
      "dispatch",
      WebDevToolsAgentImpl::JsDispatchOnClient);
  devtools_agent_host.AddProtoFunction(
      "dispatchToApu",
      WebDevToolsAgentImpl::JsDispatchToApu);
  devtools_agent_host.AddProtoFunction(
      "runtimeFeatureStateChanged",
      WebDevToolsAgentImpl::JsOnRuntimeFeatureStateChanged);
  devtools_agent_host.Build();

  v8::HandleScope scope;
  v8::Context::Scope utility_scope(utility_context_);
  // Call custom code to create inspector backend wrapper in the utility context
  // instead of calling V8DOMWrapper::convertToV8Object that would create the
  // wrapper in the Page main frame context.
  v8::Handle<v8::Object> script_host_wrapper =
      CreateInjectedScriptHostV8Wrapper();
  if (script_host_wrapper.IsEmpty()) {
    return;
  }
  utility_context_->Global()->Set(
      v8::String::New("InjectedScriptHost"),
      script_host_wrapper);

  v8::Handle<v8::Object> backend_wrapper = CreateInspectorBackendV8Wrapper();
  if (backend_wrapper.IsEmpty()) {
    return;
  }
  utility_context_->Global()->Set(
      v8::String::New("InspectorBackend"),
      backend_wrapper);
}

v8::Local<v8::Object>
    WebDevToolsAgentImpl::CreateInjectedScriptHostV8Wrapper() {
  V8ClassIndex::V8WrapperType descriptorType =
      V8ClassIndex::INJECTEDSCRIPTHOST;
  v8::Handle<v8::Function> function =
      V8DOMWrapper::getTemplate(descriptorType)->GetFunction();
  if (function.IsEmpty()) {
    // Return if allocation failed.
    return v8::Local<v8::Object>();
  }
  v8::Local<v8::Object> instance = SafeAllocation::newInstance(function);
  if (instance.IsEmpty()) {
    // Avoid setting the wrapper if allocation failed.
    return v8::Local<v8::Object>();
  }
  InjectedScriptHost* host =
      web_view_impl_->page()->inspectorController()->injectedScriptHost();
  V8DOMWrapper::setDOMWrapper(instance, V8ClassIndex::ToInt(descriptorType),
                              host);
  // Create a weak reference to the v8 wrapper of InspectorBackend to deref
  // InspectorBackend when the wrapper is garbage collected.
  host->ref();
  v8::Persistent<v8::Object> weak_handle =
      v8::Persistent<v8::Object>::New(instance);
  weak_handle.MakeWeak(host, &InjectedScriptHostWeakReferenceCallback);
  return instance;
}

v8::Local<v8::Object> WebDevToolsAgentImpl::CreateInspectorBackendV8Wrapper() {
  V8ClassIndex::V8WrapperType descriptorType = V8ClassIndex::INSPECTORBACKEND;
  v8::Handle<v8::Function> function =
      V8DOMWrapper::getTemplate(descriptorType)->GetFunction();
  if (function.IsEmpty()) {
    // Return if allocation failed.
    return v8::Local<v8::Object>();
  }
  v8::Local<v8::Object> instance = SafeAllocation::newInstance(function);
  if (instance.IsEmpty()) {
    // Avoid setting the wrapper if allocation failed.
    return v8::Local<v8::Object>();
  }
  InspectorBackend* backend =
      web_view_impl_->page()->inspectorController()->inspectorBackend();
  V8DOMWrapper::setDOMWrapper(instance, V8ClassIndex::ToInt(descriptorType),
                              backend);
  // Create a weak reference to the v8 wrapper of InspectorBackend to deref
  // InspectorBackend when the wrapper is garbage collected.
  backend->ref();
  v8::Persistent<v8::Object> weak_handle =
      v8::Persistent<v8::Object>::New(instance);
  weak_handle.MakeWeak(backend, &InspectorBackendWeakReferenceCallback);
  return instance;
}

void WebDevToolsAgentImpl::ResetInspectorFrontendProxy() {
  DisposeUtilityContext();
  debugger_agent_impl_->CreateUtilityContext(
      web_view_impl_->page()->mainFrame(),
      &utility_context_);
  InitDevToolsAgentHost();

  v8::HandleScope scope;
  v8::Context::Scope context_scope(utility_context_);
  inspector_frontend_script_state_.set(new ScriptState(
      web_view_impl_->page()->mainFrame(),
      utility_context_));
  v8::Handle<v8::Object> injected_script = v8::Local<v8::Object>::Cast(
      utility_context_->Global()->Get(v8::String::New("InjectedScript")));
  ScriptState* state = inspector_frontend_script_state_.get();
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  ic->setFrontendProxyObject(
      state,
      ScriptObject(state, utility_context_->Global()),
      ScriptObject(state, injected_script));
}

void WebDevToolsAgentImpl::setApuAgentEnabled(bool enabled) {
  apu_agent_enabled_ = enabled;
  SetApuAgentEnabledInUtilityContext(utility_context_, enabled);
  InspectorController* ic = web_view_impl_->page()->inspectorController();
  if (enabled) {
    resource_tracking_was_enabled_ = ic->resourceTrackingEnabled();
    ic->startTimelineProfiler();
    if (!resource_tracking_was_enabled_) {
      // TODO(knorton): Introduce some kind of agents dependency here so that
      // user could turn off resource tracking while apu agent is on.
      ic->enableResourceTracking(false, false);
    }
    debugger_agent_impl_->set_auto_continue_on_exception(true);
  } else {
    ic->stopTimelineProfiler();
    if (!resource_tracking_was_enabled_) {
      ic->disableResourceTracking(false);
    }
    resource_tracking_was_enabled_ = false;
  }
  client_->runtimeFeatureStateChanged(
      webkit_glue::StringToWebString(kApuAgentFeatureName),
      enabled);
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
v8::Handle<v8::Value> WebDevToolsAgentImpl::JsDispatchToApu(
    const v8::Arguments& args) {
  v8::TryCatch exception_catcher;
  String message = WebCore::toWebCoreStringWithNullCheck(args[0]);
  if (message.isEmpty() || exception_catcher.HasCaught()) {
    return v8::Undefined();
  }
  WebDevToolsAgentImpl* agent = static_cast<WebDevToolsAgentImpl*>(
      v8::External::Cast(*args.Data())->Value());
  agent->apu_agent_delegate_stub_->DispatchToApu(message);
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsAgentImpl::JsOnRuntimeFeatureStateChanged(
    const v8::Arguments& args) {
  v8::TryCatch exception_catcher;
  String feature = WebCore::toWebCoreStringWithNullCheck(args[0]);
  bool enabled = args[1]->ToBoolean()->Value();
  if (feature.isEmpty() || exception_catcher.HasCaught()) {
    return v8::Undefined();
  }
  WebDevToolsAgentImpl* agent = static_cast<WebDevToolsAgentImpl*>(
      v8::External::Cast(*args.Data())->Value());
  agent->client_->runtimeFeatureStateChanged(
      webkit_glue::StringToWebString(feature),
      enabled);
  return v8::Undefined();
}


WebCore::InspectorController* WebDevToolsAgentImpl::GetInspectorController() {
  if (Page* page = web_view_impl_->page())
    return page->inspectorController();
  return NULL;
}


//------- plugin resource load notifications ---------------
void WebDevToolsAgentImpl::identifierForInitialRequest(
    unsigned long resourceId,
    WebKit::WebFrame* frame,
    const WebURLRequest& request) {
  if (InspectorController* ic = GetInspectorController()) {
    WebFrameImpl* web_frame_impl = static_cast<WebFrameImpl*>(frame);
    FrameLoader* frame_loader = web_frame_impl->frame()->loader();
    DocumentLoader* loader = frame_loader->activeDocumentLoader();
    ic->identifierForInitialRequest(resourceId, loader,
                                    request.toResourceRequest());
  }
}

void WebDevToolsAgentImpl::willSendRequest(
    unsigned long resourceId,
    const WebURLRequest& request) {
  if (InspectorController* ic = GetInspectorController()) {
    ic->willSendRequest(resourceId, request.toResourceRequest(),
                        ResourceResponse());
  }
}

void WebDevToolsAgentImpl::didReceiveData(unsigned long resourceId,
                                          int length) {
  if (InspectorController* ic = GetInspectorController())
    ic->didReceiveContentLength(resourceId, length);
}

void WebDevToolsAgentImpl::didReceiveResponse(
    unsigned long resourceId,
    const WebURLResponse& response) {
  if (InspectorController* ic = GetInspectorController())
    ic->didReceiveResponse(resourceId, response.toResourceResponse());
}

void WebDevToolsAgentImpl::didFinishLoading(
    unsigned long resourceId) {
  if (InspectorController* ic = GetInspectorController()) {
    ic->didFinishLoading(resourceId);
  }
}

void WebDevToolsAgentImpl::didFailLoading(unsigned long resourceId,
                                          const WebURLError& error) {
  ResourceError resource_error;
  if (InspectorController* ic = GetInspectorController())
    ic->didFailLoading(resourceId, resource_error);
}

void WebDevToolsAgentImpl::evaluateInWebInspector(long call_id,
                                                  const WebString& script) {
  InspectorController* ic = GetInspectorController();
  ic->evaluateForTestInFrontend(call_id,
                                webkit_glue::WebStringToString(script));
}

void WebDevToolsAgentImpl::setTimelineProfilingEnabled(bool enabled) {
  InspectorController* ic = GetInspectorController();
  if (enabled)
    ic->startTimelineProfiler();
  else
    ic->stopTimelineProfiler();
}


namespace WebKit {

// static
WebDevToolsAgent* WebDevToolsAgent::create(WebView* webview,
                                           WebDevToolsAgentClient* client) {
  return new WebDevToolsAgentImpl(static_cast<WebViewImpl*>(webview), client);
}

// static
void WebDevToolsAgent::executeDebuggerCommand(
    const WebString& command,
    int caller_id) {
  DebuggerAgentManager::ExecuteDebuggerCommand(
      webkit_glue::WebStringToString(command), caller_id);
}

// static
void WebDevToolsAgent::debuggerPauseScript() {
  DebuggerAgentManager::PauseScript();
}

// static
void WebDevToolsAgent::setMessageLoopDispatchHandler(
    MessageLoopDispatchHandler handler) {
  DebuggerAgentManager::SetMessageLoopDispatchHandler(handler);
}

// static
bool WebDevToolsAgent::dispatchMessageFromFrontendOnIOThread(
    const WebDevToolsMessageData& data) {
  IoRpcDelegate transport;
  ProfilerAgentDelegateStub stub(&transport);
  ProfilerAgentImpl agent(&stub);
  return ProfilerAgentDispatch::Dispatch(&agent, data);
}


} // namespace WebKit
