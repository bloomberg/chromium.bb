// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_BROWSER_WINDOW_H_
#define COMPONENTS_REMOTE_COCOA_BROWSER_WINDOW_H_

#include "components/remote_cocoa/browser/remote_cocoa_browser_export.h"
#include "ui/gfx/native_widget_types.h"

namespace remote_cocoa {

// Returns true if the specified NSWindow corresponds to an NSWindow that is
// being viewed in a remote process.
bool REMOTE_COCOA_BROWSER_EXPORT IsWindowRemote(gfx::NativeWindow window);

// Create a transparent NSWindow that is in the same position as |window|,
// but is at the ModalPanel window level, so that it will appear over all
// other window.
NSWindow* REMOTE_COCOA_BROWSER_EXPORT
CreateInProcessTransparentClone(gfx::NativeWindow window);

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_BROWSER_WINDOW_H_
