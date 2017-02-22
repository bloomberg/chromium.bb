// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_AURA_CONSTANTS_H_
#define UI_AURA_CLIENT_AURA_CONSTANTS_H_

#include <string>

#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace ui {
namespace mojom {
enum class WindowType;
}
}

namespace aura {
namespace client {

// Alphabetical sort.

// A property key to store whether accessibility focus falls back to widget or
// not.
AURA_EXPORT extern const WindowProperty<bool>* const
    kAccessibilityFocusFallsbackToWidgetKey;

// A property key to store always-on-top flag.
AURA_EXPORT extern const WindowProperty<bool>* const kAlwaysOnTopKey;

// A property key to store whether animations are disabled for the window. Type
// of value is an int.
AURA_EXPORT extern const WindowProperty<bool>* const kAnimationsDisabledKey;

// A property key to store the app icon, typically larger for shelf icons, etc.
AURA_EXPORT extern const WindowProperty<gfx::ImageSkia*>* const kAppIconKey;

// A property key to store the string id of the app associated with this window.
AURA_EXPORT extern const WindowProperty<std::string*>* const kAppIdKey;

// A property key to store the type of window that will be used to record
// pointer metrics. See AppType in ash/shared/app_types.h for more details.
AURA_EXPORT extern const WindowProperty<int>* const kAppType;

// A property key to store if a window is a constrained window or not.
AURA_EXPORT extern const WindowProperty<bool>* const kConstrainedWindowKey;

// A property key to store if a window was created by a user gesture.
AURA_EXPORT extern const WindowProperty<bool>* const kCreatedByUserGesture;

// A property key to indicate that a window should show that it deserves
// attention.
AURA_EXPORT extern const aura::WindowProperty<bool>* const kDrawAttentionKey;

// A property key to store the host window of a window. This lets
// WebContentsViews find the windows that should constrain NPAPI plugins.
AURA_EXPORT extern const WindowProperty<Window*>* const kHostWindowKey;

// A property key to indicate that a window should be in immersive mode when the
// window enters the fullscreen mode. The immersive fullscreen mode is slightly
// different from the normal fullscreen mode by allowing the user to reveal the
// top portion of the window through a touch / mouse gesture.
AURA_EXPORT extern const aura::WindowProperty<bool>* const
    kImmersiveFullscreenKey;

// A property key to indicate that a window is being "mirrored" and its contents
// should render regardless of its actual visibility state.
AURA_EXPORT extern const aura::WindowProperty<bool>* const kMirroringEnabledKey;

// A property key to store the window modality.
AURA_EXPORT extern const WindowProperty<ui::ModalType>* const kModalKey;

// A property key to store the name of the window; mostly used for debugging.
AURA_EXPORT extern const WindowProperty<std::string*>* const kNameKey;

// A property key to store the preferred size of the window.
AURA_EXPORT extern const WindowProperty<gfx::Size*>* const kPreferredSize;

// A property key to store ui::WindowShowState for restoring a window from
// minimized show state.
// Used in Ash to remember the show state before the window was minimized.
AURA_EXPORT extern const WindowProperty<ui::WindowShowState>* const
    kPreMinimizedShowStateKey;

// A property key to store ui::WindowShowState for restoring a window from
// fullscreen show state.
// Used in Ash to remember the show state before the window was fullscreen.
AURA_EXPORT extern const WindowProperty<ui::WindowShowState>* const
    kPreFullscreenShowStateKey;

// A property key to store the resize behavior, which is a bitmask of the
// ui::mojom::kResizeBehavior values.
AURA_EXPORT extern const WindowProperty<int32_t>* const kResizeBehaviorKey;

// A property key to store the restore bounds for a window.
AURA_EXPORT extern const WindowProperty<gfx::Rect*>* const kRestoreBoundsKey;

// A property key to store ui::WindowShowState for a window.
// See ui/base/ui_base_types.h for its definition.
AURA_EXPORT extern const WindowProperty<ui::WindowShowState>* const
    kShowStateKey;

// A property key to store the title of the window; sometimes shown to users.
AURA_EXPORT extern const WindowProperty<base::string16*>* const kTitleKey;

// The inset of the topmost view in the client view from the top of the
// non-client view. The topmost view depends on the window type. The topmost
// view is the tab strip for tabbed browser windows, the toolbar for popups,
// the web contents for app windows and varies for fullscreen windows.
AURA_EXPORT extern const aura::WindowProperty<int>* const kTopViewInset;

// The color of the window header.
AURA_EXPORT extern const aura::WindowProperty<SkColor>* const kTopViewColor;

// A property key to store the window icon, typically 16x16 for title bars.
AURA_EXPORT extern const WindowProperty<gfx::ImageSkia*>* const kWindowIconKey;

// Set to true if this window is a top level window in the window manager.
AURA_EXPORT extern const aura::WindowProperty<bool>* const kTopLevelWindowInWM;

AURA_EXPORT extern const aura::WindowProperty<ui::mojom::WindowType>* const
    kWindowTypeKey;

// Alphabetical sort.

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_AURA_CONSTANTS_H_
