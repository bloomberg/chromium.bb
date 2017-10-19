// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerRegistration_h
#define ServiceWorkerRegistration_h

#include <memory>
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "modules/serviceworkers/NavigationPreloadManager.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/Supplementable.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/wtf/Forward.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistrationProxy.h"

namespace blink {

class ScriptPromise;
class ScriptState;

// The implementation of a service worker registration object in Blink. Actual
// registration representation is in the embedder and this class accesses it
// via WebServiceWorkerRegistration::Handle object.
class ServiceWorkerRegistration final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<ServiceWorkerRegistration>,
      public ContextLifecycleObserver,
      public WebServiceWorkerRegistrationProxy,
      public Supplementable<ServiceWorkerRegistration> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistration);
  USING_PRE_FINALIZER(ServiceWorkerRegistration, Dispose);

 public:
  // Called from CallbackPromiseAdapter.
  using WebType = std::unique_ptr<WebServiceWorkerRegistration::Handle>;
  static ServiceWorkerRegistration* Take(
      ScriptPromiseResolver*,
      std::unique_ptr<WebServiceWorkerRegistration::Handle>);

  // ScriptWrappable overrides.
  bool HasPendingActivity() const final;

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override {
    return ContextLifecycleObserver::GetExecutionContext();
  }

  // WebServiceWorkerRegistrationProxy overrides.
  void DispatchUpdateFoundEvent() override;
  void SetInstalling(std::unique_ptr<WebServiceWorker::Handle>) override;
  void SetWaiting(std::unique_ptr<WebServiceWorker::Handle>) override;
  void SetActive(std::unique_ptr<WebServiceWorker::Handle>) override;

  // Returns an existing registration object for the handle if it exists.
  // Otherwise, returns a new registration object.
  static ServiceWorkerRegistration* GetOrCreate(
      ExecutionContext*,
      std::unique_ptr<WebServiceWorkerRegistration::Handle>);

  ServiceWorker* installing() { return installing_; }
  ServiceWorker* waiting() { return waiting_; }
  ServiceWorker* active() { return active_; }
  NavigationPreloadManager* navigationPreload();

  String scope() const;

  WebServiceWorkerRegistration* WebRegistration() {
    return handle_->Registration();
  }

  ScriptPromise update(ScriptState*);
  ScriptPromise unregister(ScriptState*);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(updatefound);

  ~ServiceWorkerRegistration() override;

  virtual void Trace(blink::Visitor*);

 private:
  ServiceWorkerRegistration(
      ExecutionContext*,
      std::unique_ptr<WebServiceWorkerRegistration::Handle>);
  void Dispose();

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

  // A handle to the registration representation in the embedder.
  std::unique_ptr<WebServiceWorkerRegistration::Handle> handle_;

  Member<ServiceWorker> installing_;
  Member<ServiceWorker> waiting_;
  Member<ServiceWorker> active_;
  Member<NavigationPreloadManager> navigation_preload_;

  bool stopped_;
};

class ServiceWorkerRegistrationArray {
  STATIC_ONLY(ServiceWorkerRegistrationArray);

 public:
  // Called from CallbackPromiseAdapter.
  using WebType = std::unique_ptr<
      WebVector<std::unique_ptr<WebServiceWorkerRegistration::Handle>>>;
  static HeapVector<Member<ServiceWorkerRegistration>> Take(
      ScriptPromiseResolver* resolver,
      WebType web_service_worker_registrations) {
    HeapVector<Member<ServiceWorkerRegistration>> registrations;
    for (auto& registration : *web_service_worker_registrations) {
      registrations.push_back(
          ServiceWorkerRegistration::Take(resolver, std::move(registration)));
    }
    return registrations;
  }
};

}  // namespace blink

#endif  // ServiceWorkerRegistration_h
