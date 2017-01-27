// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/ExistingPresentationConnectionCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationError.h"
#include "public/platform/modules/presentation/WebPresentationError.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

ExistingPresentationConnectionCallbacks::
    ExistingPresentationConnectionCallbacks(ScriptPromiseResolver* resolver,
                                            PresentationConnection* connection)
    : m_resolver(resolver), m_connection(connection) {
  DCHECK(m_resolver);
  DCHECK(m_connection);
}

void ExistingPresentationConnectionCallbacks::onSuccess(
    const WebPresentationSessionInfo& sessionInfo) {
  if (!m_resolver->getExecutionContext() ||
      m_resolver->getExecutionContext()->isContextDestroyed()) {
    return;
  }

  if (m_connection->getState() == WebPresentationConnectionState::Closed)
    m_connection->didChangeState(WebPresentationConnectionState::Connecting);

  m_resolver->resolve(m_connection);
}

void ExistingPresentationConnectionCallbacks::onError(
    const WebPresentationError& error) {
  NOTREACHED();
}

WebPresentationConnection*
ExistingPresentationConnectionCallbacks::getConnection() {
  return m_connection.get();
}

}  // namespace blink
