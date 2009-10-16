// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <string>

#include "Document.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "InspectorBackend.h"
#include "InspectorController.h"
#include "Node.h"
#include "Page.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>
#undef LOG

#include "base/string_util.h"
#include "webkit/api/public/WebFrame.h"
#include "webkit/api/public/WebScriptSource.h"
#include "webkit/glue/devtools/bound_object.h"
#include "webkit/glue/devtools/debugger_agent.h"
#include "webkit/glue/devtools/devtools_rpc_js.h"
#include "webkit/glue/devtools/tools_agent.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdevtoolsclient_delegate.h"
#include "webkit/glue/webdevtoolsclient_impl.h"
#include "webkit/glue/webview_impl.h"

using namespace WebCore;
using WebKit::WebFrame;
using WebKit::WebScriptSource;
using WebKit::WebString;

static v8::Local<v8::String> ToV8String(const String& s) {
  if (s.isNull())
    return v8::Local<v8::String>();

  return v8::String::New(reinterpret_cast<const uint16_t*>(s.characters()),
                         s.length());
}

DEFINE_RPC_JS_BOUND_OBJ(DebuggerAgent, DEBUGGER_AGENT_STRUCT,
    DebuggerAgentDelegate, DEBUGGER_AGENT_DELEGATE_STRUCT)
DEFINE_RPC_JS_BOUND_OBJ(ToolsAgent, TOOLS_AGENT_STRUCT,
    ToolsAgentDelegate, TOOLS_AGENT_DELEGATE_STRUCT)

class ToolsAgentNativeDelegateImpl : public ToolsAgentNativeDelegate {
 public:
  struct ResourceContentRequestData {
    String mime_type;
    RefPtr<Node> frame;
  };

  ToolsAgentNativeDelegateImpl(WebFrameImpl* frame) : frame_(frame) {}
  virtual ~ToolsAgentNativeDelegateImpl() {}

  // ToolsAgentNativeDelegate implementation.
  virtual void DidGetResourceContent(int request_id, const String& content) {
    if (!resource_content_requests_.contains(request_id)) {
      NOTREACHED();
      return;
    }
    ResourceContentRequestData request =
        resource_content_requests_.take(request_id);

    InspectorController* ic = frame_->frame()->page()->inspectorController();
    if (request.frame && request.frame->attached()) {
      ic->inspectorBackend()->addSourceToFrame(request.mime_type,
                                               content,
                                               request.frame.get());
    }
  }

  bool WaitingForResponse(int resource_id, Node* frame) {
    if (resource_content_requests_.contains(resource_id)) {
      DCHECK(resource_content_requests_.get(resource_id).frame.get() == frame)
          << "Only one frame is expected to display given resource";
      return true;
    }
    return false;
  }

  void RequestSent(int resource_id, String mime_type, Node* frame) {
    ResourceContentRequestData data;
    data.mime_type = mime_type;
    data.frame = frame;
    DCHECK(!resource_content_requests_.contains(resource_id));
    resource_content_requests_.set(resource_id, data);
  }

 private:
  WebFrameImpl* frame_;
  HashMap<int, ResourceContentRequestData> resource_content_requests_;
  DISALLOW_COPY_AND_ASSIGN(ToolsAgentNativeDelegateImpl);
};

// static
WebDevToolsClient* WebDevToolsClient::Create(
    WebView* view,
    WebDevToolsClientDelegate* delegate,
    const WebString& application_locale) {
  return new WebDevToolsClientImpl(
      static_cast<WebViewImpl*>(view),
      delegate,
      webkit_glue::WebStringToString(application_locale));
}

