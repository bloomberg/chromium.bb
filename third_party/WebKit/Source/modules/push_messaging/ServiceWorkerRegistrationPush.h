// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerRegistrationPush_h
#define ServiceWorkerRegistrationPush_h

#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class PushManager;
class ServiceWorkerRegistration;

class ServiceWorkerRegistrationPush final
    : public GarbageCollectedFinalized<ServiceWorkerRegistrationPush>,
      public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistrationPush);
  WTF_MAKE_NONCOPYABLE(ServiceWorkerRegistrationPush);

 public:
  virtual ~ServiceWorkerRegistrationPush();
  static ServiceWorkerRegistrationPush& From(ServiceWorkerRegistration&);

  static PushManager* pushManager(ServiceWorkerRegistration&);
  PushManager* pushManager();

  virtual void Trace(blink::Visitor*);

 private:
  explicit ServiceWorkerRegistrationPush(ServiceWorkerRegistration*);
  static const char* SupplementName();

  Member<ServiceWorkerRegistration> registration_;
  Member<PushManager> push_manager_;
};

}  // namespace blink

#endif  // ServiceWorkerRegistrationPush_h
