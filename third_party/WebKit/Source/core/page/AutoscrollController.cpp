/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
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

#include "core/page/AutoscrollController.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutListBox.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/wtf/Time.h"

namespace blink {

// Delay time in second for start autoscroll if pointer is in border edge of
// scrollable element.
static const TimeDelta kAutoscrollDelay = TimeDelta::FromSecondsD(0.2);

AutoscrollController* AutoscrollController::Create(Page& page) {
  return new AutoscrollController(page);
}

AutoscrollController::AutoscrollController(Page& page)
    : page_(&page),
      autoscroll_layout_object_(nullptr),
      pressed_layout_object_(nullptr),
      autoscroll_type_(kNoAutoscroll),
      did_latch_for_middle_click_autoscroll_(false) {}

DEFINE_TRACE(AutoscrollController) {
  visitor->Trace(page_);
}

bool AutoscrollController::AutoscrollInProgress() const {
  return autoscroll_type_ == kAutoscrollForSelection;
}

bool AutoscrollController::AutoscrollInProgress(
    const LayoutBox* layout_object) const {
  return autoscroll_layout_object_ == layout_object;
}

void AutoscrollController::StartAutoscrollForSelection(
    LayoutObject* layout_object) {
  // We don't want to trigger the autoscroll or the middleClickAutoscroll if
  // it's already active.
  if (autoscroll_type_ != kNoAutoscroll)
    return;
  LayoutBox* scrollable = LayoutBox::FindAutoscrollable(layout_object);
  if (!scrollable)
    scrollable =
        layout_object->IsListBox() ? ToLayoutListBox(layout_object) : nullptr;
  if (!scrollable)
    return;

  pressed_layout_object_ = layout_object && layout_object->IsBox()
                               ? ToLayoutBox(layout_object)
                               : nullptr;
  autoscroll_type_ = kAutoscrollForSelection;
  autoscroll_layout_object_ = scrollable;
  StartAutoscroll();
}

void AutoscrollController::StopAutoscroll() {
  if (pressed_layout_object_) {
    pressed_layout_object_->StopAutoscroll();
    pressed_layout_object_ = nullptr;
  }
  LayoutBox* scrollable = autoscroll_layout_object_;
  autoscroll_layout_object_ = nullptr;

  if (!scrollable)
    return;

  if (RuntimeEnabledFeatures::middleClickAutoscrollEnabled() &&
      MiddleClickAutoscrollInProgress()) {
    if (FrameView* view = scrollable->GetFrame()->View()) {
      view->SetCursor(PointerCursor());
    }
  }
  autoscroll_type_ = kNoAutoscroll;
}

void AutoscrollController::StopAutoscrollIfNeeded(LayoutObject* layout_object) {
  if (pressed_layout_object_ == layout_object)
    pressed_layout_object_ = nullptr;

  if (autoscroll_layout_object_ != layout_object)
    return;
  autoscroll_layout_object_ = nullptr;
  autoscroll_type_ = kNoAutoscroll;
}

void AutoscrollController::UpdateAutoscrollLayoutObject() {
  if (!autoscroll_layout_object_)
    return;

  LayoutObject* layout_object = autoscroll_layout_object_;

  if (RuntimeEnabledFeatures::middleClickAutoscrollEnabled()) {
    HitTestResult hit_test =
        layout_object->GetFrame()->GetEventHandler().HitTestResultAtPoint(
            middle_click_autoscroll_start_pos_,
            HitTestRequest::kReadOnly | HitTestRequest::kActive);

    if (Node* node_at_point = hit_test.InnerNode())
      layout_object = node_at_point->GetLayoutObject();
  }

  while (layout_object && !(layout_object->IsBox() &&
                            ToLayoutBox(layout_object)->CanAutoscroll()))
    layout_object = layout_object->Parent();

  autoscroll_layout_object_ = layout_object && layout_object->IsBox()
                                  ? ToLayoutBox(layout_object)
                                  : nullptr;

  if (autoscroll_type_ != kNoAutoscroll && !autoscroll_layout_object_)
    autoscroll_type_ = kNoAutoscroll;
}

void AutoscrollController::UpdateDragAndDrop(Node* drop_target_node,
                                             const IntPoint& event_position,
                                             TimeTicks event_time) {
  if (!drop_target_node || !drop_target_node->GetLayoutObject()) {
    StopAutoscroll();
    return;
  }

  if (autoscroll_layout_object_ &&
      autoscroll_layout_object_->GetFrame() !=
          drop_target_node->GetLayoutObject()->GetFrame())
    return;

  drop_target_node->GetLayoutObject()
      ->GetFrameView()
      ->UpdateAllLifecyclePhasesExceptPaint();

  LayoutBox* scrollable =
      LayoutBox::FindAutoscrollable(drop_target_node->GetLayoutObject());
  if (!scrollable) {
    StopAutoscroll();
    return;
  }

  Page* page =
      scrollable->GetFrame() ? scrollable->GetFrame()->GetPage() : nullptr;
  if (!page) {
    StopAutoscroll();
    return;
  }

  IntSize offset = scrollable->CalculateAutoscrollDirection(event_position);
  if (offset.IsZero()) {
    StopAutoscroll();
    return;
  }

  drag_and_drop_autoscroll_reference_position_ = event_position + offset;

  if (autoscroll_type_ == kNoAutoscroll) {
    autoscroll_type_ = kAutoscrollForDragAndDrop;
    autoscroll_layout_object_ = scrollable;
    drag_and_drop_autoscroll_start_time_ = event_time;
    UseCounter::Count(page_->MainFrame(), UseCounter::kDragAndDropScrollStart);
    StartAutoscroll();
  } else if (autoscroll_layout_object_ != scrollable) {
    drag_and_drop_autoscroll_start_time_ = event_time;
    autoscroll_layout_object_ = scrollable;
  }
}

void AutoscrollController::HandleMouseReleaseForMiddleClickAutoscroll(
    LocalFrame* frame,
    const WebMouseEvent& mouse_event) {
  DCHECK(RuntimeEnabledFeatures::middleClickAutoscrollEnabled());
  if (!frame->IsMainFrame())
    return;
  switch (autoscroll_type_) {
    case kAutoscrollForMiddleClick:
      if (mouse_event.button == WebPointerProperties::Button::kMiddle)
        autoscroll_type_ = kAutoscrollForMiddleClickCanStop;
      break;
    case kAutoscrollForMiddleClickCanStop:
      StopAutoscroll();
      break;
    case kAutoscrollForDragAndDrop:
    case kAutoscrollForSelection:
    case kNoAutoscroll:
      // Nothing to do.
      break;
  }
}

bool AutoscrollController::MiddleClickAutoscrollInProgress() const {
  return autoscroll_type_ == kAutoscrollForMiddleClickCanStop ||
         autoscroll_type_ == kAutoscrollForMiddleClick;
}

void AutoscrollController::StartMiddleClickAutoscroll(
    LayoutBox* scrollable,
    const IntPoint& last_known_mouse_position) {
  DCHECK(RuntimeEnabledFeatures::middleClickAutoscrollEnabled());
  // We don't want to trigger the autoscroll or the middleClickAutoscroll if
  // it's already active.
  if (autoscroll_type_ != kNoAutoscroll)
    return;

  autoscroll_type_ = kAutoscrollForMiddleClick;
  autoscroll_layout_object_ = scrollable;
  middle_click_autoscroll_start_pos_ = last_known_mouse_position;
  did_latch_for_middle_click_autoscroll_ = false;

  UseCounter::Count(page_->MainFrame(),
                    UseCounter::kMiddleClickAutoscrollStart);
  StartAutoscroll();
}

static inline int AdjustedScrollDelta(int beginning_delta) {
  // This implemention matches Firefox's.
  // http://mxr.mozilla.org/firefox/source/toolkit/content/widgets/browser.xml#856.
  const int kSpeedReducer = 12;

  int adjusted_delta = beginning_delta / kSpeedReducer;
  if (adjusted_delta > 1) {
    adjusted_delta =
        static_cast<int>(adjusted_delta *
                         sqrt(static_cast<double>(adjusted_delta))) -
        1;
  } else if (adjusted_delta < -1) {
    adjusted_delta =
        static_cast<int>(adjusted_delta *
                         sqrt(static_cast<double>(-adjusted_delta))) +
        1;
  }

  return adjusted_delta;
}

static inline IntSize AdjustedScrollDelta(const IntSize& delta) {
  return IntSize(AdjustedScrollDelta(delta.Width()),
                 AdjustedScrollDelta(delta.Height()));
}

FloatSize AutoscrollController::CalculateAutoscrollDelta() {
  LocalFrame* frame = autoscroll_layout_object_->GetFrame();
  if (!frame)
    return FloatSize();

  IntPoint last_known_mouse_position =
      frame->GetEventHandler().LastKnownMousePosition();

  // We need to check if the last known mouse position is out of the window.
  // When the mouse is out of the window, the position is incoherent
  static IntPoint previous_mouse_position;
  if (last_known_mouse_position.X() < 0 || last_known_mouse_position.Y() < 0)
    last_known_mouse_position = previous_mouse_position;
  else
    previous_mouse_position = last_known_mouse_position;

  IntSize delta =
      last_known_mouse_position - middle_click_autoscroll_start_pos_;

  // at the center we let the space for the icon.
  if (abs(delta.Width()) <= kNoMiddleClickAutoscrollRadius)
    delta.SetWidth(0);
  if (abs(delta.Height()) <= kNoMiddleClickAutoscrollRadius)
    delta.SetHeight(0);
  return FloatSize(AdjustedScrollDelta(delta));
}

void AutoscrollController::Animate(double) {
  if (!autoscroll_layout_object_ || !autoscroll_layout_object_->GetFrame()) {
    StopAutoscroll();
    return;
  }

  EventHandler& event_handler =
      autoscroll_layout_object_->GetFrame()->GetEventHandler();
  IntSize offset = autoscroll_layout_object_->CalculateAutoscrollDirection(
      event_handler.LastKnownMousePosition());
  IntPoint selection_point = event_handler.LastKnownMousePosition() + offset;
  switch (autoscroll_type_) {
    case kAutoscrollForDragAndDrop:
      if ((TimeTicks::Now() - drag_and_drop_autoscroll_start_time_) >
          kAutoscrollDelay)
        autoscroll_layout_object_->Autoscroll(
            drag_and_drop_autoscroll_reference_position_);
      break;
    case kAutoscrollForSelection:
      if (!event_handler.MousePressed()) {
        StopAutoscroll();
        return;
      }
      event_handler.UpdateSelectionForMouseDrag();
      autoscroll_layout_object_->Autoscroll(selection_point);
      break;
    case kNoAutoscroll:
      break;
    case kAutoscrollForMiddleClickCanStop:
    case kAutoscrollForMiddleClick:
      DCHECK(RuntimeEnabledFeatures::middleClickAutoscrollEnabled());
      if (!MiddleClickAutoscrollInProgress()) {
        StopAutoscroll();
        return;
      }
      if (FrameView* view = autoscroll_layout_object_->GetFrame()->View())
        UpdateMiddleClickAutoscrollState(
            view, event_handler.LastKnownMousePosition());
      FloatSize delta = CalculateAutoscrollDelta();
      if (delta.IsZero())
        break;
      ScrollResult result =
          autoscroll_layout_object_->Scroll(kScrollByPixel, delta);
      LayoutObject* layout_object = autoscroll_layout_object_;
      while (!did_latch_for_middle_click_autoscroll_ && !result.DidScroll()) {
        if (layout_object->GetNode() &&
            layout_object->GetNode()->IsDocumentNode()) {
          Element* owner = ToDocument(layout_object->GetNode())->LocalOwner();
          layout_object = owner ? owner->GetLayoutObject() : nullptr;
        } else {
          layout_object = layout_object->Parent();
        }
        if (!layout_object) {
          break;
        }
        if (layout_object && layout_object->IsBox() &&
            ToLayoutBox(layout_object)->CanBeScrolledAndHasScrollableArea())
          result = ToLayoutBox(layout_object)->Scroll(kScrollByPixel, delta);
      }
      if (result.DidScroll()) {
        did_latch_for_middle_click_autoscroll_ = true;
        autoscroll_layout_object_ = ToLayoutBox(layout_object);
      }
      break;
  }
  if (autoscroll_type_ != kNoAutoscroll && autoscroll_layout_object_) {
    page_->GetChromeClient().ScheduleAnimation(
        autoscroll_layout_object_->GetFrame()->View());
  }
}

void AutoscrollController::StartAutoscroll() {
  page_->GetChromeClient().ScheduleAnimation(
      autoscroll_layout_object_->GetFrame()->View());
}

void AutoscrollController::UpdateMiddleClickAutoscrollState(
    FrameView* view,
    const IntPoint& last_known_mouse_position) {
  DCHECK(RuntimeEnabledFeatures::middleClickAutoscrollEnabled());
  // At the original click location we draw a 4 arrowed icon. Over this icon
  // there won't be any scroll, So don't change the cursor over this area.
  bool east = middle_click_autoscroll_start_pos_.X() <
              (last_known_mouse_position.X() - kNoMiddleClickAutoscrollRadius);
  bool west = middle_click_autoscroll_start_pos_.X() >
              (last_known_mouse_position.X() + kNoMiddleClickAutoscrollRadius);
  bool north = middle_click_autoscroll_start_pos_.Y() >
               (last_known_mouse_position.Y() + kNoMiddleClickAutoscrollRadius);
  bool south = middle_click_autoscroll_start_pos_.Y() <
               (last_known_mouse_position.Y() - kNoMiddleClickAutoscrollRadius);

  if (autoscroll_type_ == kAutoscrollForMiddleClick &&
      (east || west || north || south))
    autoscroll_type_ = kAutoscrollForMiddleClickCanStop;

  if (north) {
    if (east)
      view->SetCursor(NorthEastPanningCursor());
    else if (west)
      view->SetCursor(NorthWestPanningCursor());
    else
      view->SetCursor(NorthPanningCursor());
  } else if (south) {
    if (east)
      view->SetCursor(SouthEastPanningCursor());
    else if (west)
      view->SetCursor(SouthWestPanningCursor());
    else
      view->SetCursor(SouthPanningCursor());
  } else if (east) {
    view->SetCursor(EastPanningCursor());
  } else if (west) {
    view->SetCursor(WestPanningCursor());
  } else {
    view->SetCursor(MiddlePanningCursor());
  }
}

}  // namespace blink
