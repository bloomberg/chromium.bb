// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_REMOTE_VIEWS_WINDOW_H_
#define UI_BASE_COCOA_REMOTE_VIEWS_WINDOW_H_

#include "ui/base/ui_base_export.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// Returns true if the specified NSWindow corresponds to an NSWindow that is
// being viewed in a remote process.
bool UI_BASE_EXPORT IsWindowUsingRemoteViews(gfx::NativeWindow window);

// Create a transparent NSWindow that is in the same position as |window|,
// but is at the ModalPanel window level, so that it will appear over all
// other window.
NSWindow* UI_BASE_EXPORT
CreateTransparentRemoteViewsClone(gfx::NativeWindow window);

}  // namespace ui

#endif  // UI_BASE_COCOA_REMOTE_VIEWS_WINDOW_H_
