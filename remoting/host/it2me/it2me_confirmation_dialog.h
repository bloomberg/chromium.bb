// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_CONFIRMATION_DIALOG_H_
#define REMOTING_HOST_IT2ME_IT2ME_CONFIRMATION_DIALOG_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"

namespace remoting {

// Interface for a dialog to confirm an It2Me session with the user.
// All methods, with the exception of the constructor, are guaranteed to be
// called on the UI thread.
class It2MeConfirmationDialog {
 public:
  enum class Result {
    OK,
    CANCEL
  };

  typedef base::Callback<void(Result)> ResultCallback;

  virtual ~It2MeConfirmationDialog() {}

  // Shows the dialog. |callback| will be called with the user's selection.
  // |callback| will not be called if the dialog is destroyed.
  virtual void Show(const ResultCallback& callback) = 0;
};

// Used to create an platform specific instance of It2MeConfirmationDialog.
class It2MeConfirmationDialogFactory {
 public:
  It2MeConfirmationDialogFactory();
  virtual ~It2MeConfirmationDialogFactory();

  virtual scoped_ptr<It2MeConfirmationDialog> Create();

  DISALLOW_COPY_AND_ASSIGN(It2MeConfirmationDialogFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_CONFIRMATION_DIALOG_H_
