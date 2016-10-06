// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaSession.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "modules/mediasession/MediaMetadata.h"
#include "modules/mediasession/MediaMetadataSanitizer.h"
#include "public/platform/InterfaceProvider.h"
#include <memory>

namespace blink {

MediaSession::MediaSession() = default;

MediaSession* MediaSession::create() {
  return new MediaSession();
}

void MediaSession::setMetadata(ScriptState* scriptState,
                               MediaMetadata* metadata) {
  if (getService(scriptState)) {
    getService(scriptState)
        ->SetMetadata(
            MediaMetadataSanitizer::sanitizeAndConvertToMojo(metadata));
  }
}

MediaMetadata* MediaSession::metadata(ScriptState*) const {
  return m_metadata;
}

mojom::blink::MediaSessionService* MediaSession::getService(
    ScriptState* scriptState) {
  if (!m_service) {
    InterfaceProvider* interfaceProvider = nullptr;
    DCHECK(scriptState->getExecutionContext()->isDocument())
        << "MediaSession::getService() is only available from a frame";
    Document* document = toDocument(scriptState->getExecutionContext());
    if (document->frame())
      interfaceProvider = document->frame()->interfaceProvider();

    if (interfaceProvider)
      interfaceProvider->getInterface(mojo::GetProxy(&m_service));
  }
  return m_service.get();
}

DEFINE_TRACE(MediaSession) {
  visitor->trace(m_metadata);
}

}  // namespace blink
