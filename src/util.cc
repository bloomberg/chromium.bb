// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/util.h"

#include "gestures/include/gestures.h"
#include "gestures/include/logging.h"

namespace gestures {

void CombineGestures(Gesture* gesture, const Gesture* addend) {
  if (!gesture) {
    Err("gesture must be non-NULL.");
    return;
  }
  if (!addend)
    return;
  if (gesture->type == kGestureTypeNull) {
    *gesture = *addend;
    return;
  }
  if (gesture->type == addend->type &&
      gesture->type != kGestureTypeButtonsChange) {
    // Same type; merge them
    if (gesture->type == kGestureTypeMove) {
      gesture->details.move.dx += addend->details.move.dx;
      gesture->details.move.dy += addend->details.move.dy;
    } else if (gesture->type == kGestureTypeScroll) {
      gesture->details.scroll.dx += addend->details.scroll.dx;
      gesture->details.scroll.dy += addend->details.scroll.dy;
    }
    return;
  }
  if (gesture->type != kGestureTypeButtonsChange) {
    // Either |addend| is a button gesture, or neither is. Either way, use
    // |addend|.
    Log("Losing gesture");
    *gesture = *addend;
    return;
  }
  // |gesture| must be a button gesture if we get to here.
  if (addend->type != kGestureTypeButtonsChange) {
    Err("Losing gesture");
    return;
  }

  // We have 2 button events. merge them
  unsigned buttons[] = { GESTURES_BUTTON_LEFT,
                         GESTURES_BUTTON_MIDDLE,
                         GESTURES_BUTTON_RIGHT };
  for (size_t i = 0; i < arraysize(buttons); ++i) {
    unsigned button = buttons[i];
    unsigned g_down = gesture->details.buttons.down & button;
    unsigned g_up = gesture->details.buttons.up & button;
    unsigned a_down = addend->details.buttons.down & button;
    unsigned a_up = addend->details.buttons.up & button;
    // How we merge buttons: Remember that a button gesture event can send
    // some button down events, then button up events. Ideally we can combine
    // them simply: e.g. if |gesture| has button down and |addend| has button
    // up, we can put those both into |gesture|. If there is a conflict (e.g.
    // button up followed by button down/up), there is no proper way to
    // represent that in a single gesture. We work around that case by removing
    // pairs of down/up, so in the example just given, the result would be just
    // button up. There is one exception to these two rules: if |gesture| is
    // button up, and |addend| is button down, combing them into one gesture
    // would mean a click, because when executing the gestures, the down
    // actions happen before the up. So for that case, we just remove all
    // button action.
    if (!g_down && g_up && a_down && !a_up) {
      // special case
      g_down = 0;
      g_up = 0;
    } else if ((g_down & a_down) | (g_up & a_up)) {
      // If we have a conflict, this logic seems to remove the full click.
      g_down = (~(g_down ^ a_down)) & button;
      g_up = (~(g_up ^ a_up)) & button;
    } else {
      // Non-conflict case
      g_down |= a_down;
      g_up |= a_up;
    }
    gesture->details.buttons.down =
        (gesture->details.buttons.down & ~button) | g_down;
    gesture->details.buttons.up =
        (gesture->details.buttons.up & ~button) | g_up;
  }
  if (!gesture->details.buttons.down && !gesture->details.buttons.up)
    *gesture = Gesture();
}
}  // namespace gestures
