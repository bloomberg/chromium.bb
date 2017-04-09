// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorProxyClientImpl_h
#define CompositorProxyClientImpl_h

#include "core/dom/CompositorProxyClient.h"

namespace blink {

// Registry for CompositorProxies on the control thread.
// Owned by the control thread.
class CompositorProxyClientImpl
    : public GarbageCollectedFinalized<CompositorProxyClientImpl>,
      public CompositorProxyClient {
  USING_GARBAGE_COLLECTED_MIXIN(CompositorProxyClientImpl);

 public:
  DECLARE_TRACE();

  // CompositorProxyClient:
  void RegisterCompositorProxy(CompositorProxy*) override;
  void UnregisterCompositorProxy(CompositorProxy*) override;

  HeapHashSet<WeakMember<CompositorProxy>>& Proxies() { return proxies_; }

 private:
  // CompositorProxy can be constructed in the main thread and control thread.
  // However this only ever contains ones constructed in the compositor worker
  // thread.
  HeapHashSet<WeakMember<CompositorProxy>> proxies_;
};

}  // namespace blink

#endif  // CompositorProxyClientImpl_h
