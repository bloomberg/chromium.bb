// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushMessagingBridge_h
#define PushMessagingBridge_h

#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/Supplementable.h"
#include "platform/heap/GarbageCollected.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"
#include "public/platform/modules/permissions/permission_status.mojom-blink.h"

namespace blink {

class PushSubscriptionOptionsInit;
class ScriptPromiseResolver;
class ScriptState;

// The bridge is responsible for establishing and maintaining the Mojo
// connection to the permission service. It's keyed on an active Service Worker
// Registration.
//
// TODO(peter): Use the PushMessaging Mojo service directly from here.
class PushMessagingBridge final
    : public GarbageCollectedFinalized<PushMessagingBridge>,
      public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(PushMessagingBridge);
  WTF_MAKE_NONCOPYABLE(PushMessagingBridge);

 public:
  static PushMessagingBridge* From(ServiceWorkerRegistration*);
  static const char* SupplementName();

  virtual ~PushMessagingBridge();

  // Asynchronously determines the permission state for the current origin.
  ScriptPromise GetPermissionState(ScriptState*,
                                   const PushSubscriptionOptionsInit&);

 private:
  explicit PushMessagingBridge(ServiceWorkerRegistration&);

  // Method to be invoked when the permission status has been retrieved from the
  // permission service. Will settle the given |resolver|.
  void DidGetPermissionState(ScriptPromiseResolver*,
                             mojom::blink::PermissionStatus);

  mojom::blink::PermissionServicePtr permission_service_;
};

}  // namespace blink

#endif  // PushMessagingBridge_h
