// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_METADATA_DOCUMENT_METADATA_SERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_METADATA_DOCUMENT_METADATA_SERVER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/document_metadata/document_metadata.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class LocalFrame;

// Mojo interface to return extracted metadata for AppIndexing.
class MODULES_EXPORT DocumentMetadataServer final
    : public mojom::blink::DocumentMetadata {
 public:
  explicit DocumentMetadataServer(LocalFrame&);

  static void BindMojoReceiver(
      LocalFrame*,
      mojo::PendingReceiver<mojom::blink::DocumentMetadata>);

  void GetEntities(GetEntitiesCallback) override;

 private:
  WeakPersistent<LocalFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_METADATA_DOCUMENT_METADATA_SERVER_H_
