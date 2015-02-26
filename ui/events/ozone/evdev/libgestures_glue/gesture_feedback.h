// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_FEEDBACK_H_
#define UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_FEEDBACK_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_vector.h"

namespace ui {

// Touch event log paths.
const char kTouchpadGestureLogPath[] =
    "/home/chronos/user/log/touchpad_activity.txt";
const char kTouchpadEvdevLogPath[] =
    "/home/chronos/user/log/cmt_input_events.dat";

class GesturePropertyProvider;

typedef base::Callback<void(scoped_ptr<std::vector<base::FilePath>>)>
    GetTouchEventLogReply;

// Utility functions for generating gesture related logs. These logs will be
// included in user feedback reports.
void DumpTouchDeviceStatus(GesturePropertyProvider* provider,
                           std::string* status);

void DumpTouchEventLog(GesturePropertyProvider* provider,
                       const base::FilePath& out_dir,
                       scoped_ptr<std::vector<base::FilePath>> log_paths,
                       const GetTouchEventLogReply& reply);

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_FEEDBACK_H_
