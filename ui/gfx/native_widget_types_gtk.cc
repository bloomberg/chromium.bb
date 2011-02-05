// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_widget_types.h"

#include "ui/gfx/gtk_native_view_id_manager.h"

namespace gfx {

NativeViewId IdFromNativeView(NativeView view) {
  return GtkNativeViewManager::GetInstance()->GetIdForWidget(view);
}

}  // namespace gfx
