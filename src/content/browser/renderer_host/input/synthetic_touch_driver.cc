// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_touch_driver.h"

#include "content/browser/renderer_host/input/synthetic_gesture_target.h"

namespace content {

SyntheticTouchDriver::SyntheticTouchDriver() {}

SyntheticTouchDriver::SyntheticTouchDriver(SyntheticWebTouchEvent touch_event)
    : touch_event_(touch_event) {}

SyntheticTouchDriver::~SyntheticTouchDriver() {}

void SyntheticTouchDriver::DispatchEvent(SyntheticGestureTarget* target,
                                         const base::TimeTicks& timestamp) {
  touch_event_.SetTimeStamp(timestamp);
  if (touch_event_.GetType() != blink::WebInputEvent::kUndefined)
    target->DispatchInputEventToPlatform(touch_event_);
  touch_event_.ResetPoints();
  ResetPointerIdIndexMap();
}

void SyntheticTouchDriver::Press(float x,
                                 float y,
                                 int index,
                                 SyntheticPointerActionParams::Button button) {
  DCHECK_GE(index, 0);
  DCHECK(pointer_id_map_.find(index) == pointer_id_map_.end());
  int touch_index = touch_event_.PressPoint(x, y);
  touch_event_.touches[touch_index].id = index;
  pointer_id_map_[index] = touch_index;
}

void SyntheticTouchDriver::Move(float x, float y, int index) {
  DCHECK_GE(index, 0);
  DCHECK(pointer_id_map_.find(index) != pointer_id_map_.end());
  touch_event_.MovePoint(pointer_id_map_[index], x, y);
}

void SyntheticTouchDriver::Release(
    int index,
    SyntheticPointerActionParams::Button button) {
  DCHECK_GE(index, 0);
  DCHECK(pointer_id_map_.find(index) != pointer_id_map_.end());
  touch_event_.ReleasePoint(pointer_id_map_[index]);
  pointer_id_map_.erase(index);
}

void SyntheticTouchDriver::Leave(int index) {
  NOTIMPLEMENTED();
}

bool SyntheticTouchDriver::UserInputCheck(
    const SyntheticPointerActionParams& params) const {
  if (params.pointer_action_type() ==
      SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED) {
    return false;
  }

  if (params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::PRESS &&
      pointer_id_map_.find(params.pointer_id()) != pointer_id_map_.end()) {
    return false;
  }

  if (params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::MOVE &&
      pointer_id_map_.find(params.pointer_id()) == pointer_id_map_.end()) {
    return false;
  }

  if (params.pointer_action_type() ==
          SyntheticPointerActionParams::PointerActionType::RELEASE &&
      pointer_id_map_.find(params.pointer_id()) == pointer_id_map_.end()) {
    return false;
  }

  return true;
}

void SyntheticTouchDriver::ResetPointerIdIndexMap() {
  unsigned free_index = 0;
  for (unsigned int i = 0; i < blink::WebTouchEvent::kTouchesLengthCap; ++i) {
    if (free_index >= touch_event_.touches_length)
      break;
    if (touch_event_.touches[i].state !=
        blink::WebTouchPoint::kStateUndefined) {
      touch_event_.touches[free_index] = touch_event_.touches[i];
      int pointer_id = GetIndexFromMap(i);
      pointer_id_map_[pointer_id] = free_index;
      free_index++;
    }
  }
}

int SyntheticTouchDriver::GetIndexFromMap(int value) const {
  PointerIdIndexMap::const_iterator it;
  for (it = pointer_id_map_.begin(); it != pointer_id_map_.end(); ++it) {
    if (it->second == value) {
      return it->first;
    }
  }
  NOTREACHED() << "Failed to find the value.";
  return -1;
}

}  // namespace content