WebDevToolsClientImpl::WebDevToolsClientImpl(
    WebViewImpl* web_view_impl,
    WebDevToolsClientDelegate* delegate,
    const String& application_locale)
    : web_view_impl_(web_view_impl),
      delegate_(delegate),
      application_locale_(application_locale),
      loaded_(false) {
  WebFrameImpl* frame = web_view_impl_->main_frame();
  v8::HandleScope scope;
  v8::Handle<v8::Context> frame_context = V8Proxy::context(frame->frame());

  debugger_agent_obj_.set(new JsDebuggerAgentBoundObj(
      this, frame_context, "RemoteDebuggerAgent"));
  tools_agent_obj_.set(
      new JsToolsAgentBoundObj(this, frame_context, "RemoteToolsAgent"));

  // Debugger commands should be sent using special method.
  debugger_command_executor_obj_.set(
      new BoundObject(frame_context, this, "RemoteDebuggerCommandExecutor"));
  debugger_command_executor_obj_->AddProtoFunction(
      "DebuggerCommand",
      WebDevToolsClientImpl::JsDebuggerCommand);
  debugger_command_executor_obj_->Build();

  dev_tools_host_.set(new BoundObject(frame_context, this, "DevToolsHost"));
  dev_tools_host_->AddProtoFunction(
      "reset",
      WebDevToolsClientImpl::JsReset);
  dev_tools_host_->AddProtoFunction(
      "addSourceToFrame",
      WebDevToolsClientImpl::JsAddSourceToFrame);
  dev_tools_host_->AddProtoFunction(
      "addResourceSourceToFrame",
      WebDevToolsClientImpl::JsAddResourceSourceToFrame);
  dev_tools_host_->AddProtoFunction(
      "loaded",
      WebDevToolsClientImpl::JsLoaded);
  dev_tools_host_->AddProtoFunction(
      "search",
      WebCore::V8Custom::v8InspectorBackendSearchCallback);
  dev_tools_host_->AddProtoFunction(
      "getPlatform",
      WebDevToolsClientImpl::JsGetPlatform);
  dev_tools_host_->AddProtoFunction(
      "activateWindow",
      WebDevToolsClientImpl::JsActivateWindow);
  dev_tools_host_->AddProtoFunction(
      "closeWindow",
      WebDevToolsClientImpl::JsCloseWindow);
  dev_tools_host_->AddProtoFunction(
      "dockWindow",
      WebDevToolsClientImpl::JsDockWindow);
  dev_tools_host_->AddProtoFunction(
      "undockWindow",
      WebDevToolsClientImpl::JsUndockWindow);
  dev_tools_host_->AddProtoFunction(
      "toggleInspectElementMode",
      WebDevToolsClientImpl::JsToggleInspectElementMode);
  dev_tools_host_->AddProtoFunction(
      "getApplicationLocale",
      WebDevToolsClientImpl::JsGetApplicationLocale);
  dev_tools_host_->AddProtoFunction(
      "hiddenPanels",
      WebDevToolsClientImpl::JsHiddenPanels);
  dev_tools_host_->Build();
}

WebDevToolsClientImpl::~WebDevToolsClientImpl() {
}

void WebDevToolsClientImpl::DispatchMessageFromAgent(
      const WebString& class_name,
      const WebString& method_name,
      const WebString& param1,
      const WebString& param2,
      const WebString& param3) {
  if (ToolsAgentNativeDelegateDispatch::Dispatch(
          tools_agent_native_delegate_impl_.get(),
          webkit_glue::WebStringToString(class_name),
          webkit_glue::WebStringToString(method_name),
          webkit_glue::WebStringToString(param1),
          webkit_glue::WebStringToString(param2),
          webkit_glue::WebStringToString(param3))) {
    return;
  }

  Vector<String> v;
  v.append(webkit_glue::WebStringToString(class_name));
  v.append(webkit_glue::WebStringToString(method_name));
  v.append(webkit_glue::WebStringToString(param1));
  v.append(webkit_glue::WebStringToString(param2));
  v.append(webkit_glue::WebStringToString(param3));
  if (!loaded_) {
    pending_incoming_messages_.append(v);
    return;
  }
  ExecuteScript(v);
}

void WebDevToolsClientImpl::AddResourceSourceToFrame(int resource_id,
                                                     String mime_type,
                                                     Node* frame) {
  if (tools_agent_native_delegate_impl_->WaitingForResponse(resource_id,
                                                            frame)) {
    return;
  }
  tools_agent_obj_->GetResourceContent(resource_id, resource_id);
  tools_agent_native_delegate_impl_->RequestSent(resource_id, mime_type, frame);
}

void WebDevToolsClientImpl::ExecuteScript(const Vector<String>& v) {
  WebFrameImpl* frame = web_view_impl_->main_frame();
  v8::HandleScope scope;
  v8::Handle<v8::Context> frame_context = V8Proxy::context(frame->frame());
  v8::Context::Scope context_scope(frame_context);
  v8::Handle<v8::Value> dispatch_function =
      frame_context->Global()->Get(v8::String::New("devtools$$dispatch"));
  ASSERT(dispatch_function->IsFunction());
  v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(dispatch_function);
  v8::Handle<v8::Value> args[] = {
    ToV8String(v.at(0)),
    ToV8String(v.at(1)),
    ToV8String(v.at(2)),
    ToV8String(v.at(3)),
    ToV8String(v.at(4)),
  };
  function->Call(frame_context->Global(), 5, args);
}

