// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/aura_constants.h"

#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/base/class_property.h"
#include "ui/gfx/geometry/rect.h"

DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, bool)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, base::string16*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, ui::ModalType)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, gfx::ImageSkia*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, gfx::Rect*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, gfx::Size*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, std::string*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, ui::WindowShowState)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, ui::mojom::WindowType);
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, void*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, SkColor)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, int32_t)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, int64_t)

namespace aura {
namespace client {

// Alphabetical sort.

DEFINE_UI_CLASS_PROPERTY_KEY(
     bool, kAccessibilityFocusFallsbackToWidgetKey, true);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kAlwaysOnTopKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kAnimationsDisabledKey, false);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::ImageSkia, kAppIconKey, nullptr);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kAppIdKey, nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(int, kAppType, 0);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kConstrainedWindowKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kCreatedByUserGesture, false);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kDrawAttentionKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(Window*, kHostWindowKey, nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kImmersiveFullscreenKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kMirroringEnabledKey, false);
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kTopLevelWindowInWM, false);
DEFINE_UI_CLASS_PROPERTY_KEY(ui::ModalType, kModalKey, ui::MODAL_TYPE_NONE);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kNameKey, nullptr);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Size, kPreferredSize, nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(
    ui::WindowShowState, kPreMinimizedShowStateKey, ui::SHOW_STATE_DEFAULT);
DEFINE_UI_CLASS_PROPERTY_KEY(
    ui::WindowShowState, kPreFullscreenShowStateKey, ui::SHOW_STATE_DEFAULT);
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t,
                           kResizeBehaviorKey,
                           ui::mojom::kResizeBehaviorCanResize);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect, kRestoreBoundsKey, nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(
    ui::WindowShowState, kShowStateKey, ui::SHOW_STATE_DEFAULT);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(base::string16, kTitleKey, nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(int, kTopViewInset, 0);
DEFINE_UI_CLASS_PROPERTY_KEY(SkColor, kTopViewColor, SK_ColorTRANSPARENT);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::ImageSkia, kWindowIconKey, nullptr);
DEFINE_UI_CLASS_PROPERTY_KEY(ui::mojom::WindowType,
                           kWindowTypeKey,
                           ui::mojom::WindowType::UNKNOWN);

}  // namespace client
}  // namespace aura
