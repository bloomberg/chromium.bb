// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_responder.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"

InfoBarResponder::InfoBarResponder(InfoBarService* infobar_service,
                                   AutoResponseType response)
    : infobar_service_(infobar_service), response_(response) {
  infobar_service_->AddObserver(this);
}

InfoBarResponder::~InfoBarResponder() {
  // This is safe even if we were already removed as an observer in
  // OnInfoBarAdded().
  infobar_service_->RemoveObserver(this);
}

void InfoBarResponder::OnInfoBarAdded(infobars::InfoBar* infobar) {
  infobar_service_->RemoveObserver(this);
  ConfirmInfoBarDelegate* delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  DCHECK(delegate);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&InfoBarResponder::Respond,
                                base::Unretained(this), delegate));
}

void InfoBarResponder::OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                                         infobars::InfoBar* new_infobar) {
  OnInfoBarAdded(new_infobar);
}

void InfoBarResponder::Respond(ConfirmInfoBarDelegate* delegate) {
  switch (response_) {
    case ACCEPT:
      delegate->Accept();
      break;
    case DENY:
      delegate->Cancel();
      break;
    case DISMISS:
      delegate->InfoBarDismissed();
      break;
  }
}
