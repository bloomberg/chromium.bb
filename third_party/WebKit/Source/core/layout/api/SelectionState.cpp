// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/api/SelectionState.h"

#include "platform/wtf/Assertions.h"

namespace blink {

std::ostream& operator<<(std::ostream& out, const SelectionState state) {
  switch (state) {
    case SelectionState::kNone:
      return out << "None";
    case SelectionState::kStart:
      return out << "Start";
    case SelectionState::kInside:
      return out << "Inside";
    case SelectionState::kEnd:
      return out << "End";
    case SelectionState::kStartAndEnd:
      return out << "StartAndEnd";
    case SelectionState::kContain:
      return out << "Contain";
  }
  NOTREACHED();
  return out;
}

}  // namespace blink
