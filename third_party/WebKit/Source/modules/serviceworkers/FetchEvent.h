// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchEvent_h
#define FetchEvent_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/Request.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "modules/serviceworkers/FetchEventInit.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class FetchRespondWithObserver;
class Request;
class Response;
class ScriptState;
class WebDataConsumerHandle;
struct WebServiceWorkerError;
class WebURLResponse;

// A fetch event is dispatched by the client to a service worker's script
// context. FetchRespondWithObserver can be used to notify the client about the
// service worker's response.
class MODULES_EXPORT FetchEvent final : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();

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

  const AtomicString& InterfaceName() const override;

  DECLARE_VIRTUAL_TRACE();

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
  String client_id_;
  bool is_reload_;
};

}  // namespace blink

#endif  // FetchEvent_h
