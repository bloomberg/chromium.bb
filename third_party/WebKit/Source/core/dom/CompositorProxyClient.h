// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorProxyClient_h
#define CompositorProxyClient_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class CompositorProxy;

class CORE_EXPORT CompositorProxyClient
    : public GarbageCollectedFinalized<CompositorProxyClient> {
 public:
  virtual ~CompositorProxyClient(){};
  DEFINE_INLINE_VIRTUAL_TRACE() {}

  virtual void registerCompositorProxy(CompositorProxy*) = 0;
  // It is not guaranteed to receive an unregister call for every registered
  // proxy. In fact we only receive one when a proxy is explicitly
  // disconnected otherwise we rely on oilpan collection process to remove the
  // weak reference to the proxy.
  virtual void unregisterCompositorProxy(CompositorProxy*) = 0;
};

}  // namespace blink

#endif  // CompositorProxyClient_h
