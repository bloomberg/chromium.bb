// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/mediastream/MediaDevices.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/mediastream/UserMediaController.h"

namespace blink {

ScriptPromise MediaDevices::enumerateDevices(ScriptState* scriptState)
{
    Document* document = toDocument(scriptState->executionContext());
    UserMediaController* userMedia = UserMediaController::from(document->frame());
    if (!userMedia)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "No media device controller available; is this a detached window?"));

    MediaDevicesRequest* request = MediaDevicesRequest::create(scriptState, userMedia);
    return request->start();
}

} // namespace blink
