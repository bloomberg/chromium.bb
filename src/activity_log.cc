// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/activity_log.h"

#include <errno.h>
#include <fcntl.h>
#include <set>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/json/json_writer.h>
#include <base/values.h>

#include "gestures/include/file_util.h"
#include "gestures/include/logging.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/string_util.h"

// This should be set by build system:
#ifndef VCSID
#define VCSID "Unknown"
#endif  // VCSID

#define QUINTTAP_COUNT 5  /* BTN_TOOL_QUINTTAP - Five fingers on trackpad */

using base::DictionaryValue;
using base::FundamentalValue;
using base::ListValue;
using base::StringValue;
using base::Value;
using std::set;
using std::string;

namespace gestures {

ActivityLog::ActivityLog(PropRegistry* prop_reg)
    : head_idx_(0), size_(0), max_fingers_(0), hwprops_(),
      prop_reg_(prop_reg) {}

void ActivityLog::SetHardwareProperties(const HardwareProperties& hwprops) {
  hwprops_ = hwprops;

  // For old devices(such as mario, alex, zgb..), the reporting touch count
  // or 'max_touch_cnt' will be less than number of slots or 'max_finger_cnt'
  // they support. As kernel evdev drivers do not have a bitset to report
  // touch count greater than five (bitset for five-fingers gesture is
  // BTN_TOOL_QUINTAP), we respect 'max_finger_cnt' than 'max_touch_cnt'
  // reported from kernel driver as the 'max_fingers_' instead.
  if (hwprops.max_touch_cnt < QUINTTAP_COUNT) {
    max_fingers_ = std::min<size_t>(hwprops.max_finger_cnt,
                                    hwprops.max_touch_cnt);
  } else {
    max_fingers_ = std::max<size_t>(hwprops.max_finger_cnt,
                                    hwprops.max_touch_cnt);
  }

  finger_states_.reset(new FingerState[kBufferSize * max_fingers_]);
}

void ActivityLog::LogHardwareState(const HardwareState& hwstate) {
  Entry* entry = PushBack();
  entry->type = kHardwareState;
  entry->details.hwstate = hwstate;
  if (hwstate.finger_cnt > max_fingers_) {
    Err("Too many fingers! Max is %zu, but I got %d",
        max_fingers_, hwstate.finger_cnt);
    entry->details.hwstate.fingers = NULL;
    entry->details.hwstate.finger_cnt = 0;
    return;
  }
  if (!finger_states_.get())
    return;
  entry->details.hwstate.fingers = &finger_states_[TailIdx() * max_fingers_];
  std::copy(&hwstate.fingers[0], &hwstate.fingers[hwstate.finger_cnt],
            entry->details.hwstate.fingers);
}

void ActivityLog::LogTimerCallback(stime_t now) {
  Entry* entry = PushBack();
  entry->type = kTimerCallback;
  entry->details.timestamp = now;
}

void ActivityLog::LogCallbackRequest(stime_t when) {
  Entry* entry = PushBack();
  entry->type = kCallbackRequest;
  entry->details.timestamp = when;
}

void ActivityLog::LogGesture(const Gesture& gesture) {
  Entry* entry = PushBack();
  entry->type = kGesture;
  entry->details.gesture = gesture;
}

void ActivityLog::LogPropChange(const PropChangeEntry& prop_change) {
  Entry* entry = PushBack();
  entry->type = kPropChange;
  entry->details.prop_change = prop_change;
}

void ActivityLog::Dump(const char* filename) {
  string data = Encode();
  WriteFile(filename, data.c_str(), data.size());
}

ActivityLog::Entry* ActivityLog::PushBack() {
  if (size_ == kBufferSize) {
    Entry* ret = &buffer_[head_idx_];
    head_idx_ = (head_idx_ + 1) % kBufferSize;
    return ret;
  }
  ++size_;
  return &buffer_[TailIdx()];
}

Value* ActivityLog::EncodeHardwareProperties() const {
  DictionaryValue* ret = new DictionaryValue;
  ret->Set(kKeyHardwarePropLeft, new FundamentalValue(hwprops_.left));
  ret->Set(kKeyHardwarePropTop, new FundamentalValue(hwprops_.top));
  ret->Set(kKeyHardwarePropRight, new FundamentalValue(hwprops_.right));
  ret->Set(kKeyHardwarePropBottom, new FundamentalValue(hwprops_.bottom));
  ret->Set(kKeyHardwarePropXResolution, new FundamentalValue(hwprops_.res_x));
  ret->Set(kKeyHardwarePropYResolution, new FundamentalValue(hwprops_.res_y));
  ret->Set(kKeyHardwarePropXDpi, new FundamentalValue(hwprops_.screen_x_dpi));
  ret->Set(kKeyHardwarePropYDpi, new FundamentalValue(hwprops_.screen_y_dpi));
  ret->Set(kKeyHardwarePropOrientationMinimum,
           new FundamentalValue(hwprops_.orientation_minimum));
  ret->Set(kKeyHardwarePropOrientationMaximum,
           new FundamentalValue(hwprops_.orientation_maximum));
  ret->Set(kKeyHardwarePropMaxFingerCount,
           new FundamentalValue(hwprops_.max_finger_cnt));
  ret->Set(kKeyHardwarePropMaxTouchCount,
           new FundamentalValue(hwprops_.max_touch_cnt));

  ret->Set(kKeyHardwarePropSupportsT5R2,
           new FundamentalValue(hwprops_.supports_t5r2 != 0));
  ret->Set(kKeyHardwarePropSemiMt,
           new FundamentalValue(hwprops_.support_semi_mt != 0));
  ret->Set(kKeyHardwarePropIsButtonPad,
           new FundamentalValue(hwprops_.is_button_pad != 0));
  ret->Set(kKeyHardwarePropHasWheel,
           new FundamentalValue(hwprops_.has_wheel != 0));
  return ret;
}

Value* ActivityLog::EncodeHardwareState(const HardwareState& hwstate) {
  DictionaryValue* ret = new DictionaryValue;
  ret->Set(kKeyType, new StringValue(kKeyHardwareState));
  ret->Set(kKeyHardwareStateButtonsDown,
           new FundamentalValue(hwstate.buttons_down));
  ret->Set(kKeyHardwareStateTouchCnt,
           new FundamentalValue(hwstate.touch_cnt));
  ret->Set(kKeyHardwareStateTimestamp,
           new FundamentalValue(hwstate.timestamp));
  ListValue* fingers = new ListValue;
  for (size_t i = 0; i < hwstate.finger_cnt; ++i) {
    if (hwstate.fingers == NULL) {
      Err("Have finger_cnt %d but fingers is NULL!", hwstate.finger_cnt);
      break;
    }
    const FingerState& fs = hwstate.fingers[i];
    DictionaryValue* finger = new DictionaryValue;
    finger->Set(kKeyFingerStateTouchMajor,
                new FundamentalValue(fs.touch_major));
    finger->Set(kKeyFingerStateTouchMinor,
                new FundamentalValue(fs.touch_minor));
    finger->Set(kKeyFingerStateWidthMajor,
                new FundamentalValue(fs.width_major));
    finger->Set(kKeyFingerStateWidthMinor,
                new FundamentalValue(fs.width_minor));
    finger->Set(kKeyFingerStatePressure,
                new FundamentalValue(fs.pressure));
    finger->Set(kKeyFingerStateOrientation,
                new FundamentalValue(fs.orientation));
    finger->Set(kKeyFingerStatePositionX,
                new FundamentalValue(fs.position_x));
    finger->Set(kKeyFingerStatePositionY,
                new FundamentalValue(fs.position_y));
    finger->Set(kKeyFingerStateTrackingId,
                new FundamentalValue(fs.tracking_id));
    finger->Set(kKeyFingerStateFlags,
                new FundamentalValue(static_cast<int>(fs.flags)));
    fingers->Append(finger);
  }
  ret->Set(kKeyHardwareStateFingers, fingers);
  ret->Set(kKeyHardwareStateRelX,
           new FundamentalValue(hwstate.rel_x));
  ret->Set(kKeyHardwareStateRelY,
           new FundamentalValue(hwstate.rel_y));
  ret->Set(kKeyHardwareStateRelWheel,
           new FundamentalValue(hwstate.rel_wheel));
  ret->Set(kKeyHardwareStateRelHWheel,
           new FundamentalValue(hwstate.rel_hwheel));
  return ret;
}

Value* ActivityLog::EncodeTimerCallback(stime_t timestamp) {
  DictionaryValue* ret = new DictionaryValue;
  ret->Set(kKeyType, new StringValue(kKeyTimerCallback));
  ret->Set(kKeyTimerCallbackNow, new FundamentalValue(timestamp));
  return ret;
}

Value* ActivityLog::EncodeCallbackRequest(stime_t timestamp) {
  DictionaryValue* ret = new DictionaryValue;
  ret->Set(kKeyType, new StringValue(kKeyCallbackRequest));
  ret->Set(kKeyCallbackRequestWhen, new FundamentalValue(timestamp));
  return ret;
}

Value* ActivityLog::EncodeGesture(const Gesture& gesture) {
  DictionaryValue* ret = new DictionaryValue;
  ret->Set(kKeyType, new StringValue(kKeyGesture));
  ret->Set(kKeyGestureStartTime, new FundamentalValue(gesture.start_time));
  ret->Set(kKeyGestureEndTime, new FundamentalValue(gesture.end_time));

  bool handled = false;
  switch (gesture.type) {
    case kGestureTypeNull:
      handled = true;
      ret->Set(kKeyGestureType, new StringValue("null"));
      break;
    case kGestureTypeContactInitiated:
      handled = true;
      ret->Set(kKeyGestureType,
               new StringValue(kValueGestureTypeContactInitiated));
      break;
    case kGestureTypeMove:
      handled = true;
      ret->Set(kKeyGestureType,
               new StringValue(kValueGestureTypeMove));
      ret->Set(kKeyGestureMoveDX,
               new FundamentalValue(gesture.details.move.dx));
      ret->Set(kKeyGestureMoveDY,
               new FundamentalValue(gesture.details.move.dy));
      ret->Set(kKeyGestureMoveOrdinalDX,
               new FundamentalValue(gesture.details.move.ordinal_dx));
      ret->Set(kKeyGestureMoveOrdinalDY,
               new FundamentalValue(gesture.details.move.ordinal_dy));
      break;
    case kGestureTypeScroll:
      handled = true;
      ret->Set(kKeyGestureType,
               new StringValue(kValueGestureTypeScroll));
      ret->Set(kKeyGestureScrollDX,
               new FundamentalValue(gesture.details.scroll.dx));
      ret->Set(kKeyGestureScrollDY,
               new FundamentalValue(gesture.details.scroll.dy));
      ret->Set(kKeyGestureScrollOrdinalDX,
               new FundamentalValue(gesture.details.scroll.ordinal_dx));
      ret->Set(kKeyGestureScrollOrdinalDY,
               new FundamentalValue(gesture.details.scroll.ordinal_dy));
      break;
    case kGestureTypePinch:
      handled = true;
      ret->Set(kKeyGestureType,
               new StringValue(kValueGestureTypePinch));
      ret->Set(kKeyGesturePinchDZ,
               new FundamentalValue(gesture.details.pinch.dz));
      ret->Set(kKeyGesturePinchOrdinalDZ,
               new FundamentalValue(gesture.details.pinch.ordinal_dz));
      break;
    case kGestureTypeButtonsChange:
      handled = true;
      ret->Set(kKeyGestureType,
               new StringValue(kValueGestureTypeButtonsChange));
      ret->Set(kKeyGestureButtonsChangeDown,
               new FundamentalValue(
                   static_cast<int>(gesture.details.buttons.down)));
      ret->Set(kKeyGestureButtonsChangeUp,
               new FundamentalValue(
                   static_cast<int>(gesture.details.buttons.up)));
      break;
    case kGestureTypeFling:
      handled = true;
      ret->Set(kKeyGestureType,
               new StringValue(kValueGestureTypeFling));
      ret->Set(kKeyGestureFlingVX,
               new FundamentalValue(gesture.details.fling.vx));
      ret->Set(kKeyGestureFlingVY,
               new FundamentalValue(gesture.details.fling.vy));
      ret->Set(kKeyGestureFlingOrdinalVX,
               new FundamentalValue(gesture.details.fling.ordinal_vx));
      ret->Set(kKeyGestureFlingOrdinalVY,
               new FundamentalValue(gesture.details.fling.ordinal_vy));
      ret->Set(kKeyGestureFlingState,
               new FundamentalValue(
                   static_cast<int>(gesture.details.fling.fling_state)));
      break;
    case kGestureTypeSwipe:
      handled = true;
      ret->Set(kKeyGestureType,
               new StringValue(kValueGestureTypeSwipe));
      ret->Set(kKeyGestureSwipeDX,
               new FundamentalValue(gesture.details.swipe.dx));
      ret->Set(kKeyGestureSwipeDY,
               new FundamentalValue(gesture.details.swipe.dy));
      ret->Set(kKeyGestureSwipeOrdinalDX,
               new FundamentalValue(gesture.details.swipe.ordinal_dx));
      ret->Set(kKeyGestureSwipeOrdinalDY,
               new FundamentalValue(gesture.details.swipe.ordinal_dy));
      break;
    case kGestureTypeSwipeLift:
      handled = true;
      ret->Set(kKeyGestureType,
               new StringValue(kValueGestureTypeSwipeLift));
      break;
    case kGestureTypeMetrics:
      handled = true;
      ret->Set(kKeyGestureType,
               new StringValue(kValueGestureTypeMetrics));
      ret->Set(kKeyGestureMetricsType,
               new FundamentalValue(
                   static_cast<int>(gesture.details.metrics.type)));
      ret->Set(kKeyGestureMetricsData1,
               new FundamentalValue(gesture.details.metrics.data[0]));
      ret->Set(kKeyGestureMetricsData2,
               new FundamentalValue(gesture.details.metrics.data[1]));
      break;
  }
  if (!handled)
    ret->Set(kKeyGestureType,
             new StringValue(StringPrintf("Unhandled %d", gesture.type)));
  return ret;
}

Value* ActivityLog::EncodePropChange(const PropChangeEntry& prop_change) {
  DictionaryValue* ret = new DictionaryValue;
  ret->Set(kKeyType, new StringValue(kKeyPropChange));
  ret->Set(kKeyPropChangeName, new StringValue(prop_change.name));
  FundamentalValue* val = NULL;
  StringValue* type = NULL;
  switch (prop_change.type) {
    case PropChangeEntry::kBoolProp:
      val = new FundamentalValue(static_cast<bool>(prop_change.value.bool_val));
      type = new StringValue(kValuePropChangeTypeBool);
      break;
    case PropChangeEntry::kDoubleProp:
      val = new FundamentalValue(prop_change.value.double_val);
      type = new StringValue(kValuePropChangeTypeDouble);
      break;
    case PropChangeEntry::kIntProp:
      val = new FundamentalValue(prop_change.value.int_val);
      type = new StringValue(kValuePropChangeTypeInt);
      break;
    case PropChangeEntry::kShortProp:
      val = new FundamentalValue(prop_change.value.short_val);
      type = new StringValue(kValuePropChangeTypeShort);
      break;
  }
  if (val)
    ret->Set(kKeyPropChangeValue, val);
  if (type)
    ret->Set(kKeyPropChangeType, type);
  return ret;
}

Value* ActivityLog::EncodePropRegistry() {
  DictionaryValue* ret = new DictionaryValue;
  if (!prop_reg_)
    return ret;

  const set<Property*>& props = prop_reg_->props();
  for (set<Property*>::const_iterator it = props.begin(), e = props.end();
       it != e; ++it) {
    ret->Set((*it)->name(), (*it)->NewValue());
  }
  return ret;
}

DictionaryValue* ActivityLog::EncodeCommonInfo() {
  DictionaryValue* root = new DictionaryValue;

  ListValue* entries = new ListValue;
  for (size_t i = 0; i < size_; ++i) {
    const Entry& entry = buffer_[(i + head_idx_) % kBufferSize];
    switch (entry.type) {
      case kHardwareState:
        entries->Append(EncodeHardwareState(entry.details.hwstate));
        continue;
      case kTimerCallback:
        entries->Append(EncodeTimerCallback(entry.details.timestamp));
        continue;
      case kCallbackRequest:
        entries->Append(EncodeCallbackRequest(entry.details.timestamp));
        continue;
      case kGesture:
        entries->Append(EncodeGesture(entry.details.gesture));
        continue;
      case kPropChange:
        entries->Append(EncodePropChange(entry.details.prop_change));
        continue;
    }
    Err("Unknown entry type %d", entry.type);
  }
  root->Set(kKeyRoot, entries);
  root->Set(kKeyHardwarePropRoot, EncodeHardwareProperties());

  return root;
}

DictionaryValue* ActivityLog::AddEncodeInfo(DictionaryValue* root) {
  root->Set("version", new FundamentalValue(1));
  string gestures_version = VCSID;

  // Strip tailing whitespace.
  TrimWhitespaceASCII(gestures_version, TRIM_ALL, &gestures_version);
  root->Set("gesturesVersion", new StringValue(gestures_version));
  root->Set(kKeyProperties, EncodePropRegistry());

  return root;
}

string ActivityLog::Encode() {
  DictionaryValue *root;
  root = EncodeCommonInfo();
  root = AddEncodeInfo(root);

  string out;
  base::JSONWriter::WriteWithOptions(root,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &out);
  delete root;
  return out;
}

const char ActivityLog::kKeyInterpreterName[] = "interpreterName";
const char ActivityLog::kKeyNext[] = "nextLayer";
const char ActivityLog::kKeyRoot[] = "entries";
const char ActivityLog::kKeyType[] = "type";
const char ActivityLog::kKeyHardwareState[] = "hardwareState";
const char ActivityLog::kKeyTimerCallback[] = "timerCallback";
const char ActivityLog::kKeyCallbackRequest[] = "callbackRequest";
const char ActivityLog::kKeyGesture[] = "gesture";
const char ActivityLog::kKeyPropChange[] = "propertyChange";
const char ActivityLog::kKeyHardwareStateTimestamp[] = "timestamp";
const char ActivityLog::kKeyHardwareStateButtonsDown[] = "buttonsDown";
const char ActivityLog::kKeyHardwareStateTouchCnt[] = "touchCount";
const char ActivityLog::kKeyHardwareStateFingers[] = "fingers";
const char ActivityLog::kKeyHardwareStateRelX[] = "relX";
const char ActivityLog::kKeyHardwareStateRelY[] = "relY";
const char ActivityLog::kKeyHardwareStateRelWheel[] = "relWheel";
const char ActivityLog::kKeyHardwareStateRelHWheel[] = "relHWheel";
const char ActivityLog::kKeyFingerStateTouchMajor[] = "touchMajor";
const char ActivityLog::kKeyFingerStateTouchMinor[] = "touchMinor";
const char ActivityLog::kKeyFingerStateWidthMajor[] = "widthMajor";
const char ActivityLog::kKeyFingerStateWidthMinor[] = "widthMinor";
const char ActivityLog::kKeyFingerStatePressure[] = "pressure";
const char ActivityLog::kKeyFingerStateOrientation[] = "orientation";
const char ActivityLog::kKeyFingerStatePositionX[] = "positionX";
const char ActivityLog::kKeyFingerStatePositionY[] = "positionY";
const char ActivityLog::kKeyFingerStateTrackingId[] = "trackingId";
const char ActivityLog::kKeyFingerStateFlags[] = "flags";
const char ActivityLog::kKeyTimerCallbackNow[] = "now";
const char ActivityLog::kKeyCallbackRequestWhen[] = "when";
const char ActivityLog::kKeyGestureType[] = "gestureType";
const char ActivityLog::kValueGestureTypeContactInitiated[] =
    "contactInitiated";
const char ActivityLog::kValueGestureTypeMove[] = "move";
const char ActivityLog::kValueGestureTypeScroll[] = "scroll";
const char ActivityLog::kValueGestureTypePinch[] = "pinch";
const char ActivityLog::kValueGestureTypeButtonsChange[] = "buttonsChange";
const char ActivityLog::kValueGestureTypeFling[] = "fling";
const char ActivityLog::kValueGestureTypeSwipe[] = "swipe";
const char ActivityLog::kValueGestureTypeSwipeLift[] = "swipeLift";
const char ActivityLog::kValueGestureTypeMetrics[] = "metrics";
const char ActivityLog::kKeyGestureStartTime[] = "startTime";
const char ActivityLog::kKeyGestureEndTime[] = "endTime";
const char ActivityLog::kKeyGestureMoveDX[] = "dx";
const char ActivityLog::kKeyGestureMoveDY[] = "dy";
const char ActivityLog::kKeyGestureMoveOrdinalDX[] = "ordinalDx";
const char ActivityLog::kKeyGestureMoveOrdinalDY[] = "ordinalDy";
const char ActivityLog::kKeyGestureScrollDX[] = "dx";
const char ActivityLog::kKeyGestureScrollDY[] = "dy";
const char ActivityLog::kKeyGestureScrollOrdinalDX[] = "ordinalDx";
const char ActivityLog::kKeyGestureScrollOrdinalDY[] = "ordinalDy";
const char ActivityLog::kKeyGesturePinchDZ[] = "dz";
const char ActivityLog::kKeyGesturePinchOrdinalDZ[] = "ordinalDz";
const char ActivityLog::kKeyGestureButtonsChangeDown[] = "down";
const char ActivityLog::kKeyGestureButtonsChangeUp[] = "up";
const char ActivityLog::kKeyGestureFlingVX[] = "vx";
const char ActivityLog::kKeyGestureFlingVY[] = "vy";
const char ActivityLog::kKeyGestureFlingOrdinalVX[] = "ordinalVx";
const char ActivityLog::kKeyGestureFlingOrdinalVY[] = "ordinalVy";
const char ActivityLog::kKeyGestureFlingState[] = "flingState";
const char ActivityLog::kKeyGestureSwipeDX[] = "dx";
const char ActivityLog::kKeyGestureSwipeDY[] = "dy";
const char ActivityLog::kKeyGestureSwipeOrdinalDX[] = "ordinalDx";
const char ActivityLog::kKeyGestureSwipeOrdinalDY[] = "ordinalDy";
const char ActivityLog::kKeyGestureMetricsType[] = "metricsType";
const char ActivityLog::kKeyGestureMetricsData1[] = "data1";
const char ActivityLog::kKeyGestureMetricsData2[] = "data2";
const char ActivityLog::kKeyPropChangeType[] = "propChangeType";
const char ActivityLog::kKeyPropChangeName[] = "name";
const char ActivityLog::kKeyPropChangeValue[] = "value";
const char ActivityLog::kValuePropChangeTypeBool[] = "bool";
const char ActivityLog::kValuePropChangeTypeDouble[] = "double";
const char ActivityLog::kValuePropChangeTypeInt[] = "int";
const char ActivityLog::kValuePropChangeTypeShort[] = "short";
const char ActivityLog::kKeyHardwarePropRoot[] = "hardwareProperties";
const char ActivityLog::kKeyHardwarePropLeft[] = "left";
const char ActivityLog::kKeyHardwarePropTop[] = "top";
const char ActivityLog::kKeyHardwarePropRight[] = "right";
const char ActivityLog::kKeyHardwarePropBottom[] = "bottom";
const char ActivityLog::kKeyHardwarePropXResolution[] = "xResolution";
const char ActivityLog::kKeyHardwarePropYResolution[] = "yResolution";
const char ActivityLog::kKeyHardwarePropXDpi[] = "xDpi";
const char ActivityLog::kKeyHardwarePropYDpi[] = "yDpi";
const char ActivityLog::kKeyHardwarePropOrientationMinimum[] =
    "orientationMinimum";
const char ActivityLog::kKeyHardwarePropOrientationMaximum[] =
    "orientationMaximum";
const char ActivityLog::kKeyHardwarePropMaxFingerCount[] = "maxFingerCount";
const char ActivityLog::kKeyHardwarePropMaxTouchCount[] = "maxTouchCount";
const char ActivityLog::kKeyHardwarePropSupportsT5R2[] = "supportsT5R2";
const char ActivityLog::kKeyHardwarePropSemiMt[] = "semiMt";
const char ActivityLog::kKeyHardwarePropIsButtonPad[] = "isButtonPad";
const char ActivityLog::kKeyHardwarePropHasWheel[] = "hasWheel";

const char ActivityLog::kKeyProperties[] = "properties";


}  // namespace gestures
