// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dialogs/select_file_dialog.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "build/build_config.h"
#include "ui/base/dialogs/selected_file_info.h"
#include "ui/base/dialogs/select_file_dialog_factory.h"
#include "ui/base/dialogs/select_file_policy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/linux_ui.h"

#if defined(OS_WIN)
#include "ui/base/dialogs/select_file_dialog_win.h"
#elif defined(OS_MACOSX)
#include "ui/base/dialogs/select_file_dialog_mac.h"
#elif defined(TOOLKIT_GTK)
#include "ui/base/dialogs/gtk/select_file_dialog_impl.h"
#elif defined(OS_ANDROID)
#include "ui/base/dialogs/select_file_dialog_android.h"
#endif

namespace {

// Optional dialog factory. Leaked.
ui::SelectFileDialogFactory* dialog_factory_ = NULL;

}  // namespace

namespace ui {

SelectFileDialog::FileTypeInfo::FileTypeInfo()
    : include_all_files(false),
      support_gdata(false) {}

SelectFileDialog::FileTypeInfo::~FileTypeInfo() {}

void SelectFileDialog::Listener::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file,
    int index,
    void* params) {
  // Most of the dialogs need actual local path, so default to it.
  FileSelected(file.local_path, index, params);
}

void SelectFileDialog::Listener::MultiFilesSelectedWithExtraInfo(
    const std::vector<ui::SelectedFileInfo>& files,
    void* params) {
  std::vector<FilePath> file_paths;
  for (size_t i = 0; i < files.size(); ++i)
    file_paths.push_back(files[i].local_path);

  MultiFilesSelected(file_paths, params);
}

// static
void SelectFileDialog::SetFactory(ui::SelectFileDialogFactory* factory) {
  delete dialog_factory_;
  dialog_factory_ = factory;
}

// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener,
                                           ui::SelectFilePolicy* policy) {
  if (dialog_factory_) {
    SelectFileDialog* dialog = dialog_factory_->Create(listener, policy);
    if (dialog)
      return dialog;
  }

#if defined(USE_AURA) && !defined(USE_ASH) && defined(OS_LINUX)
  const ui::LinuxUI* linux_ui = ui::LinuxUI::instance();
  if (linux_ui)
    return linux_ui->CreateSelectFileDialog(listener, policy);
#endif

#if defined(OS_WIN) && !defined(USE_AURA)
  // TODO(port): The windows people need this to work in aura, too.
  return CreateWinSelectFileDialog(listener, policy);
#elif defined(OS_MACOSX) && !defined(USE_AURA)
  return CreateMacSelectFileDialog(listener, policy);
#elif defined(TOOLKIT_GTK)
  return CreateLinuxSelectFileDialog(listener, policy);
#elif defined(OS_ANDROID)
  return CreateAndroidSelectFileDialog(listener, policy);
#endif

  return NULL;
}

void SelectFileDialog::SelectFile(Type type,
                                  const string16& title,
                                  const FilePath& default_path,
                                  const FileTypeInfo* file_types,
                                  int file_type_index,
                                  const FilePath::StringType& default_extension,
                                  gfx::NativeWindow owning_window,
                                  void* params) {
  DCHECK(listener_);

  if (select_file_policy_.get() &&
      !select_file_policy_->CanOpenSelectFileDialog()) {
    select_file_policy_->SelectFileDenied();

    // Inform the listener that no file was selected.
    // Post a task rather than calling FileSelectionCanceled directly to ensure
    // that the listener is called asynchronously.
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&SelectFileDialog::CancelFileSelection, this,
                              params));
    return;
  }

  // Call the platform specific implementation of the file selection dialog.
  SelectFileImpl(type, title, default_path, file_types, file_type_index,
                 default_extension, owning_window, params);
}

bool SelectFileDialog::HasMultipleFileTypeChoices() {
  return HasMultipleFileTypeChoicesImpl();
}

SelectFileDialog::SelectFileDialog(Listener* listener,
                                   ui::SelectFilePolicy* policy)
    : listener_(listener),
      select_file_policy_(policy) {
  DCHECK(listener_);
}

SelectFileDialog::~SelectFileDialog() {}

void SelectFileDialog::CancelFileSelection(void* params) {
  if (listener_)
    listener_->FileSelectionCanceled(params);
}

}  // namespace ui
