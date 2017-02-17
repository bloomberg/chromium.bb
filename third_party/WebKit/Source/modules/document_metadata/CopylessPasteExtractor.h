// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CopylessPasteExtractor_h
#define CopylessPasteExtractor_h

#include "modules/ModulesExport.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;

// Extract structured metadata (currently schema.org in JSON-LD) for the
// Copyless Paste feature. The extraction must be done after the document
// has finished parsing.
class MODULES_EXPORT CopylessPasteExtractor final {
 public:
  static String extract(Document&);
};

}  // namespace blink

#endif  // CopylessPasteExtractor_h
