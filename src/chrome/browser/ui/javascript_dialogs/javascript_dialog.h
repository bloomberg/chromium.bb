// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_H_
#define CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/javascript_dialog_manager.h"

class JavaScriptDialog {
 public:
  virtual ~JavaScriptDialog() {}

  // Closes the dialog without sending a callback. This is useful when the
  // JavaScriptDialogTabHelper needs to make this dialog go away so that it can
  // respond to a call that requires it to make no callback or make a customized
  // one.
  virtual void CloseDialogWithoutCallback() = 0;

  // Returns the current value of the user input for a prompt dialog.
  virtual base::string16 GetUserInput() = 0;
};

#endif  // CHROME_BROWSER_UI_JAVASCRIPT_DIALOGS_JAVASCRIPT_DIALOG_H_
