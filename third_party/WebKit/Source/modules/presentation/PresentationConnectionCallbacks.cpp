// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationConnectionCallbacks.h"

#include <memory>
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationError.h"
#include "modules/presentation/PresentationRequest.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/modules/presentation/WebPresentationError.h"

namespace blink {

PresentationConnectionCallbacks::PresentationConnectionCallbacks(
    ScriptPromiseResolver* resolver,
    PresentationRequest* request)
    : resolver_(resolver), request_(request), connection_(nullptr) {
  DCHECK(resolver_);
  DCHECK(request_);
}

PresentationConnectionCallbacks::PresentationConnectionCallbacks(
    ScriptPromiseResolver* resolver,
    PresentationConnection* connection)
    : resolver_(resolver), request_(nullptr), connection_(connection) {
  DCHECK(resolver_);
  DCHECK(connection_);
}

void PresentationConnectionCallbacks::OnSuccess(
    const WebPresentationInfo& presentation_info) {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  // Reconnect to existing connection.
  if (connection_ &&
      connection_->GetState() == WebPresentationConnectionState::kClosed) {
    connection_->DidChangeState(WebPresentationConnectionState::kConnecting);
  }

  // Create a new connection.
  if (!connection_ && request_) {
    connection_ = PresentationConnection::Take(resolver_.Get(),
                                               presentation_info, request_);
  }
  resolver_->Resolve(connection_);
}

void PresentationConnectionCallbacks::OnError(
    const WebPresentationError& error) {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }
  resolver_->Reject(PresentationError::Take(error));
  connection_ = nullptr;
}

WebPresentationConnection* PresentationConnectionCallbacks::GetConnection() {
  return connection_ ? connection_.Get() : nullptr;
}

}  // namespace blink
