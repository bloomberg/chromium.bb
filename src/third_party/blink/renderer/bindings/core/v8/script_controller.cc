/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Opera Software ASA. All rights reserved.
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

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

void ScriptController::Trace(Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(window_proxy_manager_);
}

void ScriptController::ClearForClose() {
  window_proxy_manager_->ClearForClose();
  MainThreadDebugger::Instance()->DidClearContextsForFrame(GetFrame());
}

void ScriptController::UpdateSecurityOrigin(
    const SecurityOrigin* security_origin) {
  window_proxy_manager_->UpdateSecurityOrigin(security_origin);
}

v8::Local<v8::Value> ScriptController::ExecuteScriptAndReturnValue(
    v8::Local<v8::Context> context,
    const ScriptSourceCode& source,
    const KURL& base_url,
    SanitizeScriptErrors sanitize_script_errors,
    const ScriptFetchOptions& fetch_options) {
  TRACE_EVENT1(
      "devtools.timeline", "EvaluateScript", "data",
      inspector_evaluate_script_event::Data(
          GetFrame(), source.Url().GetString(), source.StartPosition()));
  v8::Local<v8::Value> result;
  {
    V8CacheOptions v8_cache_options = kV8CacheOptionsDefault;
    if (const Settings* settings = GetFrame()->GetSettings())
      v8_cache_options = settings->GetV8CacheOptions();

    // Isolate exceptions that occur when compiling and executing
    // the code. These exceptions should not interfere with
    // javascript code we might evaluate from C++ when returning
    // from here.
    v8::TryCatch try_catch(GetIsolate());
    try_catch.SetVerbose(true);

    // Omit storing base URL if it is same as source URL.
    // Note: This improves chance of getting into a fast path in
    //       ReferrerScriptInfo::ToV8HostDefinedOptions.
    KURL stored_base_url = (base_url == source.Url()) ? KURL() : base_url;
    const ReferrerScriptInfo referrer_info(stored_base_url, fetch_options);

    v8::Local<v8::Script> script;

    v8::ScriptCompiler::CompileOptions compile_options;
    V8CodeCache::ProduceCacheOptions produce_cache_options;
    v8::ScriptCompiler::NoCacheReason no_cache_reason;
    std::tie(compile_options, produce_cache_options, no_cache_reason) =
        V8CodeCache::GetCompileOptions(v8_cache_options, source);
    if (!V8ScriptRunner::CompileScript(ScriptState::From(context), source,
                                       sanitize_script_errors, compile_options,
                                       no_cache_reason, referrer_info)
             .ToLocal(&script))
      return result;

    v8::MaybeLocal<v8::Value> maybe_result;
    maybe_result = V8ScriptRunner::RunCompiledScript(GetIsolate(), script,
                                                     GetFrame()->DomWindow());
    probe::ProduceCompilationCache(frame_, source, script);
    V8CodeCache::ProduceCache(GetIsolate(), script, source,
                              produce_cache_options);

    if (!maybe_result.ToLocal(&result)) {
      return result;
    }
  }

  return result;
}

TextPosition ScriptController::EventHandlerPosition() const {
  ScriptableDocumentParser* parser =
      GetFrame()->GetDocument()->GetScriptableDocumentParser();
  if (parser)
    return parser->GetTextPosition();
  return TextPosition::MinimumPosition();
}

void ScriptController::EnableEval() {
  SetEvalForWorld(DOMWrapperWorld::MainWorld(), true /* allow_eval */,
                  g_empty_string /* error_message */);
}

void ScriptController::DisableEval(const String& error_message) {
  SetEvalForWorld(DOMWrapperWorld::MainWorld(), false /* allow_eval */,
                  error_message);
}

void ScriptController::DisableEvalForIsolatedWorld(
    int32_t world_id,
    const String& error_message) {
  DCHECK(DOMWrapperWorld::IsIsolatedWorldId(world_id));
  scoped_refptr<DOMWrapperWorld> world =
      DOMWrapperWorld::EnsureIsolatedWorld(GetIsolate(), world_id);
  SetEvalForWorld(*world, false /* allow_eval */, error_message);
}

void ScriptController::SetEvalForWorld(DOMWrapperWorld& world,
                                       bool allow_eval,
                                       const String& error_message) {
  v8::HandleScope handle_scope(GetIsolate());
  LocalWindowProxy* proxy =
      world.IsMainWorld()
          ? window_proxy_manager_->MainWorldProxyMaybeUninitialized()
          : WindowProxy(world);

  v8::Local<v8::Context> v8_context = proxy->ContextIfInitialized();
  if (v8_context.IsEmpty())
    return;

  v8_context->AllowCodeGenerationFromStrings(allow_eval);
  if (allow_eval)
    return;

  v8_context->SetErrorMessageForCodeGenerationFromStrings(
      V8String(GetIsolate(), error_message));
}

