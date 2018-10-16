// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/execute_select_file_win.h"

#include <shlobj.h>
#include <wrl/client.h>

#include <tuple>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/win/com_init_util.h"
#include "base/win/registry.h"
#include "base/win/shortcut.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/open_file_name_win.h"
#include "ui/shell_dialogs/base_shell_dialog_win.h"
#include "ui/strings/grit/ui_strings.h"

namespace ui {

namespace {

// Distinguish directories from regular files.
bool IsDirectory(const base::FilePath& path) {
  base::File::Info file_info;
  return base::GetFileInfo(path, &file_info) ? file_info.is_directory
                                             : path.EndsWithSeparator();
}

// Given |extension|, if it's not empty, then remove the leading dot.
base::string16 GetExtensionWithoutLeadingDot(const base::string16& extension) {
  DCHECK(extension.empty() || extension[0] == L'.');
  return extension.empty() ? extension : extension.substr(1);
}

struct SelectFolderDialogOptions {
  const wchar_t* default_path;
  bool is_upload;
};

// Prompt the user for location to save a file.
// Callers should provide the filter string, and also a filter index.
// The parameter |index| indicates the initial index of filter description and
// filter pattern for the dialog box. If |index| is zero or greater than the
// number of total filter types, the system uses the first filter in the
// |filter| buffer. |index| is used to specify the initial selected extension,
// and when done contains the extension the user chose. The parameter |path|
// returns the file name which contains the drive designator, path, file name,
// and extension of the user selected file name. |def_ext| is the default
// extension to give to the file if the user did not enter an extension.
bool SaveFileAsWithFilter(HWND owner,
                          const base::FilePath& default_path,
                          const base::string16& filter,
                          const base::string16& def_ext,
                          DWORD* index,
                          base::FilePath* path) {
  DCHECK(path);
  // Having an empty filter makes for a bad user experience. We should always
  // specify a filter when saving.
  DCHECK(!filter.empty());

  ui::win::OpenFileName save_as(owner, OFN_OVERWRITEPROMPT | OFN_EXPLORER |
                                           OFN_ENABLESIZING | OFN_NOCHANGEDIR |
                                           OFN_PATHMUSTEXIST);

  if (!default_path.empty()) {
    base::FilePath suggested_file_name;
    base::FilePath suggested_directory;
    if (IsDirectory(default_path)) {
      suggested_directory = default_path;
    } else {
      suggested_directory = default_path.DirName();
      suggested_file_name = default_path.BaseName();
      // If the default_path is a root directory, |suggested_file_name| will be
      // '\', and the call to GetSaveFileName below will fail.
      if (suggested_file_name.value() == L"\\")
        suggested_file_name.clear();
    }
    save_as.SetInitialSelection(suggested_directory, suggested_file_name);
  }

  save_as.GetOPENFILENAME()->lpstrFilter =
      filter.empty() ? nullptr : filter.c_str();
  save_as.GetOPENFILENAME()->nFilterIndex = *index;
  save_as.GetOPENFILENAME()->lpstrDefExt = &def_ext[0];

  BOOL success = ::GetSaveFileName(save_as.GetOPENFILENAME());
  BaseShellDialogImpl::DisableOwner(owner);
  if (!success)
    return false;

  // Return the user's choice.
  *path = base::FilePath(save_as.GetOPENFILENAME()->lpstrFile);
  *index = save_as.GetOPENFILENAME()->nFilterIndex;

  // Figure out what filter got selected. The filter index is 1-based.
  base::string16 filter_selected;
  if (*index > 0) {
    std::vector<std::tuple<base::string16, base::string16>> filters =
        ui::win::OpenFileName::GetFilters(save_as.GetOPENFILENAME());
    if (*index > filters.size())
      NOTREACHED() << "Invalid filter index.";
    else
      filter_selected = std::get<1>(filters[*index - 1]);
  }

  // Get the extension that was suggested to the user (when the Save As dialog
  // was opened).
  base::string16 suggested_ext =
      GetExtensionWithoutLeadingDot(default_path.Extension());

  // If we can't get the extension from the default_path, we use the default
  // extension passed in. This is to cover cases like when saving a web page,
  // where we get passed in a name without an extension and a default extension
  // along with it.
  if (suggested_ext.empty())
    suggested_ext = def_ext;

  *path = base::FilePath(
      AppendExtensionIfNeeded(path->value(), filter_selected, suggested_ext));
  return true;
}

int CALLBACK BrowseCallbackProc(HWND window,
                                UINT message,
                                LPARAM parameter,
                                LPARAM data) {
  if (message == BFFM_INITIALIZED) {
    SelectFolderDialogOptions* options =
        reinterpret_cast<SelectFolderDialogOptions*>(data);
    if (options->is_upload) {
      SendMessage(window, BFFM_SETOKTEXT, 0,
                  reinterpret_cast<LPARAM>(
                      l10n_util::GetStringUTF16(
                          IDS_SELECT_UPLOAD_FOLDER_DIALOG_UPLOAD_BUTTON)
                          .c_str()));
    }
    if (options->default_path) {
      SendMessage(window, BFFM_SETSELECTION, TRUE,
                  reinterpret_cast<LPARAM>(options->default_path));
    }
  }
  return 0;
}

// Runs a Folder selection dialog box, passes back the selected folder in |path|
// and returns true if the user clicks OK. If the user cancels the dialog box
// the value in |path| is not modified and returns false. Run on the dialog
// thread.
bool RunSelectFolderDialog(HWND owner,
                           SelectFileDialog::Type type,
                           const base::string16& title,
                           const base::FilePath& default_path,
                           base::FilePath* path) {
  base::win::AssertComInitialized();
  DCHECK(path);
  base::string16 new_title = title;
  if (new_title.empty() && type == SelectFileDialog::SELECT_UPLOAD_FOLDER) {
    // If it's for uploading don't use default dialog title to
    // make sure we clearly tell it's for uploading.
    new_title =
        l10n_util::GetStringUTF16(IDS_SELECT_UPLOAD_FOLDER_DIALOG_TITLE);
  }

  wchar_t dir_buffer[MAX_PATH + 1];

  bool result = false;
  BROWSEINFO browse_info = {};
  browse_info.hwndOwner = owner;
  browse_info.lpszTitle = new_title.c_str();
  browse_info.pszDisplayName = dir_buffer;
  browse_info.ulFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;

  // If uploading or a default path was provided, update the BROWSEINFO
  // and set the callback function for the dialog so the strings can be set in
  // the callback.
  SelectFolderDialogOptions dialog_options = {};
  if (!default_path.empty())
    dialog_options.default_path = default_path.value().c_str();
  dialog_options.is_upload = type == SelectFileDialog::SELECT_UPLOAD_FOLDER;
  if (type == SelectFileDialog::SELECT_UPLOAD_FOLDER ||
      type == SelectFileDialog::SELECT_EXISTING_FOLDER) {
    browse_info.ulFlags |= BIF_NONEWFOLDERBUTTON;
  }
  if (dialog_options.is_upload || dialog_options.default_path) {
    browse_info.lParam = reinterpret_cast<LPARAM>(&dialog_options);
    browse_info.lpfn = &BrowseCallbackProc;
  }

  LPITEMIDLIST list = SHBrowseForFolder(&browse_info);
  BaseShellDialogImpl::DisableOwner(owner);
  if (list) {
    STRRET out_dir_buffer = {};
    out_dir_buffer.uType = STRRET_WSTR;
    Microsoft::WRL::ComPtr<IShellFolder> shell_folder;
    if (SUCCEEDED(SHGetDesktopFolder(&shell_folder))) {
      HRESULT hr = shell_folder->GetDisplayNameOf(list, SHGDN_FORPARSING,
                                                  &out_dir_buffer);
      if (SUCCEEDED(hr) && out_dir_buffer.uType == STRRET_WSTR) {
        *path = base::FilePath(out_dir_buffer.pOleStr);
        CoTaskMemFree(out_dir_buffer.pOleStr);
        result = true;
      } else {
        // Use old way if we don't get what we want.
        wchar_t old_out_dir_buffer[MAX_PATH + 1];
        if (SHGetPathFromIDList(list, old_out_dir_buffer)) {
          *path = base::FilePath(old_out_dir_buffer);
          result = true;
        }
      }

      // According to MSDN, Win2000 will not resolve shortcuts, so we do it
      // ourselves.
      base::win::ResolveShortcut(*path, path, nullptr);
    }
    CoTaskMemFree(list);
  }
  return result;
}

// Runs an Open file dialog box, with similar semantics for input parameters as
// RunSelectFolderDialog.
bool RunOpenFileDialog(HWND owner,
                       const base::string16& title,
                       const base::FilePath& default_path,
                       const base::string16& filter,
                       base::FilePath* path) {
  // We use OFN_NOCHANGEDIR so that the user can rename or delete the
  // directory without having to close Chrome first.
  ui::win::OpenFileName ofn(owner, OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR);
  if (!default_path.empty()) {
    if (IsDirectory(default_path))
      ofn.SetInitialSelection(default_path, base::FilePath());
    else
      ofn.SetInitialSelection(default_path.DirName(), default_path.BaseName());
  }
  if (!filter.empty())
    ofn.GetOPENFILENAME()->lpstrFilter = filter.c_str();

  BOOL success = ::GetOpenFileName(ofn.GetOPENFILENAME());
  BaseShellDialogImpl::DisableOwner(owner);
  if (success)
    *path = ofn.GetSingleResult();
  return success;
}

// Runs an Open file dialog box that supports multi-select, with similar
// semantics for input parameters as RunOpenFileDialog.
bool RunOpenMultiFileDialog(HWND owner,
                            const base::string16& title,
                            const base::FilePath& default_path,
                            const base::string16& filter,
                            std::vector<base::FilePath>* paths) {
  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ui::win::OpenFileName ofn(owner, OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |
                                       OFN_EXPLORER | OFN_HIDEREADONLY |
                                       OFN_ALLOWMULTISELECT | OFN_NOCHANGEDIR);
  if (!default_path.empty()) {
    if (IsDirectory(default_path))
      ofn.SetInitialSelection(default_path, base::FilePath());
    else
      ofn.SetInitialSelection(default_path.DirName(), base::FilePath());
  }
  if (!filter.empty())
    ofn.GetOPENFILENAME()->lpstrFilter = filter.c_str();

  base::FilePath directory;
  std::vector<base::FilePath> filenames;

  BOOL success = ::GetOpenFileName(ofn.GetOPENFILENAME());
  BaseShellDialogImpl::DisableOwner(owner);
  if (success)
    ofn.GetResult(&directory, &filenames);

  for (std::vector<base::FilePath>::iterator it = filenames.begin();
       it != filenames.end(); ++it) {
    paths->push_back(directory.Append(*it));
  }

  return !paths->empty();
}

}  // namespace

// This function takes the output of a SaveAs dialog: a filename, a filter and
// the extension originally suggested to the user (shown in the dialog box) and
// returns back the filename with the appropriate extension appended. If the
// user requests an unknown extension and is not using the 'All files' filter,
// the suggested extension will be appended, otherwise we will leave the
// filename unmodified. |filename| should contain the filename selected in the
// SaveAs dialog box and may include the path, |filter_selected| should be
// '*.something', for example '*.*' or it can be blank (which is treated as
// *.*). |suggested_ext| should contain the extension without the dot (.) in
// front, for example 'jpg'.
std::wstring AppendExtensionIfNeeded(const std::wstring& filename,
                                     const std::wstring& filter_selected,
                                     const std::wstring& suggested_ext) {
  DCHECK(!filename.empty());
  std::wstring return_value = filename;

  // If we wanted a specific extension, but the user's filename deleted it or
  // changed it to something that the system doesn't understand, re-append.
  // Careful: Checking net::GetMimeTypeFromExtension() will only find
  // extensions with a known MIME type, which many "known" extensions on Windows
  // don't have.  So we check directly for the "known extension" registry key.
  std::wstring file_extension(
      GetExtensionWithoutLeadingDot(base::FilePath(filename).Extension()));
  std::wstring key(L"." + file_extension);
  if (!(filter_selected.empty() || filter_selected == L"*.*") &&
      !base::win::RegKey(HKEY_CLASSES_ROOT, key.c_str(), KEY_READ).Valid() &&
      file_extension != suggested_ext) {
    if (return_value.back() != L'.')
      return_value.append(L".");
    return_value.append(suggested_ext);
  }

  // Strip any trailing dots, which Windows doesn't allow.
  size_t index = return_value.find_last_not_of(L'.');
  if (index < return_value.size() - 1)
    return_value.resize(index + 1);

  return return_value;
}

std::pair<std::vector<base::FilePath>, int> ExecuteSelectFile(
    SelectFileDialog::Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const base::string16& filter,
    int file_type_index,
    const base::string16& default_extension,
    HWND owner) {
  std::vector<base::FilePath> paths;

  DWORD filter_index = file_type_index;

  switch (type) {
    case SelectFileDialog::SELECT_FOLDER:
    case SelectFileDialog::SELECT_UPLOAD_FOLDER:
    case SelectFileDialog::SELECT_EXISTING_FOLDER: {
      base::FilePath path;
      if (RunSelectFolderDialog(owner, type, title, default_path, &path))
        paths.push_back(std::move(path));
      break;
    }
    case SelectFileDialog::SELECT_SAVEAS_FILE: {
      base::FilePath path;
      if (SaveFileAsWithFilter(owner, default_path, filter, default_extension,
                               &filter_index, &path)) {
        paths.push_back(std::move(path));
      }
      break;
    }
    case SelectFileDialog::SELECT_OPEN_FILE: {
      base::FilePath path;
      if (RunOpenFileDialog(owner, title, default_path, filter, &path))
        paths.push_back(std::move(path));
      break;
    }
    case SelectFileDialog::SELECT_OPEN_MULTI_FILE:
      RunOpenMultiFileDialog(owner, title, default_path, filter, &paths);
      break;
    case SelectFileDialog::SELECT_NONE:
      NOTREACHED();
  }

  return std::make_pair(std::move(paths), filter_index);
}

}  // namespace ui
