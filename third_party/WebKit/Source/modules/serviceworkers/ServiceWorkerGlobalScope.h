/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#ifndef ServiceWorkerGlobalScope_h
#define ServiceWorkerGlobalScope_h

#include <memory>
#include "bindings/modules/v8/RequestOrUSVString.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Forward.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"

namespace blink {

class Dictionary;
class RespondWithObserver;
class ScriptPromise;
class ScriptState;
class ServiceWorkerClients;
class ServiceWorkerRegistration;
class ServiceWorkerThread;
class WaitUntilObserver;
class WorkerThreadStartupData;

typedef RequestOrUSVString RequestInfo;

class MODULES_EXPORT ServiceWorkerGlobalScope final : public WorkerGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ServiceWorkerGlobalScope* Create(
      ServiceWorkerThread*,
      std::unique_ptr<WorkerThreadStartupData>);

  ~ServiceWorkerGlobalScope() override;
  bool IsServiceWorkerGlobalScope() const override { return true; }

  // Counts an evaluated script and its size. Called for each of the main
  // worker script and imported scripts.
  void CountScript(size_t script_size, size_t cached_metadata_size);

  // Called when the main worker script is evaluated.
  void DidEvaluateWorkerScript();

  // ServiceWorkerGlobalScope.idl
  ServiceWorkerClients* clients();
  ServiceWorkerRegistration* registration();

  ScriptPromise fetch(ScriptState*,
                      const RequestInfo&,
                      const Dictionary&,
                      ExceptionState&);

  ScriptPromise skipWaiting(ScriptState*);

  void SetRegistration(std::unique_ptr<WebServiceWorkerRegistration::Handle>);

  // EventTarget
  const AtomicString& InterfaceName() const override;

  void DispatchExtendableEvent(Event*, WaitUntilObserver*);

  // For ExtendableEvents that also have a respondWith() function.
  void DispatchExtendableEventWithRespondWith(Event*,
                                              WaitUntilObserver*,
                                              RespondWithObserver*);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(install);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(activate);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(fetch);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(foreignfetch);

  DECLARE_VIRTUAL_TRACE();

 protected:
  // EventTarget
  bool AddEventListenerInternal(
      const AtomicString& event_type,
      EventListener*,
      const AddEventListenerOptionsResolved&) override;

 private:
  ServiceWorkerGlobalScope(const KURL&,
                           const String& user_agent,
                           ServiceWorkerThread*,
                           double time_origin,
                           std::unique_ptr<SecurityOrigin::PrivilegeData>,
                           WorkerClients*);
  void importScripts(const Vector<String>& urls, ExceptionState&) override;
  CachedMetadataHandler* CreateWorkerScriptCachedMetadataHandler(
      const KURL& script_url,
      const Vector<char>* meta_data) override;
  void ExceptionThrown(ErrorEvent*) override;

  Member<ServiceWorkerClients> clients_;
  Member<ServiceWorkerRegistration> registration_;
  bool did_evaluate_script_;
  size_t script_count_;
  size_t script_total_size_;
  size_t script_cached_metadata_total_size_;
};

DEFINE_TYPE_CASTS(ServiceWorkerGlobalScope,
                  ExecutionContext,
                  context,
                  context->IsServiceWorkerGlobalScope(),
                  context.IsServiceWorkerGlobalScope());

}  // namespace blink

#endif  // ServiceWorkerGlobalScope_h
