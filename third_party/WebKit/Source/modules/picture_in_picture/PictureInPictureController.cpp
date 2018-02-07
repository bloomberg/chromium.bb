// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/picture_in_picture/PictureInPictureController.h"

#include "core/dom/Document.h"
#include "core/html/media/HTMLVideoElement.h"
#include "platform/feature_policy/FeaturePolicy.h"

namespace blink {

PictureInPictureController::PictureInPictureController(Document& document)
    : Supplement<Document>(document) {}

PictureInPictureController::~PictureInPictureController() = default;

// static
PictureInPictureController& PictureInPictureController::Ensure(
    Document& document) {
  PictureInPictureController* controller =
      static_cast<PictureInPictureController*>(
          Supplement<Document>::From(document, SupplementName()));
  if (!controller) {
    controller = new PictureInPictureController(document);
    ProvideTo(document, SupplementName(), controller);
  }
  return *controller;
}

// static
const char* PictureInPictureController::SupplementName() {
  return "PictureInPictureController";
}

bool PictureInPictureController::PictureInPictureEnabled() const {
  return IsDocumentAllowed() == Status::kEnabled;
}

PictureInPictureController::Status
PictureInPictureController::IsDocumentAllowed() const {
  DCHECK(GetSupplementable());

  // If document has been detached from a frame, return kFrameDetached status.
  LocalFrame* frame = GetSupplementable()->GetFrame();
  if (!frame)
    return Status::kFrameDetached;

  // `picture_in_picture_enabled_` is set to false by the embedder when it
  // or the system forbids the page from using Picture-in-Picture.
  if (!picture_in_picture_enabled_)
    return Status::kDisabledBySystem;

  // If document is not allowed to use the policy-controlled feature named
  // "picture-in-picture", return kDisabledByFeaturePolicy status.
  if (IsSupportedInFeaturePolicy(
          blink::FeaturePolicyFeature::kPictureInPicture) &&
      !frame->IsFeatureEnabled(
          blink::FeaturePolicyFeature::kPictureInPicture)) {
    return Status::kDisabledByFeaturePolicy;
  }

  return Status::kEnabled;
}

PictureInPictureController::Status PictureInPictureController::IsElementAllowed(
    HTMLVideoElement& element) const {
  PictureInPictureController::Status status = IsDocumentAllowed();
  if (status != Status::kEnabled)
    return status;

  if (element.FastHasAttribute(HTMLNames::disablepictureinpictureAttr))
    return Status::kDisabledByAttribute;

  return Status::kEnabled;
}

void PictureInPictureController::SetPictureInPictureEnabledForTesting(
    bool value) {
  picture_in_picture_enabled_ = value;
}

void PictureInPictureController::SetPictureInPictureElement(
    HTMLVideoElement& element) {
  picture_in_picture_element_ = &element;
}

void PictureInPictureController::UnsetPictureInPictureElement() {
  picture_in_picture_element_ = nullptr;
}

HTMLVideoElement* PictureInPictureController::PictureInPictureElement() const {
  return picture_in_picture_element_;
}

void PictureInPictureController::Trace(blink::Visitor* visitor) {
  visitor->Trace(picture_in_picture_element_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
