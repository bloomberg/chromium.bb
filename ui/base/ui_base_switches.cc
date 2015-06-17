// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_switches.h"

namespace switches {

#if defined(OS_MACOSX) && !defined(OS_IOS)
// Disable use of cross-process CALayers to display content directly from the
// GPU process on Mac.
const char kDisableRemoteCoreAnimation[] = "disable-remote-core-animation";

// Disable using the private NSCGLSurface API to draw content.
const char kDisableNSCGLSurfaceApi[] = "disable-ns-cgl-surface-api";

// Force all content to draw via the private NSCGLSurface API, even when there
// exist performance, stability, or correctness reasons not to.
const char kForceNSCGLSurfaceApi[] = "force-ns-cgl-surface-api";
#endif

// Disables use of DWM composition for top level windows.
const char kDisableDwmComposition[] = "disable-dwm-composition";

// Disables large icons on the New Tab page.
const char kDisableIconNtp[] = "disable-icon-ntp";

// Disables touch adjustment.
const char kDisableTouchAdjustment[] = "disable-touch-adjustment";

// Disables touch event based drag and drop.
const char kDisableTouchDragDrop[] = "disable-touch-drag-drop";

// Disables controls that support touch base text editing.
const char kDisableTouchEditing[] = "disable-touch-editing";

// Disables additional visual feedback to touch input.
const char kDisableTouchFeedback[] = "disable-touch-feedback";

// Enables large icons on the New Tab page.
const char kEnableIconNtp[] = "enable-icon-ntp";

// Enables a zoomed popup bubble that allows the user to select a link.
const char kEnableLinkDisambiguationPopup[] =
    "enable-link-disambiguation-popup";

// Enables touch event based drag and drop.
const char kEnableTouchDragDrop[] = "enable-touch-drag-drop";

// Enables controls that support touch base text editing.
const char kEnableTouchEditing[] = "enable-touch-editing";

// The language file that we want to try to open. Of the form
// language[-country] where language is the 2 letter code from ISO-639.
const char kLang[] = "lang";

// On Windows only: requests that Chrome connect to the running Metro viewer
// process.
const char kViewerConnect[] = "connect-to-metro-viewer";

}  // namespace switches
