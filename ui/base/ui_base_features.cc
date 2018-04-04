// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_features.h"

#include "ui/base/ui_base_switches_util.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace features {

// If enabled, the emoji picker context menu item may be shown for editable
// text areas.
const base::Feature kEnableEmojiContextMenu{"EnableEmojiContextMenu",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the floating virtual keyboard behavior.
const base::Feature kEnableFloatingVirtualKeyboard = {
    "enable-floating-virtual-keyboard", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the full screen handwriting virtual keyboard behavior.
const base::Feature kEnableFullscreenHandwritingVirtualKeyboard = {
    "enable-fullscreen-handwriting-virtual-keyboard",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableStylusVirtualKeyboard = {
    "enable-stylus-virtual-keyboard", base::FEATURE_DISABLED_BY_DEFAULT};

// Applies the material design mode to elements throughout Chrome (not just top
// Chrome).
const base::Feature kSecondaryUiMd = {"SecondaryUiMd",
// Enabled by default on Windows, Mac and Desktop Linux.
// http://crbug.com/775847.
#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
                                      base::FEATURE_ENABLED_BY_DEFAULT
#else
                                      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kTouchableAppContextMenu = {
    "EnableTouchableAppContextMenu", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsTouchableAppContextMenuEnabled() {
  return base::FeatureList::IsEnabled(kTouchableAppContextMenu) ||
         switches::IsTouchableAppContextMenuEnabled();
}

#if defined(OS_WIN)
// Enables stylus appearing as touch when in contact with digitizer.
const base::Feature kDirectManipulationStylus = {
    "DirectManipulationStylus", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables using WM_POINTER instead of WM_TOUCH for touch events.
const base::Feature kPointerEventsForTouch = {"PointerEventsForTouch",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
// Enables using TSF (over IMM32) for IME.
const base::Feature kTSFImeSupport = {"TSFImeSupport",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

bool IsUsingWMPointerForTouch() {
  return base::win::GetVersion() >= base::win::VERSION_WIN8 &&
         base::FeatureList::IsEnabled(kPointerEventsForTouch);
}

// Enables DirectManipulation API for processing Precision Touchpad events.
const base::Feature kPrecisionTouchpad{"PrecisionTouchpad",
                                       base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

// Used to have ash run in its own process. This implicitly turns on the
// WindowService. That is, if this is set IsMusEnabled() returns true.
const base::Feature kMash = {"Mash", base::FEATURE_DISABLED_BY_DEFAULT};

// Used to control the mus service (aka the UI service). This makes mus run in
// process.
const base::Feature kMus = {"Mus", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsMusEnabled() {
#if defined(USE_AURA)
  return base::FeatureList::IsEnabled(features::kMus) ||
         base::FeatureList::IsEnabled(features::kMash);
#else
  return false;
#endif
}

#if defined(OS_MACOSX)
// When enabled, the NSWindows for apps will be created in the app's process,
// and will forward input to the browser process.
const base::Feature kHostWindowsInAppShimProcess{
    "HostWindowsInAppShimProcess", base::FEATURE_DISABLED_BY_DEFAULT};

bool HostWindowsInAppShimProcess() {
  return base::FeatureList::IsEnabled(kHostWindowsInAppShimProcess);
}

#if BUILDFLAG(MAC_VIEWS_BROWSER)
// Causes Views browser builds to use Views browser windows by default rather
// than Cocoa browser windows.
const base::Feature kViewsBrowserWindows{"ViewsBrowserWindows",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Returns whether a Views-capable browser build should use the Cocoa browser
// UI.
bool IsViewsBrowserCocoa() {
  return !base::FeatureList::IsEnabled(kViewsBrowserWindows);
}
#endif  //  BUILDFLAG(MAC_VIEWS_BROWSER)
#endif  //  defined(OS_MACOSX)

}  // namespace features
