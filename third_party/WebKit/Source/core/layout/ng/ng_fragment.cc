// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment.h"

#include "core/layout/ng/ng_physical_fragment.h"

namespace blink {

NGMarginStrut NGFragment::MarginStrut() const {
  // NOTE: Accessing the margin strut ignoring the writing mode here is fine.
  // Changing the writing mode establishes a new formatting context, for which
  // a margin strut is never set for a fragment.
  return toNGPhysicalFragment(physical_fragment_)->MarginStrut();
}

}  // namespace blink
