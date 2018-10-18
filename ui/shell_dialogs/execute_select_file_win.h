// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_EXECUTE_SELECT_FILE_WIN_H_
#define UI_SHELL_DIALOGS_EXECUTE_SELECT_FILE_WIN_H_

#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "base/win/windows_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace base {
class FilePath;
}

namespace ui {

// Implementation detail exported for unit tests.
SHELL_DIALOGS_EXPORT base::string16 AppendExtensionIfNeeded(
    const base::string16& filename,
    const base::string16& filter_selected,
    const base::string16& suggested_ext);

// Describes a filter for a file dialog.
struct FileFilterSpec {
  // A human readable description of this filter. E.g. "HTML Files."
  base::string16 description;
  // The different extensions that map to this spec. This is a semicolon-
  // separated list of extensions that contains a wildcard and the separator.
  // E.g. "*.html;*.htm"
  base::string16 extension_spec;
};

// Shows the file selection dialog modal to |owner| returns the selected
// file(s) and file type index via the return value. The file path vector will
// be empty on failure.
SHELL_DIALOGS_EXPORT
std::pair<std::vector<base::FilePath>, int> ExecuteSelectFile(
    SelectFileDialog::Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const std::vector<FileFilterSpec>& filter,
    int file_type_index,
    const base::string16& default_extension,
    HWND owner);

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_EXECUTE_SELECT_FILE_WIN_H_