void WebDevToolsClientImpl::SendRpcMessage(const String& class_name,
                                           const String& method_name,
                                           const String& param1,
                                           const String& param2,
                                           const String& param3) {
  delegate_->SendMessageToAgent(
      webkit_glue::StringToWebString(class_name),
      webkit_glue::StringToWebString(method_name),
      webkit_glue::StringToWebString(param1),
      webkit_glue::StringToWebString(param2),
      webkit_glue::StringToWebString(param3));
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsReset(
    const v8::Arguments& args) {
  WebDevToolsClientImpl* client = static_cast<WebDevToolsClientImpl*>(
      v8::External::Cast(*args.Data())->Value());
  WebFrameImpl* frame = client->web_view_impl_->main_frame();
  client->tools_agent_native_delegate_impl_.set(
      new ToolsAgentNativeDelegateImpl(frame));
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsAddSourceToFrame(
    const v8::Arguments& args) {
  if (args.Length() < 2) {
    return v8::Undefined();
  }

  v8::TryCatch exception_catcher;

  String mime_type = WebCore::toWebCoreStringWithNullCheck(args[0]);
  if (mime_type.isEmpty() || exception_catcher.HasCaught()) {
    return v8::Undefined();
  }
  String source_string = WebCore::toWebCoreStringWithNullCheck(args[1]);
  if (source_string.isEmpty() || exception_catcher.HasCaught()) {
    return v8::Undefined();
  }
  v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(args[2]);
  Node* node = V8DOMWrapper::convertDOMWrapperToNode<Node>(wrapper);
  if (!node || !node->attached()) {
    return v8::Undefined();
  }

  Page* page = V8Proxy::retrieveFrameForEnteredContext()->page();
  InspectorController* inspectorController = page->inspectorController();
  return WebCore::v8Boolean(inspectorController->inspectorBackend()->
      addSourceToFrame(mime_type, source_string, node));
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsAddResourceSourceToFrame(
    const v8::Arguments& args) {
  int resource_id = static_cast<int>(args[0]->NumberValue());
  String mime_type = WebCore::toWebCoreStringWithNullCheck(args[1]);
  if (mime_type.isEmpty()) {
    return v8::Undefined();
  }
  v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(args[2]);
  Node* node = V8DOMWrapper::convertDOMWrapperToNode<Node>(wrapper);
  WebDevToolsClientImpl* client = static_cast<WebDevToolsClientImpl*>(
      v8::External::Cast(*args.Data())->Value());
  client->AddResourceSourceToFrame(resource_id, mime_type, node);
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsLoaded(
    const v8::Arguments& args) {
  WebDevToolsClientImpl* client = static_cast<WebDevToolsClientImpl*>(
      v8::External::Cast(*args.Data())->Value());
  client->loaded_ = true;

  // Grant the devtools page the ability to have source view iframes.
  Page* page = V8Proxy::retrieveFrameForEnteredContext()->page();
  SecurityOrigin* origin = page->mainFrame()->domWindow()->securityOrigin();
  origin->grantUniversalAccess();

  for (Vector<Vector<String> >::iterator it =
           client->pending_incoming_messages_.begin();
       it != client->pending_incoming_messages_.end();
       ++it) {
    client->ExecuteScript(*it);
  }
  client->pending_incoming_messages_.clear();
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsGetPlatform(
    const v8::Arguments& args) {
#if defined OS_MACOSX
  return v8String("mac-leopard");
#elif defined OS_LINUX
  return v8String("linux");
#else
  return v8String("windows");
#endif
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsActivateWindow(
    const v8::Arguments& args) {
  WebDevToolsClientImpl* client = static_cast<WebDevToolsClientImpl*>(
      v8::External::Cast(*args.Data())->Value());
  client->delegate_->ActivateWindow();
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsCloseWindow(
    const v8::Arguments& args) {
  WebDevToolsClientImpl* client = static_cast<WebDevToolsClientImpl*>(
      v8::External::Cast(*args.Data())->Value());
  client->delegate_->CloseWindow();
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsDockWindow(
    const v8::Arguments& args) {
  WebDevToolsClientImpl* client = static_cast<WebDevToolsClientImpl*>(
      v8::External::Cast(*args.Data())->Value());
  client->delegate_->DockWindow();
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsUndockWindow(
    const v8::Arguments& args) {
  WebDevToolsClientImpl* client = static_cast<WebDevToolsClientImpl*>(
      v8::External::Cast(*args.Data())->Value());
  client->delegate_->UndockWindow();
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsToggleInspectElementMode(
    const v8::Arguments& args) {
  WebDevToolsClientImpl* client = static_cast<WebDevToolsClientImpl*>(
      v8::External::Cast(*args.Data())->Value());
  int enabled = static_cast<int>(args[0]->BooleanValue());
  client->delegate_->ToggleInspectElementMode(enabled);
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsGetApplicationLocale(
    const v8::Arguments& args) {
  WebDevToolsClientImpl* client = static_cast<WebDevToolsClientImpl*>(
      v8::External::Cast(*args.Data())->Value());
  return v8String(client->application_locale_);
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsHiddenPanels(
    const v8::Arguments& args) {
  Page* page = V8Proxy::retrieveFrameForEnteredContext()->page();
  return v8String(page->settings()->databasesEnabled() ? "" : "databases");
}

// static
v8::Handle<v8::Value> WebDevToolsClientImpl::JsDebuggerCommand(
    const v8::Arguments& args) {
  WebDevToolsClientImpl* client = static_cast<WebDevToolsClientImpl*>(
      v8::External::Cast(*args.Data())->Value());
  String command = WebCore::toWebCoreStringWithNullCheck(args[0]);
  WebString std_command = webkit_glue::StringToWebString(command);
  client->delegate_->SendDebuggerCommandToAgent(std_command);
  return v8::Undefined();
}
