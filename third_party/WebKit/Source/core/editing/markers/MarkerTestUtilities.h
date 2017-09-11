// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MarkerTestUtilities_h
#define MarkerTestUtilities_h

#include "core/editing/markers/SuggestionMarker.h"
#include "platform/heap/Handle.h"

namespace blink {
inline bool compare_markers(const Member<DocumentMarker>& marker1,
                            const Member<DocumentMarker>& marker2) {
  if (marker1->StartOffset() != marker2->StartOffset())
    return marker1->StartOffset() < marker2->StartOffset();

  return marker1->EndOffset() < marker2->EndOffset();
}
}  // namespace blink

#endif  // MarkerTestUtilities_h
