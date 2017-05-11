// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TouchEventManager_h
#define TouchEventManager_h

#include "core/CoreExport.h"
#include "core/events/PointerEventFactory.h"
#include "platform/graphics/TouchAction.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "public/platform/WebInputEventResult.h"
#include "public/platform/WebTouchPoint.h"

namespace blink {

class LocalFrame;
class Document;
class WebTouchEvent;

// This class takes care of dispatching all touch events and
// maintaining related states.
class CORE_EXPORT TouchEventManager
    : public GarbageCollectedFinalized<TouchEventManager> {
  WTF_MAKE_NONCOPYABLE(TouchEventManager);

 public:
  class TouchInfo {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

   public:
    DEFINE_INLINE_TRACE() {
      visitor->Trace(touch_node);
      visitor->Trace(target_frame);
    }

    WebTouchPoint point;
    Member<Node> touch_node;
    Member<LocalFrame> target_frame;
    FloatPoint content_point;
    FloatSize adjusted_radius;
    bool known_target;
    String region;
  };

  explicit TouchEventManager(LocalFrame&);
  DECLARE_TRACE();

  // Does the hit-testing again if the original hit test result was not inside
  // capturing frame for touch events. Returns true if touch events could be
  // dispatched and otherwise returns false.
  bool ReHitTestTouchPointsIfNeeded(const WebTouchEvent&,
                                    HeapVector<TouchInfo>&);

  // The TouchInfo array is reference just to prevent the copy. However, it
  // cannot be const as this function might change some of the properties in
  // TouchInfo objects.
  WebInputEventResult HandleTouchEvent(const WebTouchEvent&,
                                       HeapVector<TouchInfo>&);

  // Resets the internal state of this object.
  void Clear();

  // Returns whether there is any touch on the screen.
  bool IsAnyTouchActive() const;

 private:
  void UpdateTargetAndRegionMapsForTouchStarts(HeapVector<TouchInfo>&);
  void SetAllPropertiesOfTouchInfos(HeapVector<TouchInfo>&);

  WebInputEventResult DispatchTouchEvents(const WebTouchEvent&,
                                          const HeapVector<TouchInfo>&,
                                          bool all_touches_released);

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |TouchEventManager::clear()|.

  const Member<LocalFrame> frame_;

  // The target of each active touch point indexed by the touch ID.
  using TouchTargetMap =
      HeapHashMap<unsigned,
                  Member<Node>,
                  DefaultHash<unsigned>::Hash,
                  WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;
  TouchTargetMap target_for_touch_id_;
  using TouchRegionMap = HashMap<unsigned,
                                 String,
                                 DefaultHash<unsigned>::Hash,
                                 WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;
  TouchRegionMap region_for_touch_id_;

  // If set, the document of the active touch sequence. Unset if no touch
  // sequence active.
  Member<Document> touch_sequence_document_;

  bool touch_pressed_;
  bool suppressing_touchmoves_within_slop_;

  // The current touch action, computed on each touch start and is
  // a union of all touches. Reset when all touches are released.
  TouchAction current_touch_action_;
};

}  // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::TouchEventManager::TouchInfo);

#endif  // TouchEventManager_h
