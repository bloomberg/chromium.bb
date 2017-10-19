/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AutoscrollController_h
#define AutoscrollController_h

#include "core/CoreExport.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntPoint.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Time.h"

namespace blink {

class LocalFrame;
class Node;
class Page;
class LayoutBox;
class LayoutObject;

enum AutoscrollType {
  kNoAutoscroll,
  kAutoscrollForDragAndDrop,
  kAutoscrollForSelection,
  kAutoscrollForMiddleClick,
};

enum MiddleClickMode {
  // Middle button was just pressed but was neither released nor moved out of
  // the deadzone yet.
  kMiddleClickInitial,
  // Mouse was moved out of the deadzone while still holding middle mouse
  // button.  In this mode, we'll stop autoscrolling when it's released.
  kMiddleClickHolding,
  // Middle button was released while still in the deadzone.  In this mode,
  // we'll stop autoscrolling when any button is clicked.
  kMiddleClickToggled,
};

// AutscrollController handels autoscroll and middle click autoscroll for
// EventHandler.
class CORE_EXPORT AutoscrollController final
    : public GarbageCollected<AutoscrollController> {
 public:
  static AutoscrollController* Create(Page&);
  void Trace(blink::Visitor*);

  // Selection and drag-and-drop autoscroll.
  void Animate(double monotonic_frame_begin_time);
  bool SelectionAutoscrollInProgress() const;
  bool AutoscrollInProgressFor(const LayoutBox*) const;
  bool AutoscrollInProgress() const;
  void StartAutoscrollForSelection(LayoutObject*);
  void StopAutoscroll();
  void StopAutoscrollIfNeeded(LayoutObject*);
  void UpdateAutoscrollLayoutObject();
  void UpdateDragAndDrop(Node* target_node,
                         const IntPoint& event_position,
                         TimeTicks event_time);

  // Middle-click autoscroll.
  void StartMiddleClickAutoscroll(LocalFrame*,
                                  const FloatPoint& position,
                                  const FloatPoint& position_global);
  void HandleMouseMoveForMiddleClickAutoscroll(
      LocalFrame*,
      const FloatPoint& position_global,
      bool is_middle_button);
  void HandleMouseReleaseForMiddleClickAutoscroll(LocalFrame*,
                                                  bool is_middle_button);
  void StopMiddleClickAutoscroll(LocalFrame*);
  bool MiddleClickAutoscrollInProgress() const;

 private:
  explicit AutoscrollController(Page&);

  Member<Page> page_;
  AutoscrollType autoscroll_type_ = kNoAutoscroll;

  // Selection and drag-and-drop autoscroll.
  void ScheduleMainThreadAnimation();
  LayoutBox* autoscroll_layout_object_ = nullptr;
  LayoutBox* pressed_layout_object_ = nullptr;
  IntPoint drag_and_drop_autoscroll_reference_position_;
  TimeTicks drag_and_drop_autoscroll_start_time_;

  // Middle-click autoscroll.
  FloatPoint middle_click_autoscroll_start_pos_global_;
  FloatSize last_velocity_;
  MiddleClickMode middle_click_mode_ = kMiddleClickInitial;
};

}  // namespace blink

#endif  // AutoscrollController_h
