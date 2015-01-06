// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/chromeos/message_box.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

class It2MeConfirmationDialogChromeOS : public It2MeConfirmationDialog {
 public:
  It2MeConfirmationDialogChromeOS();
  ~It2MeConfirmationDialogChromeOS() override;

  // It2MeConfirmationDialog implementation.
  void Show(const ResultCallback& callback) override;

 private:
  // Handles result from |message_box_|.
  void OnMessageBoxResult(MessageBox::Result result);

  scoped_ptr<MessageBox> message_box_;
  ResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(It2MeConfirmationDialogChromeOS);
};

It2MeConfirmationDialogChromeOS::It2MeConfirmationDialogChromeOS() {}

It2MeConfirmationDialogChromeOS::~It2MeConfirmationDialogChromeOS() {}

void It2MeConfirmationDialogChromeOS::Show(const ResultCallback& callback) {
  callback_ = callback;

  message_box_.reset(new MessageBox(
      l10n_util::GetStringUTF16(IDS_MODE_IT2ME),
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_MESSAGE),
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_CONFIRM),
      l10n_util::GetStringUTF16(IDS_SHARE_CONFIRM_DIALOG_DECLINE),
      base::Bind(&It2MeConfirmationDialogChromeOS::OnMessageBoxResult,
                 base::Unretained(this))));

  message_box_->Show();
}

void It2MeConfirmationDialogChromeOS::OnMessageBoxResult(
    MessageBox::Result result) {
  message_box_->Hide();
  base::ResetAndReturn(&callback_).Run(result == MessageBox::OK ?
                                       Result::OK : Result::CANCEL);
}

scoped_ptr<It2MeConfirmationDialog> It2MeConfirmationDialogFactory::Create() {
  return scoped_ptr<It2MeConfirmationDialog>(
      new It2MeConfirmationDialogChromeOS());
}

}  // namespace remoting
