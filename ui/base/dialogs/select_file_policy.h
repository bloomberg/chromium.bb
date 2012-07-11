// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DIALOGS_SELECT_FILE_POLICY_H_
#define UI_BASE_DIALOGS_SELECT_FILE_POLICY_H_

#include "ui/base/ui_export.h"

namespace ui {

// An optional policy class that provides decisions on whether to allow showing
// a native file dialog. Some ports need this.
class UI_EXPORT SelectFilePolicy {
 public:
  virtual ~SelectFilePolicy();

  // Returns true if the current policy allows for file selection dialogs.
  virtual bool CanOpenSelectFileDialog() = 0;

  // Called from the SelectFileDialog when we've denied a request.
  virtual void SelectFileDenied() = 0;
};

}  // namespace ui

#endif  // UI_BASE_DIALOGS_SELECT_FILE_POLICY_H_
