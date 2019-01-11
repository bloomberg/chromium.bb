// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/frame_resource_fetcher_properties.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/frame_or_imported_document.h"

namespace blink {

FrameResourceFetcherProperties::FrameResourceFetcherProperties(
    FrameOrImportedDocument& frame_or_imported_document)
    : frame_or_imported_document_(frame_or_imported_document) {}

void FrameResourceFetcherProperties::Trace(Visitor* visitor) {
  visitor->Trace(frame_or_imported_document_);
  ResourceFetcherProperties::Trace(visitor);
}

bool FrameResourceFetcherProperties::IsMainFrame() const {
  return frame_or_imported_document_->GetFrame().IsMainFrame();
}

}  // namespace blink
