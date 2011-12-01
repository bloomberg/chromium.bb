// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_AURA_CONSTANTS_H_
#define UI_AURA_CLIENT_AURA_CONSTANTS_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace aura {
// Window property keys that are shared between aura_shell and chrome/views.

// A property key to store always-on-top flag. The type of the value is boolean.
AURA_EXPORT extern const char kAlwaysOnTopKey[];

// A property key to store the restore bounds for a window. The type
// of the value is |gfx::Rect*|.
AURA_EXPORT extern const char kRestoreBoundsKey[];

// A property key to store ui::WindowShowState for a window.
// See ui/base/ui_base_types.h for its definition.
AURA_EXPORT extern const char kShowStateKey[];

// A property key to store tooltip text for a window. The type of the value
// is |string16*|.
AURA_EXPORT extern const char kTooltipTextKey[];

// A property key to store the boolean property of window modality.
AURA_EXPORT extern const char kModalKey[];

// A property key for a value from aura::ShadowType describing the drop shadow
// that should be displayed under the window.  If unset, no shadow is displayed.
AURA_EXPORT extern const char kShadowTypeKey[];

// A property key to store the drag and drop client for the desktop. The type of
// the value is |aura::DragDropClient*|.
AURA_EXPORT extern const char kDesktopDragDropClientKey[];

// A property key to store the drag and drop delegate for a window. The type of
// the value is |aura::WindowDragDropDelegate*|.
AURA_EXPORT extern const char kDragDropDelegateKey[];

// A property key to store the tooltip client for the desktop. The type of
// the value is |aura::TooltipClient*|.
AURA_EXPORT extern const char kDesktopTooltipClientKey[];
}  // namespace aura

#endif  // UI_AURA_CLIENT_AURA_CONSTANTS_H_
