// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DIALOGS_SELECT_FILE_DIALOG_WIN_H_
#define UI_BASE_DIALOGS_SELECT_FILE_DIALOG_WIN_H_

#include "ui/base/ui_export.h"
#include "ui/base/dialogs/select_file_dialog.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class SelectFilePolicy;

// Implementation detail exported for unit tests.
UI_EXPORT std::wstring AppendExtensionIfNeeded(
    const std::wstring& filename,
    const std::wstring& filter_selected,
    const std::wstring& suggested_ext);

// Intentionally not exported. Implementation detail of SelectFileDialog. Use
// SelectFileDialog::Create() instead.
SelectFileDialog* CreateWinSelectFileDialog(
    SelectFileDialog::Listener* listener,
    SelectFilePolicy* policy);

}  // namespace ui

#endif  //  UI_BASE_DIALOGS_SELECT_FILE_DIALOG_WIN_H_
