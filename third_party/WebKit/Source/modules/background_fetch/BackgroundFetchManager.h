// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchManager_h
#define BackgroundFetchManager_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/background_fetch/background_fetch.mojom-blink.h"

class SkBitmap;

namespace blink {

class BackgroundFetchBridge;
class BackgroundFetchIconLoader;
class BackgroundFetchOptions;
class BackgroundFetchRegistration;
class ExceptionState;
class ExecutionContext;
class RequestOrUSVStringOrRequestOrUSVStringSequence;
class ScriptPromiseResolver;
class ScriptState;
class ServiceWorkerRegistration;
class WebServiceWorkerRequest;

// Implementation of the BackgroundFetchManager JavaScript object, accessible
// by developers through ServiceWorkerRegistration.backgroundFetch.
class MODULES_EXPORT BackgroundFetchManager final
    : public ScriptWrappable,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(BackgroundFetchManager);
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~BackgroundFetchManager() override = default;
  static BackgroundFetchManager* Create(
      ServiceWorkerRegistration* registration) {
    return new BackgroundFetchManager(registration);
  }

  // Web Exposed methods defined in the IDL file.
  ScriptPromise fetch(
      ScriptState*,
      const String& id,
      const RequestOrUSVStringOrRequestOrUSVStringSequence& requests,
      const BackgroundFetchOptions&,
      ExceptionState&);
  ScriptPromise get(ScriptState*, const String& id);
  ScriptPromise getIds(ScriptState*);

  void Trace(blink::Visitor*);

  // ContextLifecycleObserver interface
  void ContextDestroyed(ExecutionContext*) override;

 private:
  friend class BackgroundFetchManagerTest;

  explicit BackgroundFetchManager(ServiceWorkerRegistration*);

  // Creates a vector of WebServiceWorkerRequest objects for the given set of
  // |requests|, which can be either Request objects or URL strings.
  static Vector<WebServiceWorkerRequest> CreateWebRequestVector(
      ScriptState*,
      const RequestOrUSVStringOrRequestOrUSVStringSequence& requests,
      ExceptionState&);

  void DidLoadIcons(const String&,
                    Vector<WebServiceWorkerRequest>,
                    mojom::blink::BackgroundFetchOptionsPtr,
                    ScriptPromiseResolver*,
                    const SkBitmap&);
  void DidFetch(ScriptPromiseResolver*,
                mojom::blink::BackgroundFetchError,
                BackgroundFetchRegistration*);
  void DidGetRegistration(ScriptPromiseResolver*,
                          mojom::blink::BackgroundFetchError,
                          BackgroundFetchRegistration*);
  void DidGetDeveloperIds(ScriptPromiseResolver*,
                          mojom::blink::BackgroundFetchError,
                          const Vector<String>& developer_ids);

  Member<ServiceWorkerRegistration> registration_;
  Member<BackgroundFetchBridge> bridge_;
  Member<BackgroundFetchIconLoader> loader_;
};

}  // namespace blink

#endif  // BackgroundFetchManager_h
