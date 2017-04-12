// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/api/SelectionState.h"

#include "platform/wtf/Assertions.h"

namespace blink {

std::ostream& operator<<(std::ostream& out, const SelectionState state) {
  static const char* const kText[] = {
#define V(state) #state,
      FOR_EACH_SELECTION_STATE(V)
#undef V
  };

  const auto& it = std::begin(kText) + static_cast<size_t>(state);
  DCHECK_GE(it, std::begin(kText)) << "Unknown state value";
  DCHECK_LT(it, std::end(kText)) << "Unknown state value";
  return out << *it;
}

}  // namespace blink
