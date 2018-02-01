// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/picture_in_picture/DocumentPictureInPicture.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "modules/picture_in_picture/PictureInPictureController.h"

namespace blink {

namespace {

const char kNoPictureInPictureElement[] =
    "There is no Picture-in-Picture element in this document.";

}  // namespace

// static
bool DocumentPictureInPicture::pictureInPictureEnabled(Document& document) {
  return PictureInPictureController::Ensure(document).PictureInPictureEnabled();
}

// static
ScriptPromise DocumentPictureInPicture::exitPictureInPicture(
    ScriptState* script_state,
    Document& document) {
  if (!PictureInPictureController::Ensure(document).PictureInPictureElement()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kNoPictureInPictureElement));
  }

  // TODO(crbug.com/806249): Call element.exitPictureInPicture().
  // TODO(crbug.com/806249): Trigger leavepictureinpicture event.

  PictureInPictureController::Ensure(document).UnsetPictureInPictureElement();

  return ScriptPromise::CastUndefined(script_state);
}

// static
HTMLVideoElement* DocumentPictureInPicture::pictureInPictureElement(
    Document& document) {
  return PictureInPictureController::Ensure(document).PictureInPictureElement();
}

}  // namespace blink
