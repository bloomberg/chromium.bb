// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TouchEventManager_h
#define TouchEventManager_h

#include "core/CoreExport.h"
#include "core/events/PointerEventFactory.h"
#include "core/input/EventHandlingUtil.h"
#include "platform/graphics/TouchAction.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"
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

  explicit TouchEventManager(LocalFrame&);
  DECLARE_TRACE();

  // The entries in touch point array of WebTouchEvent (i.e. first parameter)
  // correspond to the entries of the PointerEventTargets (i.e. last parameter).
  WebInputEventResult HandleTouchEvent(
      const WebTouchEvent&,
      const Vector<WebTouchEvent>&,
      const HeapVector<EventHandlingUtil::PointerEventTarget>&);

  // Resets the internal state of this object.
  void Clear();

  // Returns whether there is any touch on the screen.
  bool IsAnyTouchActive() const;

 private:
  Touch* CreateDomTouch(const WebTouchPoint&, bool* known_target);

  void UpdateTargetAndRegionMapsForTouchStart(
      const WebTouchPoint&,
      const EventHandlingUtil::PointerEventTarget&);

  // Does the hit-testing if the original hit test result was not inside
  // capturing frame for touch events. Returns true if touch events could be
  // dispatched and otherwise returns false.
  bool HitTestTouchPointsIfNeeded(
      const WebTouchEvent&,
      const HeapVector<EventHandlingUtil::PointerEventTarget>&);

  WebInputEventResult DispatchTouchEvents(const WebTouchEvent&,
                                          const Vector<WebTouchEvent>&,
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

#endif  // TouchEventManager_h
