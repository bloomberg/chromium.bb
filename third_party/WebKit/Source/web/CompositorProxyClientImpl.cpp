// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/CompositorProxyClientImpl.h"

#include "core/dom/CompositorProxy.h"

namespace blink {

DEFINE_TRACE(CompositorProxyClientImpl) {
  visitor->Trace(proxies_);
  CompositorProxyClient::Trace(visitor);
}

void CompositorProxyClientImpl::RegisterCompositorProxy(
    CompositorProxy* proxy) {
  DCHECK(!IsMainThread());
  proxies_.insert(proxy);
}

void CompositorProxyClientImpl::UnregisterCompositorProxy(
    CompositorProxy* proxy) {
  DCHECK(!IsMainThread());
  proxies_.erase(proxy);
}

}  // namespace blink
