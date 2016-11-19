// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SyncManager_h
#define SyncManager_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BackgroundSyncProvider;
class ExecutionContext;
class ScriptPromise;
class ScriptState;
class ServiceWorkerRegistration;

class SyncManager final : public GarbageCollected<SyncManager>,
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

  static BackgroundSyncProvider* backgroundSyncProvider();

  Member<ServiceWorkerRegistration> m_registration;
};

}  // namespace blink

#endif  // SyncManager_h
