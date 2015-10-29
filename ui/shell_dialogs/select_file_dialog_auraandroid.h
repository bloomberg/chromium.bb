// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_ANDROID_SELECT_FILE_DIALOG_ANDROID_H_
#define UI_SHELL_DIALOGS_ANDROID_SELECT_FILE_DIALOG_ANDROID_H_

#include "base/macros.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace ui {

class SelectFileDialogImpl : public SelectFileDialog {
 public:
  static SelectFileDialogImpl* Create(Listener* listener,
                                      SelectFilePolicy* policy);

  // From SelectFileDialog
  bool IsRunning(gfx::NativeWindow) const override;
  void ListenerDestroyed() override;
  void SelectFileImpl(SelectFileDialog::Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const SelectFileDialog::FileTypeInfo* file_types,
                      int file_type_index,
                      const std::string& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override;

 protected:
  ~SelectFileDialogImpl() override;

 private:
  SelectFileDialogImpl(Listener* listener,  SelectFilePolicy* policy);

  bool HasMultipleFileTypeChoicesImpl() override;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImpl);
};

SelectFileDialog* CreateAndroidSelectFileDialog(
    SelectFileDialog::Listener* listener,
    SelectFilePolicy* policy);

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_ANDROID_SELECT_FILE_DIALOG_ANDROID_H_

