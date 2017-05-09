// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NetworkInstrumentation_h
#define NetworkInstrumentation_h

#include "platform/PlatformExport.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"

namespace blink {

class ResourceRequest;

namespace network_instrumentation {

enum RequestOutcome { kSuccess, kFail };

class PLATFORM_EXPORT ScopedResourceLoadTracker {
 public:
  ScopedResourceLoadTracker(unsigned long resource_id,
                            const blink::ResourceRequest&);
  ~ScopedResourceLoadTracker();
  void ResourceLoadContinuesBeyondScope();

 private:
  // If this variable is false, close resource load slice at end of scope.
  bool resource_load_continues_beyond_scope_;

  const unsigned long resource_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedResourceLoadTracker);
};

void PLATFORM_EXPORT ResourcePrioritySet(unsigned long resource_id,
                                         blink::ResourceLoadPriority);

void PLATFORM_EXPORT EndResourceLoad(unsigned long resource_id, RequestOutcome);

}  // namespace network_instrumentation
}  // namespace blink

#endif  // NetworkInstrumentation_h
