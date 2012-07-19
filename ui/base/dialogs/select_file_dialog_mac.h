// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DIALOGS_SELECT_FILE_DIALOG_MAC_H_
#define UI_BASE_DIALOGS_SELECT_FILE_DIALOG_MAC_H_

#include "ui/base/dialogs/select_file_dialog.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

SelectFileDialog* CreateMacSelectFileDialog(
    SelectFileDialog::Listener* listener,
    SelectFilePolicy* policy);

}  // namespace ui

#endif  //  UI_BASE_DIALOGS_SELECT_FILE_DIALOG_MAC_H_
