// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollState_h
#define ScrollState_h

#include <deque>
#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "core/CoreExport.h"
#include "core/dom/Element.h"
#include "core/page/scrolling/ScrollStateInit.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/scroll/ScrollStateData.h"
#include "platform/wtf/Forward.h"

namespace blink {

class Element;

class CORE_EXPORT ScrollState final
    : public GarbageCollectedFinalized<ScrollState>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ScrollState* Create(ScrollStateInit);
  static ScrollState* Create(std::unique_ptr<ScrollStateData>);

  ~ScrollState() {}

  // Web exposed methods.

  // Reduce deltas by x, y.
  void consumeDelta(double x, double y, ExceptionState&);
  // Pops the first element off of |m_scrollChain| and calls |distributeScroll|
  // on it.
  void distributeToScrollChainDescendant();
  int positionX() { return data_->position_x; };
  int positionY() { return data_->position_y; };
  // Positive when scrolling right.
  double deltaX() const { return data_->delta_x; };
  // Positive when scrolling down.
  double deltaY() const { return data_->delta_y; };
  // Positive when scrolling right.
  double deltaXHint() const { return data_->delta_x_hint; };
  // Positive when scrolling down.
  double deltaYHint() const { return data_->delta_y_hint; };
  // Indicates the smallest delta the input device can produce. 0 for
  // unquantized inputs.
  double deltaGranularity() const { return data_->delta_granularity; };
  // Positive if moving right.
  double velocityX() const { return data_->velocity_x; };
  // Positive if moving down.
  double velocityY() const { return data_->velocity_y; };
  // True for events dispatched after the users's gesture has finished.
  bool inInertialPhase() const { return data_->is_in_inertial_phase; };
  // True if this is the first event for this scroll.
  bool isBeginning() const { return data_->is_beginning; };
  // True if this is the last event for this scroll.
  bool isEnding() const { return data_->is_ending; };
  // True if this scroll is the direct result of user input.
  bool fromUserInput() const { return data_->from_user_input; };
  // True if this scroll is the result of the user interacting directly with
  // the screen, e.g., via touch.
  bool isDirectManipulation() const { return data_->is_direct_manipulation; }
  // True if this scroll is allowed to bubble upwards.
  bool shouldPropagate() const { return data_->should_propagate; };

  // Non web exposed methods.
  void ConsumeDeltaNative(double x, double y);

  // TODO(tdresser): this needs to be web exposed. See crbug.com/483091.
  void SetScrollChain(std::deque<int> scroll_chain) {
    scroll_chain_ = scroll_chain;
  }

  Element* CurrentNativeScrollingElement();
  void SetCurrentNativeScrollingElement(Element*);

  bool DeltaConsumedForScrollSequence() const {
    return data_->delta_consumed_for_scroll_sequence;
  }

  // Scroll begin and end must propagate to all nodes to ensure
  // their state is updated.
  bool FullyConsumed() const {
    return !data_->delta_x && !data_->delta_y && !data_->is_ending &&
           !data_->is_beginning;
  }

  ScrollStateData* Data() const { return data_.get(); }

  DEFINE_INLINE_TRACE() { visitor->Trace(element_); }

 private:
  ScrollState();
  explicit ScrollState(std::unique_ptr<ScrollStateData>);

  std::unique_ptr<ScrollStateData> data_;
  std::deque<int> scroll_chain_;
  Member<Element> element_;
};

}  // namespace blink

#endif  // ScrollState_h
