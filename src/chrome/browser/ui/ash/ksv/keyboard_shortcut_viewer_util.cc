// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ksv/keyboard_shortcut_viewer_util.h"

#include "ash/components/shortcut_viewer/public/mojom/shortcut_viewer.mojom.h"
#include "ash/components/shortcut_viewer/views/keyboard_shortcut_view.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "base/time/time.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/ui_base_features.h"

namespace keyboard_shortcut_viewer_util {

void ToggleKeyboardShortcutViewer() {
  base::TimeTicks user_gesture_time = base::TimeTicks::Now();
  if (ash::features::IsKeyboardShortcutViewerAppEnabled()) {
    shortcut_viewer::mojom::ShortcutViewerPtr shortcut_viewer_ptr;
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    connector->BindInterface(shortcut_viewer::mojom::kServiceName,
                             &shortcut_viewer_ptr);
    shortcut_viewer_ptr->Toggle(user_gesture_time);
  } else {
    // A value of |null| while IsSingleProcessMash() results in the keyboard
    // shortcut viewer using DesktopNativeWidgetAura, just as all other non-ash
    // codes does in single-process-mash.
    aura::Window* context =
        features::IsSingleProcessMash()
            ? nullptr
            : ash::Shell::Get()->GetRootWindowForNewWindows();
    keyboard_shortcut_viewer::KeyboardShortcutView::Toggle(user_gesture_time,
                                                           context);
  }
}

}  // namespace keyboard_shortcut_viewer_util
