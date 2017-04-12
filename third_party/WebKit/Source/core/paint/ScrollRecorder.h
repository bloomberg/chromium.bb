// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollRecorder_h
#define ScrollRecorder_h

#include "core/CoreExport.h"
#include "core/paint/PaintPhase.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class GraphicsContext;

// Emits display items which represent a region which is scrollable, so that it
// can be translated by the scroll offset.
class CORE_EXPORT ScrollRecorder {
  USING_FAST_MALLOC(ScrollRecorder);

 public:
  ScrollRecorder(GraphicsContext&,
                 const DisplayItemClient&,
                 DisplayItem::Type,
                 const IntSize& current_offset);
  ScrollRecorder(GraphicsContext&,
                 const DisplayItemClient&,
                 PaintPhase,
                 const IntSize& current_offset);
  ~ScrollRecorder();

 private:
  const DisplayItemClient& client_;
  DisplayItem::Type begin_item_type_;
  GraphicsContext& context_;
};

}  // namespace blink

#endif  // ScrollRecorder_h
