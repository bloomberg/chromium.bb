// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "select_file_dialog_auraandroid.h"

#include "base/logging.h"

namespace ui {

// static
SelectFileDialogImpl* SelectFileDialogImpl::Create(Listener* listener,
                                                   SelectFilePolicy* policy) {
  return new SelectFileDialogImpl(listener, policy);
}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow) const {
  return listener_;
}

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = NULL;
}

void SelectFileDialogImpl::SelectFileImpl(
    SelectFileDialog::Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const std::string& default_extension,
    gfx::NativeWindow owning_window,
    void* params) {
  NOTIMPLEMENTED();
}

SelectFileDialogImpl::~SelectFileDialogImpl() {
}

SelectFileDialogImpl::SelectFileDialogImpl(Listener* listener,
                                           SelectFilePolicy* policy)
    : SelectFileDialog(listener, policy) {
}

bool SelectFileDialogImpl::HasMultipleFileTypeChoicesImpl() {
  NOTIMPLEMENTED();
  return false;
}

SelectFileDialog* CreateAndroidSelectFileDialog(
    SelectFileDialog::Listener* listener,
    SelectFilePolicy* policy) {
  return SelectFileDialogImpl::Create(listener, policy);
}

}  // namespace ui