namespace {

Vector<const char*>& RegisteredExtensionNames() {
  DEFINE_STATIC_LOCAL(Vector<const char*>, extension_names, ());
  return extension_names;
}

}  // namespace

void ScriptController::RegisterExtensionIfNeeded(
    std::unique_ptr<v8::Extension> extension) {
  for (const auto* extension_name : RegisteredExtensionNames()) {
    if (!strcmp(extension_name, extension->name()))
      return;
  }
  RegisteredExtensionNames().push_back(extension->name());
  v8::RegisterExtension(std::move(extension));
}

v8::ExtensionConfiguration ScriptController::ExtensionsFor(
    const ExecutionContext* context) {
  if (context->ShouldInstallV8Extensions()) {
    return v8::ExtensionConfiguration(RegisteredExtensionNames().size(),
                                      RegisteredExtensionNames().data());
  }
  return v8::ExtensionConfiguration();
}

void ScriptController::ClearWindowProxy() {
  // V8 binding expects ScriptController::clearWindowProxy only be called when a
  // frame is loading a new page. This creates a new context for the new page.
  window_proxy_manager_->ClearForNavigation();
  MainThreadDebugger::Instance()->DidClearContextsForFrame(GetFrame());
}

void ScriptController::UpdateDocument() {
  window_proxy_manager_->MainWorldProxyMaybeUninitialized()->UpdateDocument();
}

void ScriptController::ExecuteJavaScriptURL(
    const KURL& url,
    network::mojom::CSPDisposition check_main_world_csp) {
  DCHECK(url.ProtocolIsJavaScript());

  const int kJavascriptSchemeLength = sizeof("javascript:") - 1;
  String script_source = DecodeURLEscapeSequences(
      url.GetString(), DecodeURLMode::kUTF8OrIsomorphic);

  bool should_bypass_main_world_content_security_policy =
      check_main_world_csp == network::mojom::CSPDisposition::DO_NOT_CHECK ||
      ContentSecurityPolicy::ShouldBypassMainWorld(GetFrame()->DomWindow());
  if (!GetFrame()->GetPage())
    return;

  if (!should_bypass_main_world_content_security_policy &&
      !GetFrame()->GetDocument()->GetContentSecurityPolicy()->AllowInline(
          ContentSecurityPolicy::InlineType::kNavigation, nullptr,
          script_source, String() /* nonce */, GetFrame()->GetDocument()->Url(),
          EventHandlerPosition().line_)) {
    return;
  }

  script_source = script_source.Substring(kJavascriptSchemeLength);
  if (!should_bypass_main_world_content_security_policy) {
    script_source = TrustedTypesCheckForJavascriptURLinNavigation(
        script_source, GetFrame()->DomWindow());
    if (script_source.IsEmpty())
      return;
  }

  bool had_navigation_before = GetFrame()->Loader().HasProvisionalNavigation();

  v8::HandleScope handle_scope(GetIsolate());

  // https://html.spec.whatwg.org/C/#navigate
  // Step 12.8 "Let base URL be settings object's API base URL." [spec text]
  KURL base_url = GetFrame()->GetDocument()->BaseURL();

  // Step 12.9 "Let script be result of creating a classic script given script
  // source, settings, base URL, and the default classic script fetch options."
  // [spec text]
  // We pass |SanitizeScriptErrors::kDoNotSanitize| because |muted errors| is
  // false by default.
  v8::Local<v8::Value> v8_result = EvaluateScriptInMainWorld(
      ScriptSourceCode(script_source, ScriptSourceLocationType::kJavascriptUrl),
      base_url, SanitizeScriptErrors::kDoNotSanitize, ScriptFetchOptions(),
      kDoNotExecuteScriptWhenScriptsDisabled);
  UseCounter::Count(*GetFrame()->GetDocument(),
                    WebFeature::kExecutedJavaScriptURL);

  // If executing script caused this frame to be removed from the page, we
  // don't want to try to replace its document!
  if (!GetFrame()->GetPage())
    return;
  // If a navigation begins during the javascript: url's execution, ignore
  // the return value of the script. Otherwise, replacing the document with a
  // string result would cancel the navigation.
  if (!had_navigation_before && GetFrame()->Loader().HasProvisionalNavigation())
    return;
  if (v8_result.IsEmpty() || !v8_result->IsString())
    return;

  UseCounter::Count(*GetFrame()->GetDocument(),
                    WebFeature::kReplaceDocumentViaJavaScriptURL);
  auto params = std::make_unique<WebNavigationParams>();
  params->url = GetFrame()->GetDocument()->Url();
  if (auto* owner = GetFrame()->Owner())
    params->frame_policy = owner->GetFramePolicy();

  String result = ToCoreString(v8::Local<v8::String>::Cast(v8_result));
  WebNavigationParams::FillStaticResponse(params.get(), "text/html", "UTF-8",
                                          StringUTF8Adaptor(result));
  GetFrame()->Loader().CommitNavigation(std::move(params), nullptr,
                                        CommitReason::kJavascriptUrl);
}

