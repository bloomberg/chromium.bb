// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SelectionState_h
#define SelectionState_h

#include <iosfwd>
#include "core/CoreExport.h"

namespace blink {

enum class SelectionState {
  /* The object is not selected. */
  kNone,
  /* The object either contains the start of a selection run or is the */
  /* start of a run. */
  kStart,
  /* The object is fully encompassed by a selection run. */
  kInside,
  /* The object either contains the end of a selection run or is the */
  /* end of a run. */
  kEnd,
  /* The object contains an entire run or is the sole selected object */
  /* in that run. */
  kStartAndEnd
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const SelectionState);

}  // namespace blink

#endif  // SelectionState_h
