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
#include "public/platform/WebCoalescedInputEvent.h"
#include "public/platform/WebInputEventResult.h"
#include "public/platform/WebPointerEvent.h"
#include "public/platform/WebTouchEvent.h"

namespace blink {

class LocalFrame;
class Document;

// This class takes care of dispatching all touch events and
// maintaining related states.
class CORE_EXPORT TouchEventManager
    : public GarbageCollectedFinalized<TouchEventManager> {
  WTF_MAKE_NONCOPYABLE(TouchEventManager);

 public:

  explicit TouchEventManager(LocalFrame&);
  DECLARE_TRACE();

  void HandleTouchPoint(const WebPointerEvent&,
                        const Vector<WebPointerEvent>&,
                        const EventHandlingUtil::PointerEventTarget&);

  WebInputEventResult FlushEvents();

  // Resets the internal state of this object.
  void Clear();

  // Returns whether there is any touch on the screen.
  bool IsAnyTouchActive() const;

 private:
  // Class represending one touch point event with its coalesced events and
  // related attributes.
  class TouchPointAttributes
      : public GarbageCollectedFinalized<TouchPointAttributes> {
   public:
    DEFINE_INLINE_TRACE() { visitor->Trace(target_); }

    TouchPointAttributes() {}
    explicit TouchPointAttributes(WebPointerEvent event)
        : event_(event), stale_(false) {}

    // Last state of the touch point.
    WebPointerEvent event_;
    // The list of coalesced events of the touch point represented by this class
    // if there is any. Note that at the end of each frame this list gets
    // cleared and the touch point |stale_| flag will be true for the next frame
    // unless more new events arrives for this touch point.
    Vector<WebPointerEvent> coalesced_events_;
    Member<Node> target_;  // The target of each active touch point.
    String region_;        //  // The region of each active touch point.
    bool stale_;
  };

  WebCoalescedInputEvent GenerateWebCoalescedInputEvent();
  Touch* CreateDomTouch(const TouchPointAttributes*, bool* known_target);
  void AllTouchesReleasedCleanup();

  // Keeps track of attributes of the touch point in the
  // |touch_points_attributes_| map and does the hit-testing if the original hit
  // test result was not inside capturing frame |touch_sequence_document_| for
  // touch events.
  void UpdateTouchAttributeMapsForPointerDown(
      const WebPointerEvent&,
      const EventHandlingUtil::PointerEventTarget&);

  // This is triggered either by VSync signal to send one touch event per frame
  // accumulating all move events or by discrete events pointerdown/up/cancel.
  WebInputEventResult DispatchTouchEventFromAccumulatdTouchPoints();

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |TouchEventManager::clear()|.

  const Member<LocalFrame> frame_;

  // The attributes of each active touch point indexed by the touch ID.
  using TouchAttributeMap =
      HeapHashMap<int,
                  Member<TouchPointAttributes>,
                  WTF::IntHash<int>,
                  WTF::UnsignedWithZeroKeyHashTraits<int>>;
  TouchAttributeMap touch_attribute_map_;

  // If set, the document of the active touch sequence. Unset if no touch
  // sequence active.
  Member<Document> touch_sequence_document_;

  bool suppressing_touchmoves_within_slop_;

  // This is used to created a consistent sequence of coalesced events compared
  // to the last frame.
  WebTouchEvent last_coalesced_touch_event_;

  // The current touch action, computed on each touch start and is
  // a union of all touches. Reset when all touches are released.
  TouchAction current_touch_action_;
};

}  // namespace blink

#endif  // TouchEventManager_h
