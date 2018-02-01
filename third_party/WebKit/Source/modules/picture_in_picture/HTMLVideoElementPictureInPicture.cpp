// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/picture_in_picture/HTMLVideoElementPictureInPicture.h"

#include "core/dom/DOMException.h"
#include "core/html/media/HTMLVideoElement.h"
#include "modules/picture_in_picture/PictureInPictureController.h"
#include "platform/feature_policy/FeaturePolicy.h"

namespace blink {

using Status = PictureInPictureController::Status;

namespace {

const char kDetachedError[] =
    "The element is no longer associated with a document.";
const char kFeaturePolicyBlocked[] =
    "Access to the feature \"picture-in-picture\" is disallowed by feature "
    "policy.";
const char kNotAvailable[] = "Picture-in-Picture is not available.";
const char kUserGestureRequired[] =
    "Must be handling a user gesture to request picture in picture.";

}  // namespace

// static
ScriptPromise HTMLVideoElementPictureInPicture::requestPictureInPicture(
    ScriptState* script_state,
    HTMLVideoElement& element) {
  Document& document = element.GetDocument();
  if (!document.GetFrame()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError, kDetachedError));
  }

  switch (PictureInPictureController::Ensure(document).GetStatus()) {
    case Status::kDisabledByFeaturePolicy:
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kSecurityError, kFeaturePolicyBlocked));
    case Status::kDisabledBySystem:
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kNotSupportedError, kNotAvailable));
    case Status::kEnabled:
      break;
  }

  LocalFrame* frame = element.GetFrame();
  if (!Frame::ConsumeTransientUserActivation(frame)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kNotAllowedError, kUserGestureRequired));
  }

  // TODO(crbug.com/806249): Call element.enterPictureInPicture().

  PictureInPictureController::Ensure(document).SetPictureInPictureElement(
      element);

  return ScriptPromise::CastUndefined(script_state);
}

}  // namespace blink
