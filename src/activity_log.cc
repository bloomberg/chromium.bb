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
#include <base/stringprintf.h>
#include <base/values.h>

#include "gestures/include/logging.h"
#include "gestures/include/prop_registry.h"

// This should be set by build system:
#ifndef VCSID
#define VCSID "Unknown"
#endif  // VCSID

using base::FundamentalValue;
using std::set;
using std::string;

namespace gestures {

ActivityLog::ActivityLog(PropRegistry* prop_reg)
    : head_idx_(0), size_(0), max_fingers_(0), prop_reg_(prop_reg) {}

void ActivityLog::SetHardwareProperties(const HardwareProperties& hwprops) {
  hwprops_ = hwprops;
  max_fingers_ = hwprops.max_finger_cnt;
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
    return;
  }
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
  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    Err("Can't open file %s: %s", filename, strerror(errno));
    return;
  }
  string data = Encode();
  for (size_t bytes_written = 0; bytes_written < data.size(); ) {
    ssize_t rc = write(fd, data.c_str() + bytes_written,
                       data.size() - bytes_written);
    if (rc < 0) {
      Err("write failed: %s", strerror(errno));
      close(fd);
      return;
    }
    bytes_written += rc;
  }
  close(fd);
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

namespace {
const char kEntryPadding[] = "   ";
const char kSubEntryPadding[] = "     ";
const char kSubSubEntryPadding[] = "      ";
}

::Value* ActivityLog::EncodeHardwareProperties() const {
  DictionaryValue* ret = new DictionaryValue;
  ret->Set(kKeyHardwarePropLeft, new FundamentalValue(hwprops_.left));
  ret->Set(kKeyHardwarePropTop, new FundamentalValue(hwprops_.top));
  ret->Set(kKeyHardwarePropRight, new FundamentalValue(hwprops_.right));
  ret->Set(kKeyHardwarePropBottom, new FundamentalValue(hwprops_.bottom));
  ret->Set(kKeyHardwarePropXResolution, new FundamentalValue(hwprops_.res_x));
  ret->Set(kKeyHardwarePropYResolution, new FundamentalValue(hwprops_.res_y));
  ret->Set(kKeyHardwarePropXDpi, new FundamentalValue(hwprops_.screen_x_dpi));
  ret->Set(kKeyHardwarePropYDpi, new FundamentalValue(hwprops_.screen_y_dpi));
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
  return ret;
}

::Value* ActivityLog::EncodeHardwareState(const HardwareState& hwstate) {
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
  return ret;
}

::Value* ActivityLog::EncodeTimerCallback(stime_t timestamp) {
  DictionaryValue* ret = new DictionaryValue;
  ret->Set(kKeyType, new StringValue(kKeyTimerCallback));
  ret->Set(kKeyTimerCallbackNow, new FundamentalValue(timestamp));
  return ret;
}

::Value* ActivityLog::EncodeCallbackRequest(stime_t timestamp) {
  DictionaryValue* ret = new DictionaryValue;
  ret->Set(kKeyType, new StringValue(kKeyCallbackRequest));
  ret->Set(kKeyCallbackRequestWhen, new FundamentalValue(timestamp));
  return ret;
}

::Value* ActivityLog::EncodeGesture(const Gesture& gesture) {
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
      break;
    case kGestureTypeScroll:
      handled = true;
      ret->Set(kKeyGestureType,
               new StringValue(kValueGestureTypeScroll));
      ret->Set(kKeyGestureScrollDX,
               new FundamentalValue(gesture.details.scroll.dx));
      ret->Set(kKeyGestureScrollDY,
               new FundamentalValue(gesture.details.scroll.dy));
      break;
    case kGestureTypePinch:
      handled = true;
      ret->Set(kKeyGestureType,
               new StringValue(kValueGestureTypePinch));
      ret->Set(kKeyGesturePinchDZ,
               new FundamentalValue(gesture.details.pinch.dz));
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
      break;
  }
  if (!handled)
    ret->Set(kKeyGestureType,
             new StringValue(StringPrintf("Unhandled %d", gesture.type)));
  return ret;
}

::Value* ActivityLog::EncodePropChange(const PropChangeEntry& prop_change) {
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

::Value* ActivityLog::EncodePropRegistry() {
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

string ActivityLog::Encode() {
  DictionaryValue root;
  root.Set("version", new FundamentalValue(1));
  string gestures_version = VCSID;

  // Strip tailing whitespace.
  TrimWhitespaceASCII(gestures_version, TRIM_ALL, &gestures_version);
  root.Set("gesturesVersion", new StringValue(gestures_version));
  root.Set(kKeyProperties, EncodePropRegistry());
  root.Set(kKeyHardwarePropRoot, EncodeHardwareProperties());

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
  root.Set(kKeyRoot, entries);
  string out;
  base::JSONWriter::Write(&root, true, &out);
  return out;
}

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
const char ActivityLog::kKeyGestureStartTime[] = "startTime";
const char ActivityLog::kKeyGestureEndTime[] = "endTime";
const char ActivityLog::kKeyGestureMoveDX[] = "dx";
const char ActivityLog::kKeyGestureMoveDY[] = "dy";
const char ActivityLog::kKeyGestureScrollDX[] = "dx";
const char ActivityLog::kKeyGestureScrollDY[] = "dy";
const char ActivityLog::kKeyGesturePinchDZ[] = "dz";
const char ActivityLog::kKeyGestureButtonsChangeDown[] = "down";
const char ActivityLog::kKeyGestureButtonsChangeUp[] = "up";
const char ActivityLog::kKeyGestureFlingVX[] = "vx";
const char ActivityLog::kKeyGestureFlingVY[] = "vy";
const char ActivityLog::kKeyGestureFlingState[] = "flingState";
const char ActivityLog::kKeyGestureSwipeDX[] = "dx";
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
const char ActivityLog::kKeyHardwarePropMaxFingerCount[] = "maxFingerCount";
const char ActivityLog::kKeyHardwarePropMaxTouchCount[] = "maxTouchCount";
const char ActivityLog::kKeyHardwarePropSupportsT5R2[] = "supportsT5R2";
const char ActivityLog::kKeyHardwarePropSemiMt[] = "semiMt";
const char ActivityLog::kKeyHardwarePropIsButtonPad[] = "isButtonPad";

const char ActivityLog::kKeyProperties[] = "properties";


}  // namespace gestures
