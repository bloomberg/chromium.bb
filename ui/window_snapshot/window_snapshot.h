// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WINDOW_SNAPSHOT_WINDOW_SNAPSHOT_H_
#define UI_WINDOW_SNAPSHOT_WINDOW_SNAPSHOT_H_

#include <vector>

#include "ui/base/ui_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
}

namespace ui {

// Grabs a snapshot of the rectangle area |snapshot_bounds| with respect to the
// top left corner of the designated window and stores a PNG representation
// into a byte vector. On Windows, |window| may be NULL to grab a snapshot of
// the primary monitor. DO NOT use in browser code. Screenshots initiated by
// user action should be taken with chrome::GrabWindowSnapshot
// (chrome/browser/ui/window_snapshot/window_snapshot.cc), so user context is
// taken into account (eg. policy settings are checked).
// Returns true if the operation is successful.
UI_EXPORT bool GrabWindowSnapshot(
    gfx::NativeWindow window,
    std::vector<unsigned char>* png_representation,
    const gfx::Rect& snapshot_bounds);

}  // namespace chrome

#endif  // UI_WINDOW_SNAPSHOT_WINDOW_SNAPSHOT_H_