void ScriptController::ExecuteScriptInMainWorld(
    const String& script,
    ScriptSourceLocationType source_location_type,
    ExecuteScriptPolicy policy) {
  v8::HandleScope handle_scope(GetIsolate());
  EvaluateScriptInMainWorld(ScriptSourceCode(script, source_location_type),
                            KURL(), SanitizeScriptErrors::kSanitize,
                            ScriptFetchOptions(), policy);
}

void ScriptController::ExecuteScriptInMainWorld(
    const ScriptSourceCode& source_code,
    const KURL& base_url,
    SanitizeScriptErrors sanitize_script_errors,
    const ScriptFetchOptions& fetch_options) {
  v8::HandleScope handle_scope(GetIsolate());
  EvaluateScriptInMainWorld(source_code, base_url, sanitize_script_errors,
                            fetch_options,
                            kDoNotExecuteScriptWhenScriptsDisabled);
}

v8::Local<v8::Value> ScriptController::ExecuteScriptInMainWorldAndReturnValue(
    const ScriptSourceCode& source_code,
    const KURL& base_url,
    SanitizeScriptErrors sanitize_script_errors,
    const ScriptFetchOptions& fetch_options,
    ExecuteScriptPolicy policy) {
  return EvaluateScriptInMainWorld(
      source_code, base_url, sanitize_script_errors, fetch_options, policy);
}

v8::Local<v8::Value> ScriptController::EvaluateScriptInMainWorld(
    const ScriptSourceCode& source_code,
    const KURL& base_url,
    SanitizeScriptErrors sanitize_script_errors,
    const ScriptFetchOptions& fetch_options,
    ExecuteScriptPolicy policy) {
  if (policy == kDoNotExecuteScriptWhenScriptsDisabled &&
      !GetFrame()->GetDocument()->CanExecuteScripts(kAboutToExecuteScript))
    return v8::Local<v8::Value>();

  // |context| should be initialized already due to the MainWorldProxy() call.
  v8::Local<v8::Context> context =
      window_proxy_manager_->MainWorldProxy()->ContextIfInitialized();

  v8::Context::Scope scope(context);
  v8::EscapableHandleScope handle_scope(GetIsolate());

  if (GetFrame()->Loader().StateMachine()->IsDisplayingInitialEmptyDocument())
    GetFrame()->Loader().DidAccessInitialDocument();

  v8::Local<v8::Value> object = ExecuteScriptAndReturnValue(
      context, source_code, base_url, sanitize_script_errors, fetch_options);

  if (object.IsEmpty())
    return v8::Local<v8::Value>();

  return handle_scope.Escape(object);
}

v8::Local<v8::Value> ScriptController::ExecuteScriptInIsolatedWorld(
    int32_t world_id,
    const ScriptSourceCode& source,
    const KURL& base_url,
    SanitizeScriptErrors sanitize_script_errors) {
  DCHECK_GT(world_id, 0);

  scoped_refptr<DOMWrapperWorld> world =
      DOMWrapperWorld::EnsureIsolatedWorld(GetIsolate(), world_id);
  LocalWindowProxy* isolated_world_window_proxy = WindowProxy(*world);
  // TODO(dcheng): Context must always be initialized here, due to the call to
  // windowProxy() on the previous line. Add a helper which makes that obvious?
  v8::Local<v8::Context> context =
      isolated_world_window_proxy->ContextIfInitialized();
  v8::Context::Scope scope(context);

  v8::Local<v8::Value> evaluation_result = ExecuteScriptAndReturnValue(
      context, source, base_url, sanitize_script_errors);
  if (!evaluation_result.IsEmpty())
    return evaluation_result;
  return v8::Local<v8::Value>::New(GetIsolate(), v8::Undefined(GetIsolate()));
}

scoped_refptr<DOMWrapperWorld>
ScriptController::CreateNewInspectorIsolatedWorld(const String& world_name) {
  scoped_refptr<DOMWrapperWorld> world = DOMWrapperWorld::Create(
      GetIsolate(), DOMWrapperWorld::WorldType::kInspectorIsolated);
  // Bail out if we could not create an isolated world.
  if (!world)
    return nullptr;
  if (!world_name.IsEmpty()) {
    DOMWrapperWorld::SetNonMainWorldHumanReadableName(world->GetWorldId(),
                                                      world_name);
  }
  // Make sure the execution context exists.
  WindowProxy(*world);
  return world;
}

}  // namespace blink
