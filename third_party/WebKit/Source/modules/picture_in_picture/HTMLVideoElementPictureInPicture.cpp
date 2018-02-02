// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/picture_in_picture/HTMLVideoElementPictureInPicture.h"

#include "core/dom/DOMException.h"
#include "core/dom/events/Event.h"
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
const char kDisablePictureInPicturePresent[] =
    "\"disablePictureInPicture\" attribute is present.";
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

  switch (
      PictureInPictureController::Ensure(document).IsElementAllowed(element)) {
    case Status::kDisabledByFeaturePolicy:
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kSecurityError, kFeaturePolicyBlocked));
    case Status::kDisabledByAttribute:
      return ScriptPromise::RejectWithDOMException(
          script_state, DOMException::Create(kInvalidStateError,
                                             kDisablePictureInPicturePresent));
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

  element.DispatchEvent(
      Event::CreateBubble(EventTypeNames::enterpictureinpicture));

  return ScriptPromise::CastUndefined(script_state);
}

// static
bool HTMLVideoElementPictureInPicture::FastHasAttribute(
    const QualifiedName& name,
    const HTMLVideoElement& element) {
  DCHECK(name == HTMLNames::disablepictureinpictureAttr);
  return element.FastHasAttribute(name);
}

// static
void HTMLVideoElementPictureInPicture::SetBooleanAttribute(
    const QualifiedName& name,
    HTMLVideoElement& element,
    bool value) {
  DCHECK(name == HTMLNames::disablepictureinpictureAttr);
  element.SetBooleanAttribute(name, value);

  if (!value)
    return;

  // TODO(crbug.com/806249): Reject pending PiP requests.

  Document& document = element.GetDocument();
  if (PictureInPictureController::Ensure(document).PictureInPictureElement() ==
      &element) {
    // TODO(crbug.com/806249): Call element.exitPictureInPicture().
    // TODO(crbug.com/806249): Trigger leavepictureinpicture event.

    PictureInPictureController::Ensure(document).UnsetPictureInPictureElement();
  }
}

}  // namespace blink
