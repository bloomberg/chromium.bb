// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GrammarMarkerListImpl_h
#define GrammarMarkerListImpl_h

#include "core/editing/markers/SpellCheckMarkerListImpl.h"

namespace blink {

// This is the DocumentMarkerList implementation used to store Grammar markers.
class CORE_EXPORT GrammarMarkerListImpl final
    : public SpellCheckMarkerListImpl {
 public:
  GrammarMarkerListImpl() = default;

  DocumentMarker::MarkerType MarkerType() const final;

 private:
  DISALLOW_COPY_AND_ASSIGN(GrammarMarkerListImpl);
};

}  // namespace blink

#endif  // GrammarMarkerListImpl_h
