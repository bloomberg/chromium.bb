/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WorkerGlobalScope_h
#define WorkerGlobalScope_h

#include <memory>
#include "bindings/core/v8/V8CacheOptions.h"
#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMTimerCoordinator.h"
#include "core/frame/DOMWindowBase64.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerEventQueue.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkerSettings.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/CachedMetadataHandler.h"
#include "platform/wtf/ListHashSet.h"

namespace blink {

class ConsoleMessage;
class ExceptionState;
class V8AbstractEventListener;
class WorkerLocation;
class WorkerNavigator;
class WorkerThread;

class CORE_EXPORT WorkerGlobalScope
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<WorkerGlobalScope>,
      public SecurityContext,
      public WorkerOrWorkletGlobalScope,
      public Supplementable<WorkerGlobalScope>,
      public DOMWindowBase64 {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(WorkerGlobalScope);

 public:
  using SecurityContext::GetSecurityOrigin;
  using SecurityContext::GetContentSecurityPolicy;

  ~WorkerGlobalScope() override;

  // Returns null if caching is not supported.
  virtual CachedMetadataHandler* CreateWorkerScriptCachedMetadataHandler(
      const KURL& script_url,
      const Vector<char>* meta_data) {
    return nullptr;
  }

  KURL CompleteURL(const String&) const;

  // WorkerOrWorkletGlobalScope
  bool IsClosing() const final { return closing_; }
  virtual void Dispose();
  void ReportFeature(WebFeature) final;
  void ReportDeprecation(WebFeature) final;
  WorkerThread* GetThread() const final { return thread_; }

  void ExceptionUnhandled(int exception_id);

  void RegisterEventListener(V8AbstractEventListener*);
  void DeregisterEventListener(V8AbstractEventListener*);

  // WorkerGlobalScope
  WorkerGlobalScope* self() { return this; }
  WorkerLocation* location() const;
  WorkerNavigator* navigator() const;
  void close();
  bool isSecureContextForBindings() const {
    return ExecutionContext::IsSecureContext();
  }

  String origin() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(rejectionhandled);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(unhandledrejection);

  // WorkerUtils
  virtual void importScripts(const Vector<String>& urls, ExceptionState&);

  // ScriptWrappable
  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) final;
  v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) final;

  // ScriptWrappable
  bool HasPendingActivity() const override;

  // ExecutionContext
  bool IsWorkerGlobalScope() const final { return true; }
  bool IsContextThread() const final;
  String UserAgent() const final { return user_agent_; }

  DOMTimerCoordinator* Timers() final { return &timers_; }
  SecurityContext& GetSecurityContext() final { return *this; }
  void AddConsoleMessage(ConsoleMessage*) final;
  WorkerEventQueue* GetEventQueue() const final;
  bool IsSecureContext(String& error_message) const override;

  CoreProbeSink* GetProbeSink() final;

  // EventTarget
  ExecutionContext* GetExecutionContext() const final;

  // WorkerOrWorkletGlobalScope
  ScriptWrappable* GetScriptWrappable() const final {
    return const_cast<WorkerGlobalScope*>(this);
  }

  double TimeOrigin() const { return time_origin_; }
  WorkerSettings* GetWorkerSettings() const { return worker_settings_.get(); }

  DECLARE_VIRTUAL_TRACE();

 protected:
  WorkerGlobalScope(const KURL&,
                    const String& user_agent,
                    WorkerThread*,
                    double time_origin,
                    std::unique_ptr<SecurityOrigin::PrivilegeData>,
                    WorkerClients*);
  void SetWorkerSettings(std::unique_ptr<WorkerSettings>);
  void ApplyContentSecurityPolicyFromHeaders(
      const ContentSecurityPolicyResponseHeaders&);
  void ApplyContentSecurityPolicyFromVector(
      const Vector<CSPHeaderAndType>& headers);

  void SetV8CacheOptions(V8CacheOptions v8_cache_options) {
    v8_cache_options_ = v8_cache_options;
  }

  // ExecutionContext
  void ExceptionThrown(ErrorEvent*) override;
  void RemoveURLFromMemoryCache(const KURL&) final;

 private:
  // |kNotHandled| is used when the script was not in
  // InstalledScriptsManager, which means either it was not an installed script
  // or it was already taken.
  enum class LoadResult { kSuccess, kFailed, kNotHandled };

  // Tries to load the script synchronously from the
  // InstalledScriptsManager, which holds scripts that are sent from the browser
  // upon starting an installed worker. This blocks until the script is
  // received. If the script load could not be handled by the
  // InstalledScriptsManager, e.g. when the script was not an installed script,
  // returns LoadResult::kNotHandled.
  // TODO(crbug.com/753350): Factor out LoadingScriptFrom* into a new class
  // which provides the worker's scripts.
  LoadResult LoadingScriptFromInstalledScriptsManager(
      const KURL& script_url,
      KURL* out_response_url,
      String* out_source_code,
      std::unique_ptr<Vector<char>>* out_cached_meta_data);
  // Tries to load the script synchronously from the WorkerScriptLoader, which
  // requests the script from the browser. This
  // blocks until the script is received.
  LoadResult LoadingScriptFromWorkerScriptLoader(
      const KURL& script_url,
      KURL* out_response_url,
      String* out_source_code,
      std::unique_ptr<Vector<char>>* out_cached_meta_data);

  // ExecutionContext
  EventTarget* ErrorEventTarget() final { return this; }
  const KURL& VirtualURL() const final { return url_; }
  KURL VirtualCompleteURL(const String&) const final;

  // SecurityContext
  void DidUpdateSecurityOrigin() final {}

  const KURL url_;
  const String user_agent_;
  V8CacheOptions v8_cache_options_;
  std::unique_ptr<WorkerSettings> worker_settings_;

  mutable Member<WorkerLocation> location_;
  mutable Member<WorkerNavigator> navigator_;

  WorkerThread* thread_;

  bool closing_ = false;

  Member<WorkerEventQueue> event_queue_;

  DOMTimerCoordinator timers_;

  const double time_origin_;

  HeapHashSet<Member<V8AbstractEventListener>> event_listeners_;

  HeapHashMap<int, Member<ErrorEvent>> pending_error_events_;
  int last_pending_error_event_id_ = 0;
};

DEFINE_TYPE_CASTS(WorkerGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsWorkerGlobalScope(),
                  context.IsWorkerGlobalScope());

}  // namespace blink

#endif  // WorkerGlobalScope_h
