// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

namespace blink {

// Define the constructor and destructor here, so that we can forward-declare
// more in the header file.
NGUnpositionedFloat::NGUnpositionedFloat(NGBlockNode node,
                                         const NGBlockBreakToken* token)
    : node(node), token(token) {}

NGUnpositionedFloat::~NGUnpositionedFloat() = default;

}  // namespace blink
