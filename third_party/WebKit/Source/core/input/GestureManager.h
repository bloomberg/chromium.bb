// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GestureManager_h
#define GestureManager_h

#include "core/CoreExport.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/HitTestRequest.h"
#include "core/page/EventWithHitTestResults.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Time.h"
#include "public/platform/WebInputEventResult.h"

namespace blink {

class ScrollManager;
class SelectionController;
class PointerEventManager;
class MouseEventManager;

// This class takes care of gestures and delegating the action based on the
// gesture to the responsible class.
class CORE_EXPORT GestureManager
    : public GarbageCollectedFinalized<GestureManager> {
  WTF_MAKE_NONCOPYABLE(GestureManager);

 public:
  GestureManager(LocalFrame&,
                 ScrollManager&,
                 MouseEventManager&,
                 PointerEventManager&,
                 SelectionController&);
  void Trace(blink::Visitor*);

  void Clear();

  HitTestRequest::HitTestRequestType GetHitTypeForGestureType(
      WebInputEvent::Type);
  WebInputEventResult HandleGestureEventInFrame(
      const GestureEventWithHitTestResults&);

  // TODO(nzolghadr): This can probably be hidden and the related logic
  // be moved to this class (see crrev.com/112023010). Since that might cause
  // regression it's better to move that logic in another change.
  WTF::Optional<WTF::TimeTicks> GetLastShowPressTimestamp() const;

 private:
  WebInputEventResult HandleGestureShowPress();
  WebInputEventResult HandleGestureTapDown(
      const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureTap(const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureLongPress(
      const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureLongTap(
      const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureTwoFingerTap(
      const GestureEventWithHitTestResults&);

  WebInputEventResult SendContextMenuEventForGesture(
      const GestureEventWithHitTestResults&);

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared if needed in |GestureManager::clear()|.

  const Member<LocalFrame> frame_;

  Member<ScrollManager> scroll_manager_;
  Member<MouseEventManager> mouse_event_manager_;
  Member<PointerEventManager> pointer_event_manager_;

  // Set on GestureTapDown if the |pointerdown| event corresponding to the
  // triggering |touchstart| event was canceled. This suppresses mouse event
  // firing for the current gesture sequence (i.e. until next GestureTapDown).
  bool suppress_mouse_events_from_gestures_;

  bool long_tap_should_invoke_context_menu_;

  const Member<SelectionController> selection_controller_;

  WTF::Optional<WTF::TimeTicks> last_show_press_timestamp_;
};

}  // namespace blink

#endif  // GestureManager_h
