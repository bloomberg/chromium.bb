// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/repost_form_warning_controller.h"

#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

RepostFormWarningController::RepostFormWarningController(
    content::WebContents* web_contents)
    : TabModalConfirmDialogDelegate(web_contents),
      content::WebContentsObserver(web_contents) {
}

RepostFormWarningController::~RepostFormWarningController() {
}

base::string16 RepostFormWarningController::GetTitle() {
  return l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING_TITLE);
}

base::string16 RepostFormWarningController::GetDialogMessage() {
  return l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING);
}

base::string16 RepostFormWarningController::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING_RESEND);
}

void RepostFormWarningController::OnAccepted() {
  web_contents()->GetController().ContinuePendingReload();
}

void RepostFormWarningController::OnCanceled() {
  web_contents()->GetController().CancelPendingReload();
}

void RepostFormWarningController::OnClosed() {
  web_contents()->GetController().CancelPendingReload();
}

void RepostFormWarningController::BeforeFormRepostWarningShow() {
  // Close the dialog if we show an additional dialog, to avoid them
  // stacking up.
  Cancel();
}
