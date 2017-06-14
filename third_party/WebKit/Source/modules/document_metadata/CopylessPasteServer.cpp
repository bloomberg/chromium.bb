// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/document_metadata/CopylessPasteServer.h"

#include "core/frame/LocalFrame.h"
#include "modules/document_metadata/CopylessPasteExtractor.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace blink {

CopylessPasteServer::CopylessPasteServer(LocalFrame& frame) : frame_(frame) {}

void CopylessPasteServer::BindMojoRequest(
    LocalFrame* frame,
    mojom::document_metadata::blink::CopylessPasteRequest request) {
  DCHECK(frame);

  // TODO(wychen): remove bindMojoRequest pattern, and make this a service
  // associated with frame lifetime.
  mojo::MakeStrongBinding(WTF::MakeUnique<CopylessPasteServer>(*frame),
                          std::move(request));
}

void CopylessPasteServer::GetEntities(GetEntitiesCallback callback) {
  if (!frame_ || !frame_->GetDocument()) {
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(
      CopylessPasteExtractor::extract(*frame_->GetDocument()));
}

}  // namespace blink
