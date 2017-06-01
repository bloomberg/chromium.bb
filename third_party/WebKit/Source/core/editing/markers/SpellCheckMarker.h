// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SpellCheckMarker_h
#define SpellCheckMarker_h

#include "core/editing/markers/DocumentMarker.h"

namespace blink {

// A subclass of DocumentMarker used to implement functionality shared between
// spelling and grammar markers. These two marker types both store a
// description string that can contain suggested replacements for a misspelling
// or grammar error.
class CORE_EXPORT SpellCheckMarker : public DocumentMarker {
 public:
  SpellCheckMarker(unsigned start_offset,
                   unsigned end_offset,
                   const String& description);

  const String& Description() const { return description_; }

 private:
  const String description_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckMarker);
};

bool CORE_EXPORT IsSpellCheckMarker(const DocumentMarker&);

DEFINE_TYPE_CASTS(SpellCheckMarker,
                  DocumentMarker,
                  marker,
                  IsSpellCheckMarker(*marker),
                  IsSpellCheckMarker(marker));

}  // namespace blink

#endif
