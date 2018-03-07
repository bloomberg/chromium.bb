// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/PictureInPictureController.h"

#include "core/CoreInitializer.h"
#include "core/dom/Document.h"

namespace blink {

PictureInPictureController::PictureInPictureController(Document& document)
    : Supplement<Document>(document) {}

// static
const char PictureInPictureController::kSupplementName[] =
    "PictureInPictureController";

// static
PictureInPictureController& PictureInPictureController::From(
    Document& document) {
  PictureInPictureController* controller =
      Supplement<Document>::From<PictureInPictureController>(document);
  if (!controller) {
    controller =
        CoreInitializer::GetInstance().CreatePictureInPictureController(
            document);
    ProvideTo(document, controller);
  }
  return *controller;
}

void PictureInPictureController::Trace(blink::Visitor* visitor) {
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
