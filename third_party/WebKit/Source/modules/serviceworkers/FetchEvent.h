// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchEvent_h
#define FetchEvent_h

#include <memory>

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/Request.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "modules/serviceworkers/FetchEventInit.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceResponse.h"

namespace blink {

class ExceptionState;
class FetchRespondWithObserver;
class Request;
class Response;
class ScriptState;
class WebDataConsumerHandle;
struct WebServiceWorkerError;
class WebURLResponse;
class WorkerGlobalScope;

// A fetch event is dispatched by the client to a service worker's script
// context. FetchRespondWithObserver can be used to notify the client about the
// service worker's response.
class MODULES_EXPORT FetchEvent final
    : public ExtendableEvent,
      public ActiveScriptWrappable<FetchEvent>,
      public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(FetchEvent);

 public:
  using PreloadResponseProperty = ScriptPromiseProperty<Member<FetchEvent>,
                                                        Member<Response>,
                                                        Member<DOMException>>;
  static FetchEvent* Create(ScriptState*,
                            const AtomicString& type,
                            const FetchEventInit&);
  static FetchEvent* Create(ScriptState*,
                            const AtomicString& type,
                            const FetchEventInit&,
                            FetchRespondWithObserver*,
                            WaitUntilObserver*,
                            bool navigation_preload_sent);

  ~FetchEvent() override;

  Request* request() const;
  String clientId() const;
  bool isReload() const;

  void respondWith(ScriptState*, ScriptPromise, ExceptionState&);
  ScriptPromise preloadResponse(ScriptState*);

  void OnNavigationPreloadResponse(ScriptState*,
                                   std::unique_ptr<WebURLResponse>,
                                   std::unique_ptr<WebDataConsumerHandle>);
  void OnNavigationPreloadError(ScriptState*,
                                std::unique_ptr<WebServiceWorkerError>);
  void OnNavigationPreloadComplete(WorkerGlobalScope*,
                                   double completion_time,
                                   int64_t encoded_data_length,
                                   int64_t encoded_body_length,
                                   int64_t decoded_body_length);

  const AtomicString& InterfaceName() const override;

  // ScriptWrappable
  bool HasPendingActivity() const override;

  virtual void Trace(blink::Visitor*);

 protected:
  FetchEvent(ScriptState*,
             const AtomicString& type,
             const FetchEventInit&,
             FetchRespondWithObserver*,
             WaitUntilObserver*,
             bool navigation_preload_sent);

 private:
  Member<FetchRespondWithObserver> observer_;
  Member<Request> request_;
  Member<PreloadResponseProperty> preload_response_property_;
  std::unique_ptr<WebURLResponse> preload_response_;
  String client_id_;
  bool is_reload_;
};

}  // namespace blink

#endif  // FetchEvent_h
