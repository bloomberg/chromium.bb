// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/CompositorProxyClientImpl.h"

#include "core/dom/CompositorProxy.h"

namespace blink {

DEFINE_TRACE(CompositorProxyClientImpl) {
  visitor->trace(m_proxies);
  CompositorProxyClient::trace(visitor);
}

void CompositorProxyClientImpl::registerCompositorProxy(
    CompositorProxy* proxy) {
  DCHECK(!isMainThread());
  m_proxies.insert(proxy);
}

void CompositorProxyClientImpl::unregisterCompositorProxy(
    CompositorProxy* proxy) {
  DCHECK(!isMainThread());
  m_proxies.erase(proxy);
}

}  // namespace blink
