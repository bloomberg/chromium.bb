// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchManager_h
#define BackgroundFetchManager_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"

namespace blink {

class BackgroundFetchOptions;
class RequestOrUSVStringOrRequestOrUSVStringSequence;
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
      String tag,
      const RequestOrUSVStringOrRequestOrUSVStringSequence& requests,
      const BackgroundFetchOptions&);
  ScriptPromise get(ScriptState*, String tag);
  ScriptPromise getTags(ScriptState*);

  DECLARE_TRACE();

 private:
  explicit BackgroundFetchManager(ServiceWorkerRegistration*);

  Member<ServiceWorkerRegistration> m_registration;
};

}  // namespace blink

#endif  // BackgroundFetchManager_h
