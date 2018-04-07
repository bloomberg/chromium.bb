// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/html_video_element_picture_in_picture.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_controller_impl.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_window.h"
#include "third_party/blink/renderer/platform/feature_policy/feature_policy.h"

namespace blink {

using Status = PictureInPictureControllerImpl::Status;

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
  PictureInPictureControllerImpl& controller =
      PictureInPictureControllerImpl::From(document);

  switch (controller.IsElementAllowed(element)) {
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
  PictureInPictureWindow* window = controller.CreatePictureInPictureWindow(
      500 /* width */, 300 /* height */);

  controller.SetPictureInPictureElement(element);

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
  PictureInPictureControllerImpl& controller =
      PictureInPictureControllerImpl::From(document);
  if (controller.PictureInPictureElement(scope) == &element) {
    // TODO(crbug.com/806249): Call element.exitPictureInPicture().

    controller.OnClosePictureInPictureWindow();

    controller.UnsetPictureInPictureElement();

    element.DispatchEvent(
        Event::CreateBubble(EventTypeNames::leavepictureinpicture));
  }
}

}  // namespace blink
