// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/SubsequenceRecorder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

SubsequenceRecorder::SubsequenceRecorder(GraphicsContext& context,
                                         const DisplayItemClient& client)
    : paint_controller_(context.GetPaintController()),
      client_(client),
      begin_subsequence_index_(0) {
  if (paint_controller_.DisplayItemConstructionIsDisabled())
    return;

  begin_subsequence_index_ = paint_controller_.NewDisplayItemList().size();
}

SubsequenceRecorder::~SubsequenceRecorder() {
  if (paint_controller_.DisplayItemConstructionIsDisabled())
    return;

  // Skip empty subsequences.
  if (paint_controller_.NewDisplayItemList().size() == begin_subsequence_index_)
    return;

  paint_controller_.AddCachedSubsequence(
      client_, begin_subsequence_index_,
      paint_controller_.NewDisplayItemList().size() - 1);
}

}  // namespace blink
