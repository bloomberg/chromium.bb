// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/drag_utils.h"

#if defined(USE_AURA)
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#elif defined(OS_WIN)
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_source.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#else
#error
#endif

namespace views {

void RunShellDrag(gfx::NativeView view,
                  const ui::OSExchangeData& data,
                  const gfx::Point& location,
                  int operation) {
#if defined(USE_AURA)
  gfx::Point root_location(location);
  aura::RootWindow* root_window = view->GetRootWindow();
  aura::Window::ConvertPointToWindow(view, root_window, &root_location);
  if (aura::client::GetDragDropClient(root_window)) {
    aura::client::GetDragDropClient(root_window)->StartDragAndDrop(
        data, root_location, operation);
  }
#elif defined(OS_WIN)
  scoped_refptr<ui::DragSource> drag_source(new ui::DragSource);
  DWORD effects;
  DoDragDrop(ui::OSExchangeDataProviderWin::GetIDataObject(data),
             drag_source,
             ui::DragDropTypes::DragOperationToDropEffect(operation),
             &effects);
#endif
}

}  // namespace views
