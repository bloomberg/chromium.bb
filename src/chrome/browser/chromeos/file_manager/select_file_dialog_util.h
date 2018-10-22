// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides utilities related to the select file dialog.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_SELECT_FILE_DIALOG_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_SELECT_FILE_DIALOG_UTIL_H_

#include "ui/shell_dialogs/select_file_dialog.h"

namespace file_manager {
namespace util {

// Get file dialog title string from its type.
base::string16 GetSelectFileDialogTitle(ui::SelectFileDialog::Type type);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_SELECT_FILE_DIALOG_UTIL_H_
