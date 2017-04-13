/*
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/LocalWindowProxy.h"

#include "bindings/core/v8/ConditionalFeaturesForCore.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8GCForContextDispose.h"
#include "bindings/core/v8/V8HTMLDocument.h"
#include "bindings/core/v8/V8Initializer.h"
#include "bindings/core/v8/V8PagePopupControllerBinding.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "bindings/core/v8/V8Window.h"
#include "core/dom/Modulator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/DocumentNameCollection.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/FrameLoader.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "platform/wtf/Assertions.h"
#include "v8/include/v8.h"

namespace blink {

void LocalWindowProxy::DisposeContext(Lifecycle next_status) {
  DCHECK(next_status == Lifecycle::kGlobalObjectIsDetached ||
         next_status == Lifecycle::kFrameIsDetached);

  if (lifecycle_ != Lifecycle::kContextIsInitialized)
    return;

  ScriptState::Scope scope(script_state_.Get());
  v8::Local<v8::Context> context = script_state_->GetContext();
  // The embedder could run arbitrary code in response to the
  // willReleaseScriptContext callback, so all disposing should happen after
  // it returns.
  GetFrame()->Loader().Client()->WillReleaseScriptContext(context,
                                                          world_->GetWorldId());
  MainThreadDebugger::Instance()->ContextWillBeDestroyed(script_state_.Get());

  if (next_status == Lifecycle::kGlobalObjectIsDetached) {
    v8::Local<v8::Context> context = script_state_->GetContext();
    // Clean up state on the global proxy, which will be reused.
    if (!global_proxy_.IsEmpty()) {
      CHECK(global_proxy_ == context->Global());
      CHECK_EQ(ToScriptWrappable(context->Global()),
               ToScriptWrappable(
                   context->Global()->GetPrototype().As<v8::Object>()));
      global_proxy_.Get().SetWrapperClassId(0);
    }
    V8DOMWrapper::ClearNativeInfo(GetIsolate(), context->Global());
    script_state_->DetachGlobalObject();

#if DCHECK_IS_ON()
    DidDetachGlobalObject();
#endif
  }

  script_state_->DisposePerContextData();

  // It's likely that disposing the context has created a lot of
  // garbage. Notify V8 about this so it'll have a chance of cleaning
  // it up when idle.
  V8GCForContextDispose::Instance().NotifyContextDisposed(
      GetFrame()->IsMainFrame());

  if (next_status == Lifecycle::kFrameIsDetached) {
    // The context's frame is detached from the DOM, so there shouldn't be a
    // strong reference to the context.
    global_proxy_.SetPhantom();
  }

  DCHECK_EQ(lifecycle_, Lifecycle::kContextIsInitialized);
  lifecycle_ = next_status;
}

void LocalWindowProxy::Initialize() {
  TRACE_EVENT1("v8", "LocalWindowProxy::initialize", "isMainWindow",
               GetFrame()->IsMainFrame());
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER(
      GetFrame()->IsMainFrame()
          ? "Blink.Binding.InitializeMainLocalWindowProxy"
          : "Blink.Binding.InitializeNonMainLocalWindowProxy");

  ScriptForbiddenScope::AllowUserAgentScript allow_script;

  v8::HandleScope handle_scope(GetIsolate());

  CreateContext();

  ScriptState::Scope scope(script_state_.Get());
  v8::Local<v8::Context> context = script_state_->GetContext();
  if (global_proxy_.IsEmpty()) {
    global_proxy_.Set(GetIsolate(), context->Global());
    CHECK(!global_proxy_.IsEmpty());
  }

  SetupWindowPrototypeChain();

  SecurityOrigin* origin = 0;
  if (world_->IsMainWorld()) {
    // ActivityLogger for main world is updated within updateDocumentInternal().
    UpdateDocumentInternal();
    origin = GetFrame()->GetDocument()->GetSecurityOrigin();
    // FIXME: Can this be removed when CSP moves to browser?
    ContentSecurityPolicy* csp =
        GetFrame()->GetDocument()->GetContentSecurityPolicy();
    context->AllowCodeGenerationFromStrings(csp->AllowEval(
        0, SecurityViolationReportingPolicy::kSuppressReporting));
    context->SetErrorMessageForCodeGenerationFromStrings(
        V8String(GetIsolate(), csp->EvalDisabledErrorMessage()));
  } else {
    UpdateActivityLogger();
    origin = world_->IsolatedWorldSecurityOrigin();
    SetSecurityToken(origin);
  }

  MainThreadDebugger::Instance()->ContextCreated(script_state_.Get(),
                                                 GetFrame(), origin);
  GetFrame()->Loader().Client()->DidCreateScriptContext(context,
                                                        world_->GetWorldId());
  // If conditional features for window have been queued before the V8 context
  // was ready, then inject them into the context now
  if (world_->IsMainWorld()) {
    InstallConditionalFeaturesOnWindow(script_state_.Get());
  }

  if (world_->IsMainWorld())
    GetFrame()->Loader().DispatchDidClearWindowObjectInMainWorld();
}

void LocalWindowProxy::CreateContext() {
  // Create a new v8::Context with the window object as the global object
  // (aka the inner global). Reuse the outer global proxy if it already exists.
  v8::Local<v8::ObjectTemplate> global_template =
      V8Window::domTemplate(GetIsolate(), *world_)->InstanceTemplate();
  CHECK(!global_template.IsEmpty());

  Vector<const char*> extension_names;
  // Dynamically tell v8 about our extensions now.
  if (GetFrame()->Loader().Client()->AllowScriptExtensions()) {
    const V8Extensions& extensions = ScriptController::RegisteredExtensions();
    extension_names.ReserveInitialCapacity(extensions.size());
    for (const auto* extension : extensions)
      extension_names.push_back(extension->name());
  }
  v8::ExtensionConfiguration extension_configuration(extension_names.size(),
                                                     extension_names.Data());

  v8::Local<v8::Context> context;
  {
    V8PerIsolateData::UseCounterDisabledScope use_counter_disabled(
        V8PerIsolateData::From(GetIsolate()));
    context =
        v8::Context::New(GetIsolate(), &extension_configuration,
                         global_template, global_proxy_.NewLocal(GetIsolate()));
  }
  CHECK(!context.IsEmpty());

#if DCHECK_IS_ON()
  DidAttachGlobalObject();
#endif

  script_state_ = ScriptState::Create(context, world_);

  DCHECK(lifecycle_ == Lifecycle::kContextIsUninitialized ||
         lifecycle_ == Lifecycle::kGlobalObjectIsDetached);
  lifecycle_ = Lifecycle::kContextIsInitialized;
  DCHECK(script_state_->ContextIsValid());
}

void LocalWindowProxy::SetupWindowPrototypeChain() {
  // Associate the window wrapper object and its prototype chain with the
  // corresponding native DOMWindow object.
  DOMWindow* window = GetFrame()->DomWindow();
  const WrapperTypeInfo* wrapper_type_info = window->GetWrapperTypeInfo();
  v8::Local<v8::Context> context = script_state_->GetContext();

  // The global proxy object.  Note this is not the global object.
  v8::Local<v8::Object> global_proxy = context->Global();
  CHECK(global_proxy_ == global_proxy);
  V8DOMWrapper::SetNativeInfo(GetIsolate(), global_proxy, wrapper_type_info,
                              window);
  // Mark the handle to be traced by Oilpan, since the global proxy has a
  // reference to the DOMWindow.
  global_proxy_.Get().SetWrapperClassId(wrapper_type_info->wrapper_class_id);

  // The global object, aka window wrapper object.
  v8::Local<v8::Object> window_wrapper =
      global_proxy->GetPrototype().As<v8::Object>();
  v8::Local<v8::Object> associated_wrapper =
      AssociateWithWrapper(window, wrapper_type_info, window_wrapper);
  DCHECK(associated_wrapper == window_wrapper);

  // The prototype object of Window interface.
  v8::Local<v8::Object> window_prototype =
      window_wrapper->GetPrototype().As<v8::Object>();
  CHECK(!window_prototype.IsEmpty());
  V8DOMWrapper::SetNativeInfo(GetIsolate(), window_prototype, wrapper_type_info,
                              window);

  // The named properties object of Window interface.
  v8::Local<v8::Object> window_properties =
      window_prototype->GetPrototype().As<v8::Object>();
  CHECK(!window_properties.IsEmpty());
  V8DOMWrapper::SetNativeInfo(GetIsolate(), window_properties,
                              wrapper_type_info, window);

  // TODO(keishi): Remove installPagePopupController and implement
  // PagePopupController in another way.
  V8PagePopupControllerBinding::InstallPagePopupController(context,
                                                           window_wrapper);
}

void LocalWindowProxy::UpdateDocumentProperty() {
  DCHECK(world_->IsMainWorld());

  ScriptState::Scope scope(script_state_.Get());
  v8::Local<v8::Context> context = script_state_->GetContext();
  v8::Local<v8::Value> document_wrapper =
      ToV8(GetFrame()->GetDocument(), context->Global(), GetIsolate());
  DCHECK(document_wrapper->IsObject());
  // Update the cached accessor for window.document.
  CHECK(V8PrivateProperty::GetWindowDocumentCachedAccessor(GetIsolate())
            .Set(context->Global(), document_wrapper));
}

void LocalWindowProxy::UpdateActivityLogger() {
  script_state_->PerContextData()->SetActivityLogger(
      V8DOMActivityLogger::ActivityLogger(
          world_->GetWorldId(), GetFrame()->GetDocument()
                                    ? GetFrame()->GetDocument()->baseURI()
                                    : KURL()));
}

void LocalWindowProxy::SetSecurityToken(SecurityOrigin* origin) {
  // If two tokens are equal, then the SecurityOrigins canAccess each other.
  // If two tokens are not equal, then we have to call canAccess.
  // Note: we can't use the HTTPOrigin if it was set from the DOM.
  String token;
  // If document.domain is modified, v8 needs to do a full canAccess check,
  // so always use an empty security token in that case.
  bool delay_set = world_->IsMainWorld() && origin->DomainWasSetInDOM();
  if (origin && !delay_set)
    token = origin->ToString();

  // An empty or "null" token means we always have to call
  // canAccess. The toString method on securityOrigins returns the
  // string "null" for empty security origins and for security
  // origins that should only allow access to themselves. In this
  // case, we use the global object as the security token to avoid
  // calling canAccess when a script accesses its own objects.
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> context = script_state_->GetContext();
  if (token.IsEmpty() || token == "null") {
    context->UseDefaultSecurityToken();
    return;
  }

  if (world_->IsIsolatedWorld()) {
    SecurityOrigin* frame_security_origin =
        GetFrame()->GetDocument()->GetSecurityOrigin();
    String frame_security_token = frame_security_origin->ToString();
    // We need to check the return value of domainWasSetInDOM() on the
    // frame's SecurityOrigin because, if that's the case, only
    // SecurityOrigin::m_domain would have been modified.
    // m_domain is not used by SecurityOrigin::toString(), so we would end
    // up generating the same token that was already set.
    if (frame_security_origin->DomainWasSetInDOM() ||
        frame_security_token.IsEmpty() || frame_security_token == "null") {
      context->UseDefaultSecurityToken();
      return;
    }
    token = frame_security_token + token;
  }

  // NOTE: V8 does identity comparison in fast path, must use a symbol
  // as the security token.
  context->SetSecurityToken(V8AtomicString(GetIsolate(), token));
}

void LocalWindowProxy::UpdateDocument() {
  DCHECK(world_->IsMainWorld());
  // For an uninitialized main window proxy, there's nothing we need
  // to update. The update is done when the window proxy gets initialized later.
  if (lifecycle_ == Lifecycle::kContextIsUninitialized)
    return;

  // For a navigated-away window proxy, reinitialize it as a new window with new
  // context and document.
  if (lifecycle_ == Lifecycle::kGlobalObjectIsDetached) {
    Initialize();
    DCHECK_EQ(Lifecycle::kContextIsInitialized, lifecycle_);
    // Initialization internally updates the document properties, so just
    // return afterwards.
    return;
  }

  UpdateDocumentInternal();
}

void LocalWindowProxy::UpdateDocumentInternal() {
  UpdateActivityLogger();
  UpdateDocumentProperty();
  UpdateSecurityOrigin(GetFrame()->GetDocument()->GetSecurityOrigin());
}

static v8::Local<v8::Value> GetNamedProperty(
    HTMLDocument* html_document,
    const AtomicString& key,
    v8::Local<v8::Object> creation_context,
    v8::Isolate* isolate) {
  if (!html_document->HasNamedItem(key) &&
      !html_document->HasExtraNamedItem(key))
    return V8Undefined();

  DocumentNameCollection* items = html_document->DocumentNamedItems(key);
  if (items->IsEmpty())
    return V8Undefined();

  if (items->HasExactlyOneItem()) {
    HTMLElement* element = items->Item(0);
    DCHECK(element);
    Frame* frame = isHTMLIFrameElement(*element)
                       ? toHTMLIFrameElement(*element).ContentFrame()
                       : 0;
    if (frame)
      return ToV8(frame->DomWindow(), creation_context, isolate);
    return ToV8(element, creation_context, isolate);
  }
  return ToV8(items, creation_context, isolate);
}

static void Getter(v8::Local<v8::Name> property,
                   const v8::PropertyCallbackInfo<v8::Value>& info) {
  if (!property->IsString())
    return;
  // FIXME: Consider passing StringImpl directly.
  AtomicString name = ToCoreAtomicString(property.As<v8::String>());
  HTMLDocument* html_document = V8HTMLDocument::toImpl(info.Holder());
  DCHECK(html_document);
  v8::Local<v8::Value> result =
      GetNamedProperty(html_document, name, info.Holder(), info.GetIsolate());
  if (!result.IsEmpty()) {
    V8SetReturnValue(info, result);
    return;
  }
  v8::Local<v8::Value> value;
  if (info.Holder()
          ->GetRealNamedPropertyInPrototypeChain(
              info.GetIsolate()->GetCurrentContext(), property.As<v8::String>())
          .ToLocal(&value))
    V8SetReturnValue(info, value);
}

void LocalWindowProxy::NamedItemAdded(HTMLDocument* document,
                                      const AtomicString& name) {
  DCHECK(world_->IsMainWorld());

  // Currently only contexts in attached frames can change the named items.
  // TODO(yukishiino): Support detached frame's case, too, since the spec is not
  // saying that the document needs to be attached to the DOM.
  // https://html.spec.whatwg.org/C/dom.html#dom-document-nameditem
  DCHECK(lifecycle_ == Lifecycle::kContextIsInitialized);
  // TODO(yukishiino): Remove the following if-clause due to the above DCHECK.
  if (lifecycle_ != Lifecycle::kContextIsInitialized)
    return;

  ScriptState::Scope scope(script_state_.Get());
  v8::Local<v8::Object> document_wrapper =
      world_->DomDataStore().Get(document, GetIsolate());
  document_wrapper
      ->SetAccessor(GetIsolate()->GetCurrentContext(),
                    V8String(GetIsolate(), name), Getter)
      .ToChecked();
}

void LocalWindowProxy::NamedItemRemoved(HTMLDocument* document,
                                        const AtomicString& name) {
  DCHECK(world_->IsMainWorld());

  // Currently only contexts in attached frames can change the named items.
  // TODO(yukishiino): Support detached frame's case, too, since the spec is not
  // saying that the document needs to be attached to the DOM.
  // https://html.spec.whatwg.org/C/dom.html#dom-document-nameditem
  DCHECK(lifecycle_ == Lifecycle::kContextIsInitialized);
  // TODO(yukishiino): Remove the following if-clause due to the above DCHECK.
  if (lifecycle_ != Lifecycle::kContextIsInitialized)
    return;

  if (document->HasNamedItem(name) || document->HasExtraNamedItem(name))
    return;
  ScriptState::Scope scope(script_state_.Get());
  v8::Local<v8::Object> document_wrapper =
      world_->DomDataStore().Get(document, GetIsolate());
  document_wrapper
      ->Delete(GetIsolate()->GetCurrentContext(), V8String(GetIsolate(), name))
      .ToChecked();
}

void LocalWindowProxy::UpdateSecurityOrigin(SecurityOrigin* origin) {
  // For an uninitialized window proxy, there's nothing we need to update. The
  // update is done when the window proxy gets initialized later.
  if (lifecycle_ == Lifecycle::kContextIsUninitialized ||
      lifecycle_ == Lifecycle::kGlobalObjectIsDetached)
    return;

  SetSecurityToken(origin);
}

LocalWindowProxy::LocalWindowProxy(v8::Isolate* isolate,
                                   LocalFrame& frame,
                                   RefPtr<DOMWrapperWorld> world)
    : WindowProxy(isolate, frame, std::move(world)) {}

}  // namespace blink
