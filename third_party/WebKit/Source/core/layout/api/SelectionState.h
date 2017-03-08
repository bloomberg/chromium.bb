// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SelectionState_h
#define SelectionState_h

#include <iosfwd>
#include "core/CoreExport.h"

namespace blink {

#define FOR_EACH_SELECTION_STATE(V)                                       \
  /* The object is not selected. */                                       \
  V(None)                                                                 \
  /* The object either contains the start of a selection run or is the */ \
  /* start of a run. */                                                   \
  V(Start)                                                                \
  /* The object is fully encompassed by a selection run. */               \
  V(Inside)                                                               \
  /* The object either contains the end of a selection run or is the */   \
  /* end of a run. */                                                     \
  V(End)                                                                  \
  /* The object contains an entire run or is the sole selected object */  \
  /* in that run. */                                                      \
  V(Both)

enum SelectionState {
#define V(state) Selection##state,
  FOR_EACH_SELECTION_STATE(V)
#undef V
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const SelectionState);

}  // namespace blink

#endif  // SelectionState_h
