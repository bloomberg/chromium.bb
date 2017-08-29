// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerGlobalScopeBackgroundFetch_h
#define ServiceWorkerGlobalScopeBackgroundFetch_h

#include "core/dom/events/EventTarget.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ServiceWorkerGlobalScopeBackgroundFetch {
  STATIC_ONLY(ServiceWorkerGlobalScopeBackgroundFetch);

 public:
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(backgroundfetched);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(backgroundfetchfail);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(backgroundfetchabort);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(backgroundfetchclick);
};

}  // namespace blink

#endif  // ServiceWorkerGlobalScopeBackgroundFetch_h
