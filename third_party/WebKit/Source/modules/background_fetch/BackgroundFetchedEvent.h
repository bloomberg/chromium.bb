// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchedEvent_h
#define BackgroundFetchedEvent_h

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/ModulesExport.h"
#include "modules/background_fetch/BackgroundFetchEvent.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/background_fetch/background_fetch.mojom-blink.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class BackgroundFetchSettledFetch;
class BackgroundFetchedEventInit;
class ScriptState;
class ServiceWorkerRegistration;
struct WebBackgroundFetchSettledFetch;

class MODULES_EXPORT BackgroundFetchedEvent final
    : public BackgroundFetchEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BackgroundFetchedEvent* create(
      const AtomicString& type,
      const BackgroundFetchedEventInit& initializer) {
    return new BackgroundFetchedEvent(type, initializer);
  }

  static BackgroundFetchedEvent* create(
      const AtomicString& type,
      const BackgroundFetchedEventInit& initializer,
      const WebVector<WebBackgroundFetchSettledFetch>& fetches,
      ScriptState* scriptState,
      WaitUntilObserver* observer,
      ServiceWorkerRegistration* registration) {
    return new BackgroundFetchedEvent(type, initializer, fetches, scriptState,
                                      observer, registration);
  }

  ~BackgroundFetchedEvent() override;

  // Web Exposed attribute defined in the IDL file.
  HeapVector<Member<BackgroundFetchSettledFetch>> fetches() const;

  // Web Exposed method defined in the IDL file.
  ScriptPromise updateUI(ScriptState*, const String& title);

  // ExtendableEvent interface.
  const AtomicString& interfaceName() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  BackgroundFetchedEvent(const AtomicString& type,
                         const BackgroundFetchedEventInit&);
  BackgroundFetchedEvent(
      const AtomicString& type,
      const BackgroundFetchedEventInit&,
      const WebVector<WebBackgroundFetchSettledFetch>& fetches,
      ScriptState*,
      WaitUntilObserver*,
      ServiceWorkerRegistration*);

  void didUpdateUI(ScriptPromiseResolver*, mojom::blink::BackgroundFetchError);

  HeapVector<Member<BackgroundFetchSettledFetch>> m_fetches;
  Member<ServiceWorkerRegistration> m_registration;
};

}  // namespace blink

#endif  // BackgroundFetchedEvent_h
