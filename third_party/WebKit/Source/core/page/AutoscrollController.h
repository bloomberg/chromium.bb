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
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntPoint.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Time.h"

namespace blink {

class LocalFrame;
class FrameView;
class Node;
class Page;
class LayoutBox;
class LayoutObject;
class WebMouseEvent;

enum AutoscrollType {
  kNoAutoscroll,
  kAutoscrollForDragAndDrop,
  kAutoscrollForSelection,
  kAutoscrollForMiddleClickCanStop,
  kAutoscrollForMiddleClick,
};

// AutscrollController handels autoscroll and middle click autoscroll for
// EventHandler.
class CORE_EXPORT AutoscrollController final
    : public GarbageCollected<AutoscrollController> {
 public:
  static AutoscrollController* Create(Page&);
  DECLARE_TRACE();

  static const int kNoMiddleClickAutoscrollRadius = 15;

  void Animate(double monotonic_frame_begin_time);
  bool AutoscrollInProgress() const;
  bool AutoscrollInProgress(const LayoutBox*) const;
  bool MiddleClickAutoscrollInProgress() const;
  void StartAutoscrollForSelection(LayoutObject*);
  void StopAutoscroll();
  void StopAutoscrollIfNeeded(LayoutObject*);
  void UpdateAutoscrollLayoutObject();
  void UpdateDragAndDrop(Node* target_node,
                         const IntPoint& event_position,
                         TimeTicks event_time);
  void HandleMouseReleaseForMiddleClickAutoscroll(LocalFrame*,
                                                  const WebMouseEvent&);
  void StartMiddleClickAutoscroll(LayoutBox*, const IntPoint&);

 private:
  explicit AutoscrollController(Page&);

  void StartAutoscroll();

  void UpdateMiddleClickAutoscrollState(
      FrameView*,
      const IntPoint& last_known_mouse_position);
  FloatSize CalculateAutoscrollDelta();

  Member<Page> page_;
  LayoutBox* autoscroll_layout_object_;
  LayoutBox* pressed_layout_object_;
  AutoscrollType autoscroll_type_;
  IntPoint drag_and_drop_autoscroll_reference_position_;
  TimeTicks drag_and_drop_autoscroll_start_time_;
  IntPoint middle_click_autoscroll_start_pos_;
  bool did_latch_for_middle_click_autoscroll_;
};

}  // namespace blink

#endif  // AutoscrollController_h
