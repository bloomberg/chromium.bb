// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CopylessPasteServer_h
#define CopylessPasteServer_h

#include "modules/ModulesExport.h"
#include "platform/heap/Persistent.h"
#include "public/platform/modules/document_metadata/copyless_paste.mojom-blink.h"

namespace blink {

class LocalFrame;

// Mojo interface to return extracted metadata for AppIndexing.
class MODULES_EXPORT CopylessPasteServer final
    : public mojom::document_metadata::blink::CopylessPaste {
 public:
  explicit CopylessPasteServer(LocalFrame&);

  static void BindMojoRequest(
      LocalFrame*,
      mojom::document_metadata::blink::CopylessPasteRequest);

  void GetEntities(const GetEntitiesCallback&) override;

 private:
  WeakPersistent<LocalFrame> frame_;
};

}  // namespace blink

#endif  // CopylessPasteServer_h
