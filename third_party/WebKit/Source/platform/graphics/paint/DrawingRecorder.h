// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DrawingRecorder_h
#define DrawingRecorder_h

#include "platform/PlatformExport.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "wtf/Allocator.h"
#include "wtf/AutoReset.h"
#include "wtf/Noncopyable.h"

#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

class GraphicsContext;

class PLATFORM_EXPORT DrawingRecorder final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  WTF_MAKE_NONCOPYABLE(DrawingRecorder);

 public:
  static bool useCachedDrawingIfPossible(GraphicsContext& context,
                                         const DisplayItemClient& client,
                                         DisplayItem::Type type) {
    return context.getPaintController().useCachedDrawingIfPossible(client,
                                                                   type);
  }

  DrawingRecorder(GraphicsContext&,
                  const DisplayItemClient&,
                  DisplayItem::Type,
                  const FloatRect& cullRect);
  ~DrawingRecorder();

  void setKnownToBeOpaque() {
    DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    m_knownToBeOpaque = true;
  }

 private:
  GraphicsContext& m_context;
  const DisplayItemClient& m_displayItemClient;
  const DisplayItem::Type m_displayItemType;

  // True if there are no transparent areas. Only used for SlimmingPaintV2.
  bool m_knownToBeOpaque;

#if DCHECK_IS_ON()
  // Ensures the list size does not change during the recorder's scope.
  size_t m_initialDisplayItemListSize;
#endif
};

#if DCHECK_IS_ON()
class DisableListModificationCheck {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(DisableListModificationCheck);

 public:
  DisableListModificationCheck();

 private:
  AutoReset<bool> m_disabler;
};
#endif  // DCHECK_IS_ON()

}  // namespace blink

#endif  // DrawingRecorder_h
