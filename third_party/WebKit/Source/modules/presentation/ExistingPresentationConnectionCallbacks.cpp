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
    : resolver_(resolver), connection_(connection) {
  DCHECK(resolver_);
  DCHECK(connection_);
}

void ExistingPresentationConnectionCallbacks::OnSuccess(
    const WebPresentationInfo& presentation_info) {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (connection_->GetState() == WebPresentationConnectionState::kClosed)
    connection_->DidChangeState(WebPresentationConnectionState::kConnecting);

  resolver_->Resolve(connection_);
}

void ExistingPresentationConnectionCallbacks::OnError(
    const WebPresentationError& error) {
  NOTREACHED();
}

WebPresentationConnection*
ExistingPresentationConnectionCallbacks::GetConnection() {
  return connection_.Get();
}

}  // namespace blink
