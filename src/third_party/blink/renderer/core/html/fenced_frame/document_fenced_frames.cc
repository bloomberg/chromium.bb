// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/document_fenced_frames.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// static
const char DocumentFencedFrames::kSupplementName[] = "DocumentFencedFrame";

// static
DocumentFencedFrames& DocumentFencedFrames::From(Document& document) {
  DocumentFencedFrames* supplement =
      Supplement<Document>::From<DocumentFencedFrames>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<DocumentFencedFrames>(document);
    Supplement<Document>::ProvideTo(document, supplement);
  }
  return *supplement;
}

DocumentFencedFrames::DocumentFencedFrames(Document& document)
    : Supplement<Document>(document) {}

void DocumentFencedFrames::RegisterFencedFrame(
    HTMLFencedFrameElement* fenced_frame) {
  fenced_frames_.push_back(fenced_frame);
}

void DocumentFencedFrames::DeregisterFencedFrame(
    HTMLFencedFrameElement* fenced_frame) {
  wtf_size_t index = fenced_frames_.Find(fenced_frame);
  if (index != WTF::kNotFound) {
    fenced_frames_.EraseAt(index);
  }
}

void DocumentFencedFrames::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
  visitor->Trace(fenced_frames_);
}

}  // namespace blink
