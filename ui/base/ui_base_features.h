// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_FEATURES_H_
#define UI_BASE_UI_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/base/ui_base_export.h"
#include "ui/base/ui_features.h"

namespace features {

// Keep sorted!
UI_BASE_EXPORT extern const base::Feature kEnableFloatingVirtualKeyboard;
UI_BASE_EXPORT extern const base::Feature kSecondaryUiMd;
UI_BASE_EXPORT extern const base::Feature kTouchableAppContextMenu;

UI_BASE_EXPORT bool IsTouchableAppContextMenuEnabled();

#if defined(OS_WIN)
UI_BASE_EXPORT extern const base::Feature kDirectManipulationStylus;
UI_BASE_EXPORT extern const base::Feature kPointerEventsForTouch;
UI_BASE_EXPORT extern const base::Feature kPrecisionTouchpad;
UI_BASE_EXPORT extern const base::Feature kTSFImeSupport;

// Returns true if the system should use WM_POINTER events for touch events.
UI_BASE_EXPORT bool IsUsingWMPointerForTouch();
#endif  // defined(OS_WIN)

// TODO(sky): rename this to something that better conveys what it means.
UI_BASE_EXPORT extern const base::Feature kMash;
// WARNING: generally you should only use this in tests to enable the feature.
// Outside of tests use IsMusEnabled() to detect if mus is enabled.
// TODO(sky): rename this to kWindowService.
UI_BASE_EXPORT extern const base::Feature kMus;

// Returns true if mus (the Window Service) is enabled.
// NOTE: this returns true if either kMus or kMash is specified.
// TODO(sky): rename this to IsWindowServiceEnabled().
UI_BASE_EXPORT bool IsMusEnabled();

#if defined(OS_MACOSX)
// Returns true if the NSWindows for apps will be created in the app's process,
// and will forward input to the browser process.
UI_BASE_EXPORT bool HostWindowsInAppShimProcess();

#if BUILDFLAG(MAC_VIEWS_BROWSER)
UI_BASE_EXPORT extern const base::Feature kViewsBrowserWindows;

// Returns whether a Views-capable browser build should use the Cocoa browser
// UI.
UI_BASE_EXPORT bool IsViewsBrowserCocoa();
#endif  //  BUILDFLAG(MAC_VIEWS_BROWSER)
#endif  //  defined(OS_MACOSX)

}  // namespace features

#endif  // UI_BASE_UI_BASE_FEATURES_H_
