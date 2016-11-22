// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SyncManager_h
#define SyncManager_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/background_sync/background_sync.mojom-blink.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class ServiceWorkerRegistration;

class SyncManager final : public GarbageCollectedFinalized<SyncManager>,
                          public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SyncManager* create(ServiceWorkerRegistration* registration) {
    return new SyncManager(registration);
  }

  ScriptPromise registerFunction(ScriptState*, const String& tag);
  ScriptPromise getTags(ScriptState*);

  DECLARE_TRACE();

  enum { kUnregisteredSyncID = -1 };

 private:
  explicit SyncManager(ServiceWorkerRegistration*);

  // Returns an initialized BackgroundSyncServicePtr. A connection with the
  // the browser's BackgroundSyncService is created the first time this method
  // is called.
  const mojom::blink::BackgroundSyncServicePtr& getBackgroundSyncServicePtr();

  // Callbacks
  static void registerCallback(ScriptPromiseResolver*,
                               mojom::blink::BackgroundSyncError,
                               mojom::blink::SyncRegistrationPtr options);
  static void getRegistrationsCallback(
      ScriptPromiseResolver*,
      mojom::blink::BackgroundSyncError,
      WTF::Vector<mojom::blink::SyncRegistrationPtr> registrations);

  Member<ServiceWorkerRegistration> m_registration;
  mojom::blink::BackgroundSyncServicePtr m_backgroundSyncService;
};

}  // namespace blink

#endif  // SyncManager_h
