// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/html_element_picture_in_picture.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_controller_impl.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_options.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

using Status = PictureInPictureControllerImpl::Status;

namespace {

const char kDetachedError[] =
    "The element is no longer associated with a document.";
const char kMetadataNotLoadedError[] =
    "Metadata for the video element are not loaded yet.";
const char kVideoTrackNotAvailableError[] =
    "The video element has no video track.";
const char kFeaturePolicyBlocked[] =
    "Access to the feature \"picture-in-picture\" is disallowed by feature "
    "policy.";
const char kNotAvailable[] = "Picture-in-Picture is not available.";
const char kUserGestureRequired[] =
    "Must be handling a user gesture if there isn't already an element in "
    "Picture-in-Picture.";
const char kDisablePictureInPicturePresent[] =
    "\"disablePictureInPicture\" attribute is present.";
}  // namespace

// static
ScriptPromise HTMLElementPictureInPicture::requestPictureInPicture(
    ScriptState* script_state,
    HTMLElement& element,
    PictureInPictureOptions* options) {
  DOMException* exception = CheckIfPictureInPictureIsAllowed(element);
  if (exception)
    return ScriptPromise::RejectWithDOMException(script_state, exception);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  PictureInPictureControllerImpl::From(element.GetDocument())
      .EnterPictureInPicture(&element, options, resolver);

  return promise;
}

// static
DOMException* HTMLElementPictureInPicture::CheckIfPictureInPictureIsAllowed(
    HTMLElement& element) {
  Document& document = element.GetDocument();
  PictureInPictureControllerImpl& controller =
      PictureInPictureControllerImpl::From(document);

  switch (controller.IsElementAllowed(element)) {
    case Status::kFrameDetached:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, kDetachedError);
    case Status::kMetadataNotLoaded:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, kMetadataNotLoadedError);
    case Status::kVideoTrackNotAvailable:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, kVideoTrackNotAvailableError);
    case Status::kDisabledByFeaturePolicy:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError, kFeaturePolicyBlocked);
    case Status::kDisabledByAttribute:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          kDisablePictureInPicturePresent);
    case Status::kDisabledBySystem:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, kNotAvailable);
    case Status::kEnabled:
      break;
  }

  // Frame is not null, otherwise `IsElementAllowed()` would have return
  // `kFrameDetached`.
  LocalFrame* frame = document.GetFrame();
  DCHECK(frame);
  if (!controller.PictureInPictureElement() &&
      !LocalFrame::ConsumeTransientUserActivation(frame)) {
    return MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, kUserGestureRequired);
  }

  return nullptr;
}

}  // namespace blink
