// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationConnectionCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationError.h"
#include "modules/presentation/PresentationRequest.h"
#include "public/platform/modules/presentation/WebPresentationError.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

PresentationConnectionCallbacks::PresentationConnectionCallbacks(
    ScriptPromiseResolver* resolver,
    PresentationRequest* request)
    : m_resolver(resolver), m_request(request), m_connection(nullptr) {
  ASSERT(m_resolver);
  ASSERT(m_request);
}

void PresentationConnectionCallbacks::onSuccess(
    const WebPresentationSessionInfo& sessionInfo) {
  if (!m_resolver->getExecutionContext() ||
      m_resolver->getExecutionContext()->isContextDestroyed()) {
    return;
  }

  m_connection =
      PresentationConnection::take(m_resolver.get(), sessionInfo, m_request);
  m_resolver->resolve(m_connection);
}

void PresentationConnectionCallbacks::onError(
    const WebPresentationError& error) {
  if (!m_resolver->getExecutionContext() ||
      m_resolver->getExecutionContext()->isContextDestroyed()) {
    return;
  }
  m_resolver->reject(PresentationError::take(error));
  m_connection = nullptr;
}

WebPresentationConnection* PresentationConnectionCallbacks::getConnection() {
  return m_connection ? m_connection.get() : nullptr;
}

}  // namespace blink
