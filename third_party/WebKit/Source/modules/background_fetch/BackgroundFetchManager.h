// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchManager_h
#define BackgroundFetchManager_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/background_fetch/background_fetch.mojom-blink.h"

namespace blink {

class BackgroundFetchBridge;
class BackgroundFetchOptions;
class BackgroundFetchRegistration;
class RequestOrUSVStringOrRequestOrUSVStringSequence;
class ScriptPromiseResolver;
class ScriptState;
class ServiceWorkerRegistration;

// Implementation of the BackgroundFetchManager JavaScript object, accessible
// by developers through ServiceWorkerRegistration.backgroundFetch.
class BackgroundFetchManager final
    : public GarbageCollected<BackgroundFetchManager>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BackgroundFetchManager* create(
      ServiceWorkerRegistration* registration) {
    return new BackgroundFetchManager(registration);
  }

  // Web Exposed methods defined in the IDL file.
  ScriptPromise fetch(
      ScriptState*,
      const String& tag,
      const RequestOrUSVStringOrRequestOrUSVStringSequence& requests,
      const BackgroundFetchOptions&);
  ScriptPromise get(ScriptState*, const String& tag);
  ScriptPromise getTags(ScriptState*);

  DECLARE_TRACE();

 private:
  explicit BackgroundFetchManager(ServiceWorkerRegistration*);

  void didGetRegistration(ScriptPromiseResolver*,
                          mojom::blink::BackgroundFetchError,
                          BackgroundFetchRegistration*);
  void didGetTags(ScriptPromiseResolver*,
                  mojom::blink::BackgroundFetchError,
                  const Vector<String>& tags);

  Member<ServiceWorkerRegistration> m_registration;
  Member<BackgroundFetchBridge> m_bridge;
};

}  // namespace blink

#endif  // BackgroundFetchManager_h
