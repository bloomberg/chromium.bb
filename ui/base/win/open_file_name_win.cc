// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/open_file_name_win.h"

#include "base/files/file_path.h"
#include "base/strings/string_util.h"

namespace ui {
namespace win {

OpenFileName::OpenFileName(HWND parent_window, DWORD flags) {
  ::ZeroMemory(&openfilename_, sizeof(openfilename_));
  openfilename_.lStructSize = sizeof(openfilename_);

  // According to http://support.microsoft.com/?scid=kb;en-us;222003&x=8&y=12,
  // The lpstrFile Buffer MUST be NULL Terminated.
  filename_buffer_[0] = 0;
  openfilename_.lpstrFile = filename_buffer_;
  openfilename_.nMaxFile = arraysize(filename_buffer_);

  openfilename_.Flags = flags;
  openfilename_.hwndOwner = parent_window;
}

OpenFileName::~OpenFileName() {
}

void OpenFileName::SetInitialSelection(const base::FilePath& initial_directory,
                                       const base::FilePath& initial_filename) {
  // First reset to the default case.
  // According to http://support.microsoft.com/?scid=kb;en-us;222003&x=8&y=12,
  // The lpstrFile Buffer MUST be NULL Terminated.
  filename_buffer_[0] = 0;
  openfilename_.lpstrFile = filename_buffer_;
  openfilename_.nMaxFile = arraysize(filename_buffer_);
  openfilename_.lpstrInitialDir = NULL;
  initial_directory_buffer_.clear();

  if (initial_directory.empty())
    return;

  initial_directory_buffer_ = initial_directory.value();
  openfilename_.lpstrInitialDir = initial_directory_buffer_.c_str();

  if (initial_filename.empty())
    return;

  // The filename is ignored if no initial directory is supplied.
  base::wcslcpy(filename_buffer_,
                initial_filename.value().c_str(),
                arraysize(filename_buffer_));
}

base::FilePath OpenFileName::GetSingleResult() {
  base::FilePath directory;
  std::vector<base::FilePath> filenames;
  GetResult(&directory, &filenames);
  if (filenames.size() != 1)
    return base::FilePath();
  return directory.Append(filenames[0]);
}

void OpenFileName::GetResult(base::FilePath* directory,
                             std::vector<base::FilePath>* filenames) {
  DCHECK(filenames->empty());
  const wchar_t* selection = openfilename_.lpstrFile;
  while (*selection) {  // Empty string indicates end of list.
    filenames->push_back(base::FilePath(selection));
    // Skip over filename and null-terminator.
    selection += filenames->back().value().length() + 1;
  }
  if (filenames->size() == 1) {
    // When there is one file, it contains the path and filename.
    *directory = (*filenames)[0].DirName();
    (*filenames)[0] = (*filenames)[0].BaseName();
  } else if (filenames->size() > 1) {
    // Otherwise, the first string is the path, and the remainder are
    // filenames.
    *directory = (*filenames)[0];
    filenames->erase(filenames->begin());
  }
}

}  // namespace win
}  // namespace ui
