// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_BASE_WINDOW_PROPERTIES_H_
#define CHROMEOS_UI_BASE_WINDOW_PROPERTIES_H_

#include <string>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/class_property.h"

namespace gfx {
class Rect;
}

namespace chromeos {

enum class WindowStateType;
enum class WindowPinType;

// Shell-specific window property keys for use by ash and lacros clients.

// Alphabetical sort.

// Whether resizable windows equal to or larger than the screen should be
// automatically maximized. Affects Exo's xdg-shell clients only.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kAutoMaximizeXdgShellEnabled;

// If set to true, the window will be replaced by a black rectangle when taking
// screenshot for assistant. Used to preserve privacy for incognito windows.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kBlockedForAssistantSnapshotKey;

// If true, the window can attach into another window.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kCanAttachToAnotherWindowKey;

// If true, the window is the target window for the tab-dragged window. The key
// is used by overview to show a highlight indication to indicate which overview
// window the dragged tabs will merge into when the user releases the pointer.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const
    kIsDeferredTabDraggingTargetWindowKey;

// Whether holding esc should exit fullscreen. Used by Borealis and Plugin VM.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kEscHoldToExitFullscreen;

// Whether screen should minimize when using esc hold to exit fullscreen.
// Borealis apps set this since they do not handle window size changes.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kEscHoldExitFullscreenToMinimized;

// A property key to store the active color on the window frame.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<SkColor>* const kFrameActiveColorKey;

// A property key to store the inactive color on the window frame.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<SkColor>* const kFrameInactiveColorKey;

// A property key that is set to true when the window frame should look like it
// is in restored state, but actually isn't. Set while dragging a maximized
// window.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kFrameRestoreLookKey;

// Whether the shelf should be hidden when this window is put into fullscreen.
// Exposed because some windows want to explicitly opt-out of this.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kHideShelfWhenFullscreenKey;

// Whether entering fullscreen means that a window should automatically enter
// immersive mode. This is false for some client windows, such as Chrome Apps.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kImmersiveImpliedByFullscreen;

// Whether immersive is currently active (in ImmersiveFullscreenController
// parlance, "enabled").
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kImmersiveIsActive;

// The bounds of the top container in screen coordinates.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<gfx::Rect*>* const
    kImmersiveTopContainerBoundsInScreen;

// If true, the window is currently showing in overview mode.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kIsShowingInOverviewKey;

// A property key to tell if the window's opacity should be managed by WM.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kWindowManagerManagesOpacityKey;

// A property key to indicate ash's extended window state.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<WindowStateType>* const kWindowStateTypeKey;

// A property key whose value is shown in alt-tab/overview mode. If non-value
// is set, the window's title is used.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<std::u16string*>* const kWindowOverviewTitleKey;

// Alphabetical sort.

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_WINDOW_PROPERTIES_H_
