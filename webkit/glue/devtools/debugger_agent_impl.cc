// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#include "Document.h"
#include "Frame.h"
#include "Page.h"
#include "V8Binding.h"
#include "V8DOMWindow.h"
#include "V8Index.h"
#include "V8Proxy.h"
#undef LOG

#include "grit/webkit_resources.h"
#include "third_party/WebKit/WebKit/chromium/src/WebViewImpl.h"
#include "webkit/glue/devtools/debugger_agent_impl.h"
#include "webkit/glue/devtools/debugger_agent_manager.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdevtoolsagent_impl.h"
#include "webkit/glue/webkit_glue.h"

using WebCore::DOMWindow;
using WebCore::Document;
using WebCore::Frame;
using WebCore::Page;
using WebCore::String;
using WebCore::V8ClassIndex;
using WebCore::V8Custom;
using WebCore::V8DOMWindow;
using WebCore::V8DOMWrapper;
using WebCore::V8Proxy;
using WebKit::WebViewImpl;

DebuggerAgentImpl::DebuggerAgentImpl(
    WebViewImpl* web_view_impl,
    DebuggerAgentDelegate* delegate,
    WebDevToolsAgentImpl* webdevtools_agent)
    : web_view_impl_(web_view_impl),
      delegate_(delegate),
      webdevtools_agent_(webdevtools_agent),
      auto_continue_on_exception_(false) {
  DebuggerAgentManager::DebugAttach(this);
}

DebuggerAgentImpl::~DebuggerAgentImpl() {
  DebuggerAgentManager::DebugDetach(this);
}

void DebuggerAgentImpl::GetContextId() {
  delegate_->SetContextId(webdevtools_agent_->host_id());
}

void DebuggerAgentImpl::DebuggerOutput(const String& command) {
  delegate_->DebuggerOutput(command);
  webdevtools_agent_->ForceRepaint();
}

// static
void DebuggerAgentImpl::CreateUtilityContext(
    Frame* frame,
    v8::Persistent<v8::Context>* context) {
  v8::HandleScope scope;

  // TODO(pfeldman): Validate against Soeren.
  // Set up the DOM window as the prototype of the new global object.
  v8::Handle<v8::Context> window_context =
      V8Proxy::context(frame);
  v8::Handle<v8::Object> window_global = window_context->Global();
  v8::Handle<v8::Object> window_wrapper =
      V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, window_global);

  ASSERT(V8DOMWrapper::convertDOMWrapperToNative<DOMWindow>(window_wrapper) ==
      frame->domWindow());

  v8::Handle<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();

  // TODO(yurys): provide a function in v8 bindings that would make the
  // utility context more like main world context of the inspected frame,
  // otherwise we need to manually make it satisfy various invariants
  // that V8Proxy::getEntered and some other V8Proxy methods expect to find
  // on v8 contexts on the contexts stack.
  // See V8Proxy::createNewContext.
  //
  // Install a security handler with V8.
  global_template->SetAccessCheckCallbacks(
      V8DOMWindow::namedSecurityCheck,
      V8DOMWindow::indexedSecurityCheck,
      v8::Integer::New(V8ClassIndex::DOMWINDOW));
  // We set number of internal fields to match that in V8DOMWindow wrapper.
  // See http://crbug.com/28961
  global_template->SetInternalFieldCount(
      V8Custom::kDOMWindowInternalFieldCount);

  *context = v8::Context::New(
      NULL /* no extensions */,
      global_template,
      v8::Handle<v8::Object>());
  v8::Context::Scope context_scope(*context);
  v8::Handle<v8::Object> global = (*context)->Global();

  v8::Handle<v8::String> implicit_proto_string = v8::String::New("__proto__");
  global->Set(implicit_proto_string, window_wrapper);

  // Give the code running in the new context a way to get access to the
  // original context.
  global->Set(v8::String::New("contentWindow"), window_global);

  // Inject javascript into the context.
  base::StringPiece injectjs_webkit =
      webkit_glue::GetDataResource(IDR_DEVTOOLS_INJECT_WEBKIT_JS);
  v8::Script::Compile(
      v8::String::New(injectjs_webkit.as_string().c_str()))->Run();
  base::StringPiece inject_dispatchjs = webkit_glue::GetDataResource(
      IDR_DEVTOOLS_INJECT_DISPATCH_JS);
  v8::Script::Compile(v8::String::New(
      inject_dispatchjs.as_string().c_str()))->Run();
}

String DebuggerAgentImpl::ExecuteUtilityFunction(
    v8::Handle<v8::Context> context,
    int call_id,
    const char* object,
    const String &function_name,
    const String& json_args,
    bool async,
    String* exception) {
  v8::HandleScope scope;
  ASSERT(!context.IsEmpty());
  if (context.IsEmpty()) {
    *exception = "No window context.";
    return "";
  }
  v8::Context::Scope context_scope(context);

  DebuggerAgentManager::UtilityContextScope utility_scope;

  v8::Handle<v8::Object> dispatch_object = v8::Handle<v8::Object>::Cast(
      context->Global()->Get(v8::String::New(object)));

  v8::Handle<v8::Value> dispatch_function =
      dispatch_object->Get(v8::String::New("dispatch"));
  ASSERT(dispatch_function->IsFunction());
  v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(dispatch_function);

  v8::Handle<v8::String> function_name_wrapper = v8::Handle<v8::String>(
      v8::String::New(function_name.utf8().data()));
  v8::Handle<v8::String> json_args_wrapper = v8::Handle<v8::String>(
      v8::String::New(json_args.utf8().data()));
  v8::Handle<v8::Number> call_id_wrapper = v8::Handle<v8::Number>(
      v8::Number::New(async ? call_id : 0));

  v8::Handle<v8::Value> args[] = {
    function_name_wrapper,
    json_args_wrapper,
    call_id_wrapper
  };

  v8::TryCatch try_catch;
  v8::Handle<v8::Value> res_obj = function->Call(context->Global(), 3, args);
  if (try_catch.HasCaught()) {
    v8::Local<v8::Message> message = try_catch.Message();
    if (message.IsEmpty())
      *exception = "Unknown exception";
    else
      *exception = WebCore::toWebCoreString(message->Get());
    return "";
  } else {
    return WebCore::toWebCoreStringWithNullCheck(res_obj);
  }
}

void DebuggerAgentImpl::ExecuteVoidJavaScript(v8::Handle<v8::Context> context) {
  v8::HandleScope scope;
  ASSERT(!context.IsEmpty());
  v8::Context::Scope context_scope(context);
  DebuggerAgentManager::UtilityContextScope utility_scope;

  v8::Handle<v8::Value> function =
      context->Global()->Get(v8::String::New("devtools$$void"));
  ASSERT(function->IsFunction());
  v8::Handle<v8::Value> args[] = {
    v8::Local<v8::Value>()
  };
  v8::Handle<v8::Function>::Cast(function)->Call(context->Global(), 0, args);
}

WebCore::Page* DebuggerAgentImpl::GetPage() {
  return web_view_impl_->page();
}
