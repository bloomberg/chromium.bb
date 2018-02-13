// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/picture_in_picture/HTMLVideoElementPictureInPicture.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/events/Event.h"
#include "core/html/media/HTMLVideoElement.h"
#include "modules/picture_in_picture/PictureInPictureController.h"
#include "modules/picture_in_picture/PictureInPictureWindow.h"
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
  switch (
      PictureInPictureController::Ensure(document).IsElementAllowed(element)) {
    case Status::kFrameDetached:
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kInvalidStateError, kDetachedError));
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

  // Frame is not null, otherwise `IsElementAllowed()` would have return
  // `kFrameDetached`.
  LocalFrame* frame = element.GetFrame();
  DCHECK(frame);
  if (!Frame::ConsumeTransientUserActivation(frame)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kNotAllowedError, kUserGestureRequired));
  }

  // TODO(crbug.com/806249): Call element.enterPictureInPicture().

  // TODO(crbug.com/806249): Don't use fake width and height.
  PictureInPictureWindow* window =
      PictureInPictureController::Ensure(document).CreatePictureInPictureWindow(
          500 /* width */, 300 /* height */);

  PictureInPictureController::Ensure(document).SetPictureInPictureElement(
      element);

  element.DispatchEvent(
      Event::CreateBubble(EventTypeNames::enterpictureinpicture));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  resolver->Resolve(window);

  return promise;
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
  TreeScope& scope = element.GetTreeScope();
  if (PictureInPictureController::Ensure(document).PictureInPictureElement(
          scope) == &element) {
    // TODO(crbug.com/806249): Call element.exitPictureInPicture().

    PictureInPictureController::Ensure(document)
        .OnClosePictureInPictureWindow();

    PictureInPictureController::Ensure(document).UnsetPictureInPictureElement();

    element.DispatchEvent(
        Event::CreateBubble(EventTypeNames::leavepictureinpicture));
  }
}

}  // namespace blink
