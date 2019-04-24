// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ksv/keyboard_shortcut_viewer_util.h"

#include "ash/components/shortcut_viewer/public/mojom/shortcut_viewer.mojom.h"
#include "base/time/time.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace keyboard_shortcut_viewer_util {

void ToggleKeyboardShortcutViewer() {
  shortcut_viewer::mojom::ShortcutViewerPtr shortcut_viewer_ptr;
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(shortcut_viewer::mojom::kServiceName,
                           &shortcut_viewer_ptr);
  shortcut_viewer_ptr->Toggle(base::TimeTicks::Now());
}

}  // namespace keyboard_shortcut_viewer_util
