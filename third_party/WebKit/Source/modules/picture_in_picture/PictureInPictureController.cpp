// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/picture_in_picture/PictureInPictureController.h"

#include "core/dom/Document.h"
#include "core/frame/Settings.h"
#include "core/html/media/HTMLVideoElement.h"
#include "modules/picture_in_picture/PictureInPictureWindow.h"
#include "platform/feature_policy/FeaturePolicy.h"

namespace blink {

PictureInPictureController::PictureInPictureController(Document& document)
    : Supplement<Document>(document) {}

PictureInPictureController::~PictureInPictureController() = default;

// static
PictureInPictureController& PictureInPictureController::Ensure(
    Document& document) {
  PictureInPictureController* controller =
      Supplement<Document>::From<PictureInPictureController>(document);
  if (!controller) {
    controller = new PictureInPictureController(document);
    ProvideTo(document, controller);
  }
  return *controller;
}

// static
const char PictureInPictureController::kSupplementName[] =
    "PictureInPictureController";

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

  // `GetPictureInPictureEnabled()` returns false when the embedder or the
  // system forbids the page from using Picture-in-Picture.
  DCHECK(GetSupplementable()->GetSettings());
  if (!GetSupplementable()->GetSettings()->GetPictureInPictureEnabled())
    return Status::kDisabledBySystem;

  // If document is not allowed to use the policy-controlled feature named
  // "picture-in-picture", return kDisabledByFeaturePolicy status.
  if (IsSupportedInFeaturePolicy(
          blink::mojom::FeaturePolicyFeature::kPictureInPicture) &&
      !frame->IsFeatureEnabled(
          blink::mojom::FeaturePolicyFeature::kPictureInPicture)) {
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

void PictureInPictureController::SetPictureInPictureElement(
    HTMLVideoElement& element) {
  picture_in_picture_element_ = &element;
}

void PictureInPictureController::UnsetPictureInPictureElement() {
  picture_in_picture_element_ = nullptr;
}

Element* PictureInPictureController::PictureInPictureElement(
    TreeScope& scope) const {
  if (!picture_in_picture_element_)
    return nullptr;

  return scope.AdjustedElement(*picture_in_picture_element_);
}

PictureInPictureWindow*
PictureInPictureController::CreatePictureInPictureWindow(int width,
                                                         int height) {
  if (picture_in_picture_window_)
    picture_in_picture_window_->OnClose();

  picture_in_picture_window_ =
      new PictureInPictureWindow(GetSupplementable(), width, height);
  return picture_in_picture_window_;
}

void PictureInPictureController::OnClosePictureInPictureWindow() {
  if (!picture_in_picture_window_)
    return;

  picture_in_picture_window_->OnClose();
}

void PictureInPictureController::Trace(blink::Visitor* visitor) {
  visitor->Trace(picture_in_picture_element_);
  visitor->Trace(picture_in_picture_window_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
