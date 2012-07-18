// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DIALOGS_SELECT_FILE_DIALOG_FACTORY_H_
#define UI_BASE_DIALOGS_SELECT_FILE_DIALOG_FACTORY_H_

#include "ui/base/dialogs/select_file_dialog.h"
#include "ui/base/ui_export.h"

namespace ui {
class SelectFilePolicy;

// Some chrome components want to create their own SelectFileDialog objects
// (for example, using an extension to provide the select file dialog needs to
// live in chrome/ due to the extension dependency.)
//
// They can implement a factory which creates their SelectFileDialog.
class UI_EXPORT SelectFileDialogFactory {
 public:
  virtual ~SelectFileDialogFactory();

  virtual SelectFileDialog* Create(ui::SelectFileDialog::Listener* listener,
                                   ui::SelectFilePolicy* policy) = 0;
};

}  // namespace ui

#endif  // UI_BASE_DIALOGS_SELECT_FILE_DIALOG_FACTORY_H_
