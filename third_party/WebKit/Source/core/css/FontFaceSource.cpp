// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/FontFaceSource.h"

#include "core/css/FontFaceSetDocument.h"

namespace blink {

FontFaceSet* FontFaceSource::fonts(Document& document) {
  return FontFaceSetDocument::From(document);
}

}  // namespace blink
