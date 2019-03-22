// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ELEMENT_AREA_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ELEMENT_AREA_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/selector.h"

namespace autofill_assistant {
class WebController;

// A helper that keeps track of the area on the screen that correspond to an
// changeable set of elements.
class ElementArea {
 public:
  // |web_controller| must remain valid for the lifetime of this instance.
  explicit ElementArea(WebController* web_controller);
  ~ElementArea();

  // Updates the set of elements to check and keep updating them as long as
  // there are elements to check.
  //
  // The area is updated asynchronously, so Contains will not work right away.
  void SetElements(const std::vector<Selector>& elements);

  // Clears the set of elements to check.
  void ClearElements() { SetElements({}); }

  // Forces an out-of-schedule update of the positions right away.
  //
  // This method is never strictly necessary. It is useful to call it when
  // there's a reason to think the positions might have changed, to speed up
  // updates.
  //
  // Does nothing if the area is empty.
  void UpdatePositions();

  // Checks whether the given position is in the element area.
  //
  // Coordinates are values between 0 and 1 relative to the size of the visible
  // viewport.
  bool Contains(float x, float y) const;

  // Returns true if there are no elements to check or if the elements don't
  // exist.
  bool IsEmpty() const;

  // Returns true if there are elements to check.
  bool HasElements() const { return !element_positions_.empty(); }

  // Defines a callback that'll be run every time the set of element coordinates
  // changes.
  //
  // The first argument is true if there are any elements in the area. The
  // second reports the areas that corresponds to currently known elements,
  // which might be empty.
  void SetOnUpdate(
      base::RepeatingCallback<void(bool, const std::vector<RectF>& areas)> cb) {
    on_update_ = cb;
  }

 private:
  // A rectangle that corresponds to the area of the visual viewport covered by
  // an element. Coordinates are values between 0 and 1, relative to the size of
  // the visible viewport.
  struct ElementPosition {
    Selector selector;
    RectF rect;

    ElementPosition();
    ElementPosition(const ElementPosition& orig);
    ~ElementPosition();
  };

  void KeepUpdatingPositions();
  void OnGetElementPosition(const Selector& selector,
                            bool found,
                            const RectF& rect);
  void ReportUpdate();

  WebController* const web_controller_;
  std::vector<ElementPosition> element_positions_;

  // If true, regular updates are currently scheduled.
  bool scheduled_update_;

  base::RepeatingCallback<void(bool, const std::vector<RectF>& areas)>
      on_update_;

  base::WeakPtrFactory<ElementArea> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ElementArea);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ELEMENT_AREA_H_
