// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SpellingMarkerListImpl_h
#define SpellingMarkerListImpl_h

#include "core/editing/markers/SpellCheckMarkerListImpl.h"

namespace blink {

// This is the DocumentMarkerList implementation used to store Spelling markers.
class CORE_EXPORT SpellingMarkerListImpl final
    : public SpellCheckMarkerListImpl {
 public:
  SpellingMarkerListImpl() = default;

  DocumentMarker::MarkerType MarkerType() const final;

 private:
  DISALLOW_COPY_AND_ASSIGN(SpellingMarkerListImpl);
};

}  // namespace blink

#endif  // SpellingMarkerListImpl_h
