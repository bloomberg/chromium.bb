// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/activity_log.h"

#include <errno.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/string_util.h>

#include "gestures/include/logging.h"

using std::string;

namespace gestures {

ActivityLog::ActivityLog() : head_idx_(0), size_(0), max_fingers_(0) {}

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

string ActivityLog::EncodeHardwareProperties() const {
  static const char kTrue[] = "true";
  static const char kFalse[] = "false";
  return StringPrintf("{\"%s\": %f,\n%s\"%s\": %f,\n"
                      "%s\"%s\": %f,\n%s\"%s\": %f,\n"
                      "%s\"%s\": %f,\n%s\"%s\": %f,\n"
                      "%s\"%s\": %f,\n%s\"%s\": %f,\n"
                      "%s\"%s\": %d,\n%s\"%s\": %d"
                      ",\n%s\"%s\": %s,\n%s\"%s\": %s,\n%s\"%s\": %s}",
                      kKeyHardwarePropLeft, hwprops_.left,
                      kEntryPadding,
                      kKeyHardwarePropTop, hwprops_.top,
                      kEntryPadding,
                      kKeyHardwarePropRight, hwprops_.right,
                      kEntryPadding,
                      kKeyHardwarePropBottom, hwprops_.bottom,
                      kEntryPadding,
                      kKeyHardwarePropXResolution, hwprops_.res_x,
                      kEntryPadding,
                      kKeyHardwarePropYResolution, hwprops_.res_y,
                      kEntryPadding,
                      kKeyHardwarePropXDpi, hwprops_.screen_x_dpi,
                      kEntryPadding,
                      kKeyHardwarePropYDpi, hwprops_.screen_y_dpi,
                      kEntryPadding,
                      kKeyHardwarePropMaxFingerCount, hwprops_.max_finger_cnt,
                      kEntryPadding,
                      kKeyHardwarePropMaxTouchCount, hwprops_.max_touch_cnt,
                      kEntryPadding,
                      kKeyHardwarePropSupportsT5R2,
                      hwprops_.supports_t5r2 ? kTrue : kFalse,
                      kEntryPadding,
                      kKeyHardwarePropSemiMt,
                      hwprops_.support_semi_mt ? kTrue : kFalse,
                      kEntryPadding,
                      kKeyHardwarePropIsButtonPad,
                      hwprops_.is_button_pad ? kTrue : kFalse);
}

string ActivityLog::EncodeHardwareState(const HardwareState& hwstate) {
  string ret = StringPrintf("{\"%s\": \"%s\",\n%s\"%s\": %d,\n%s\"%s\": %d,\n"
                            "%s\"%s\": %f,\n%s\"%s\": [%s",
                            kKeyType, kKeyHardwareState,
                            kEntryPadding,
                            kKeyHardwareStateButtonsDown, hwstate.buttons_down,
                            kEntryPadding,
                            kKeyHardwareStateTouchCnt, hwstate.touch_cnt,
                            kEntryPadding,
                            kKeyHardwareStateTimestamp, hwstate.timestamp,
                            kEntryPadding,
                            kKeyHardwareStateFingers,
                            hwstate.finger_cnt ? "\n" : "");
  for (size_t i = 0; i < hwstate.finger_cnt; ++i) {
    const FingerState& fs = hwstate.fingers[i];
    ret += StringPrintf("%s%s{\"%s\": %f,\n%s\"%s\": %f,\n%s\"%s\": "
                        "%f,\n%s\"%s\": %f,\n%s\"%s\": %f,\n%s\"%s\": "
                        "%f,\n%s\"%s\": %f,\n%s\"%s\": %f"
                        ",\n%s\"%s\": %d}",
                        i == 0 ? "" : ",\n",
                        kSubEntryPadding,
                        kKeyFingerStateTouchMajor, fs.touch_major,
                        kSubSubEntryPadding,
                        kKeyFingerStateTouchMinor, fs.touch_minor,
                        kSubSubEntryPadding,
                        kKeyFingerStateWidthMajor, fs.width_major,
                        kSubSubEntryPadding,
                        kKeyFingerStateWidthMinor, fs.width_minor,
                        kSubSubEntryPadding,
                        kKeyFingerStatePressure, fs.pressure,
                        kSubSubEntryPadding,
                        kKeyFingerStateOrientation, fs.orientation,
                        kSubSubEntryPadding,
                        kKeyFingerStatePositionX, fs.position_x,
                        kSubSubEntryPadding,
                        kKeyFingerStatePositionY, fs.position_y,
                        kSubSubEntryPadding,
                        kKeyFingerStateTrackingId, fs.tracking_id);
  }
  ret += "]}";
  return ret;
}

string ActivityLog::EncodeTimerCallback(stime_t timestamp) {
  return StringPrintf("{\"%s\": \"%s\", \"%s\": %f}",
                      kKeyType, kKeyTimerCallback,
                      kKeyTimerCallbackNow, timestamp);
}

string ActivityLog::EncodeCallbackRequest(stime_t timestamp) {
  return StringPrintf("{\"%s\": \"%s\", \"%s\": %f}",
                      kKeyType, kKeyCallbackRequest,
                      kKeyCallbackRequestWhen, timestamp);
}

string ActivityLog::EncodeGesture(const Gesture& gesture) {
  string ret = StringPrintf("{\"%s\": \"%s\",\n%s\"%s\": %f,\n"
                            "%s\"%s\": %f,\n%s",
                            kKeyType, kKeyGesture,
                            kEntryPadding,
                            kKeyGestureStartTime, gesture.start_time,
                            kEntryPadding,
                            kKeyGestureEndTime, gesture.end_time,
                            kEntryPadding);
  switch (gesture.type) {
    case kGestureTypeNull:
      return ret + StringPrintf("\"%s\": \"null\"}", kKeyGestureType);
    case kGestureTypeContactInitiated:
      return ret + StringPrintf("\"%s\": \"%s\"}", kKeyGestureType,
                                kValueGestureTypeContactInitiated);
    case kGestureTypeMove:
      return ret + StringPrintf(
          "\"%s\": \"%s\",\n%s\"%s\": %f,\n%s\"%s\": %f}",
          kKeyGestureType, kValueGestureTypeMove,
          kEntryPadding,
          kKeyGestureMoveDX, gesture.details.move.dx,
          kEntryPadding,
          kKeyGestureMoveDY, gesture.details.move.dy);
    case kGestureTypeScroll:
      return ret + StringPrintf(
          "\"%s\": \"%s\",\n%s\"%s\": %f,\n%s\"%s\": %f}",
          kKeyGestureType, kValueGestureTypeScroll,
          kEntryPadding,
          kKeyGestureScrollDX, gesture.details.scroll.dx,
          kEntryPadding,
          kKeyGestureScrollDY, gesture.details.scroll.dy);
    case kGestureTypeButtonsChange:
      return ret + StringPrintf(
          "\"%s\": \"%s\",\n%s\"%s\": %d,\n%s\"%s\": %d}",
          kKeyGestureType, kValueGestureTypeButtonsChange,
          kEntryPadding,
          kKeyGestureButtonsChangeDown, gesture.details.buttons.down,
          kEntryPadding,
          kKeyGestureButtonsChangeUp, gesture.details.buttons.up);
  }
  return ret + StringPrintf("\"%s\": \"Unhandled %d\"}", kKeyGestureType,
                            gesture.type);
}

string ActivityLog::Encode() {
  string ret(StringPrintf("{\"version\": 1,\n  \"%s\":\n  %s,\n  \"%s\": [",
                          kKeyHardwarePropRoot,
                          EncodeHardwareProperties().c_str(),
                          kKeyRoot));
  for (size_t i = 0; i < size_; ++i) {
    size_t idx = (head_idx_ + i) % kBufferSize;
    if (i != 0)
      ret += ",";
    ret += "\n  ";
    const Entry& entry = buffer_[idx];
    switch (entry.type) {
      case kHardwareState:
        ret += EncodeHardwareState(entry.details.hwstate);
        break;
      case kTimerCallback:
        ret += EncodeTimerCallback(entry.details.timestamp);
        break;
      case kCallbackRequest:
        ret += EncodeCallbackRequest(entry.details.timestamp);
        break;
      case kGesture:
        ret += EncodeGesture(entry.details.gesture);
        break;
    }
    Err("Unknown entry type %d", entry.type);
  }
  ret += "]}";
  return ret;
}

const char ActivityLog::kKeyRoot[] = "entries";
const char ActivityLog::kKeyType[] = "type";
const char ActivityLog::kKeyHardwareState[] = "hardwareState";
const char ActivityLog::kKeyTimerCallback[] = "timerCallback";
const char ActivityLog::kKeyCallbackRequest[] = "callbackRequest";
const char ActivityLog::kKeyGesture[] = "gesture";
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
const char ActivityLog::kKeyTimerCallbackNow[] = "now";
const char ActivityLog::kKeyCallbackRequestWhen[] = "when";
const char ActivityLog::kKeyGestureType[] = "gestureType";
const char ActivityLog::kValueGestureTypeContactInitiated[] =
    "contactInitiated";
const char ActivityLog::kValueGestureTypeMove[] = "move";
const char ActivityLog::kValueGestureTypeScroll[] = "scroll";
const char ActivityLog::kValueGestureTypeButtonsChange[] = "buttonsChange";
const char ActivityLog::kKeyGestureStartTime[] = "startTime";
const char ActivityLog::kKeyGestureEndTime[] = "endTime";
const char ActivityLog::kKeyGestureMoveDX[] = "dx";
const char ActivityLog::kKeyGestureMoveDY[] = "dy";
const char ActivityLog::kKeyGestureScrollDX[] = "dx";
const char ActivityLog::kKeyGestureScrollDY[] = "dy";
const char ActivityLog::kKeyGestureButtonsChangeDown[] = "down";
const char ActivityLog::kKeyGestureButtonsChangeUp[] = "up";
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

}  // namespace gestures
