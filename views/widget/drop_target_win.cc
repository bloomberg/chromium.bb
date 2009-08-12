// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/drop_target_win.h"

#include "app/drag_drop_types.h"
#include "app/os_exchange_data.h"
#include "app/os_exchange_data_provider_win.h"
#include "base/gfx/point.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

namespace views {

DropTargetWin::DropTargetWin(RootView* root_view)
    : BaseDropTarget(root_view->GetWidget()->GetNativeView()),
      helper_(root_view) {
}

DropTargetWin::~DropTargetWin() {
}

void DropTargetWin::ResetTargetViewIfEquals(View* view) {
  helper_.ResetTargetViewIfEquals(view);
}

DWORD DropTargetWin::OnDragOver(IDataObject* data_object,
                                DWORD key_state,
                                POINT cursor_position,
                                DWORD effect) {
  gfx::Point root_view_location(cursor_position.x, cursor_position.y);
  View::ConvertPointToView(NULL, helper_.root_view(), &root_view_location);
  OSExchangeData data(new OSExchangeDataProviderWin(data_object));
  int drop_operation =
      helper_.OnDragOver(data, root_view_location,
                         DragDropTypes::DropEffectToDragOperation(effect));
  return DragDropTypes::DragOperationToDropEffect(drop_operation);
}

void DropTargetWin::OnDragLeave(IDataObject* data_object) {
  helper_.OnDragExit();
}

DWORD DropTargetWin::OnDrop(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect) {
  gfx::Point root_view_location(cursor_position.x, cursor_position.y);
  View::ConvertPointToView(NULL, helper_.root_view(), &root_view_location);

  OSExchangeData data(new OSExchangeDataProviderWin(data_object));
  int drop_operation = DragDropTypes::DropEffectToDragOperation(effect);
  drop_operation = helper_.OnDragOver(data, root_view_location,
                                      drop_operation);
  drop_operation = helper_.OnDrop(data, root_view_location, drop_operation);
  return DragDropTypes::DragOperationToDropEffect(drop_operation);
}

}  // namespace views
