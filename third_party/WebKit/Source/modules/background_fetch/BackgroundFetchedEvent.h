// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchedEvent_h
#define BackgroundFetchedEvent_h

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/ModulesExport.h"
#include "modules/background_fetch/BackgroundFetchEvent.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"
#include "public/platform/modules/background_fetch/background_fetch.mojom-blink.h"

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
  static BackgroundFetchedEvent* Create(
      const AtomicString& type,
      const BackgroundFetchedEventInit& initializer) {
    return new BackgroundFetchedEvent(type, initializer);
  }

  static BackgroundFetchedEvent* Create(
      const AtomicString& type,
      const BackgroundFetchedEventInit& initializer,
      const String& unique_id,
      const WebVector<WebBackgroundFetchSettledFetch>& fetches,
      ScriptState* script_state,
      WaitUntilObserver* observer,
      ServiceWorkerRegistration* registration) {
    return new BackgroundFetchedEvent(type, initializer, unique_id, fetches,
                                      script_state, observer, registration);
  }

  ~BackgroundFetchedEvent() override;

  // Web Exposed attribute defined in the IDL file.
  HeapVector<Member<BackgroundFetchSettledFetch>> fetches() const;

  // Web Exposed method defined in the IDL file.
  ScriptPromise updateUI(ScriptState*, const String& title);

  // ExtendableEvent interface.
  const AtomicString& InterfaceName() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  BackgroundFetchedEvent(const AtomicString& type,
                         const BackgroundFetchedEventInit&);
  BackgroundFetchedEvent(
      const AtomicString& type,
      const BackgroundFetchedEventInit&,
      const String& unique_id,
      const WebVector<WebBackgroundFetchSettledFetch>& fetches,
      ScriptState*,
      WaitUntilObserver*,
      ServiceWorkerRegistration*);

  void DidUpdateUI(ScriptPromiseResolver*, mojom::blink::BackgroundFetchError);

  // Globally unique ID for the registration, generated in content/. Used to
  // distinguish registrations in case a developer re-uses |developer_id_|s. Not
  // exposed to JavaScript.
  String unique_id_;

  HeapVector<Member<BackgroundFetchSettledFetch>> fetches_;
  Member<ServiceWorkerRegistration> registration_;
};

}  // namespace blink

#endif  // BackgroundFetchedEvent_h
