// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/navigator_display_media.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/mediastream/media_devices.h"
#include "third_party/blink/renderer/modules/mediastream/navigator_user_media.h"

namespace blink {

ScriptPromise NavigatorDisplayMedia::getDisplayMedia(
    ScriptState* script_state,
    Navigator& navigator,
    const MediaStreamConstraints& options,
    ExceptionState& exception_state) {
  MediaDevices* const media_devices =
      NavigatorUserMedia::mediaDevices(navigator);
  if (!media_devices) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                           "Current frame is detached."));
  }

  return media_devices->SendUserMediaRequest(
      script_state, WebUserMediaRequest::MediaType::kDisplayMedia, options,
      exception_state);
}

}  // namespace blink
