// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubsequenceRecorder_h
#define SubsequenceRecorder_h

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class GraphicsContext;
class PaintController;

// SubsequenceRecorder records BeginSubsequenceDisplayItem and
// EndSubsequenceDisplayItem sentinels at either end of a continguous sequence
// of DisplayItems, and supports caching via a CachedDisplayItem with the
// CachedSubsequence DisplayItem type.
//
// Also note that useCachedSubsequenceIfPossible is not sufficient to determine
// whether a CachedSubsequence can be used. In particular, the client is
// responsible for checking that none of the DisplayItemClients that contribute
// to the subsequence have been invalidated.
//
class PLATFORM_EXPORT SubsequenceRecorder final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  WTF_MAKE_NONCOPYABLE(SubsequenceRecorder);

 public:
  static bool UseCachedSubsequenceIfPossible(GraphicsContext& context,
                                             const DisplayItemClient& client) {
    return context.GetPaintController().UseCachedSubsequenceIfPossible(client);
  }

  SubsequenceRecorder(GraphicsContext&, const DisplayItemClient&);
  ~SubsequenceRecorder();

 private:
  PaintController& paint_controller_;
  const DisplayItemClient& client_;
  size_t begin_subsequence_index_;
};

}  // namespace blink

#endif  // SubsequenceRecorder_h
