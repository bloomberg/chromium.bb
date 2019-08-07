// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/fake_xr_session_request_consent_manager.h"

#include <utility>

#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "content/public/test/test_utils.h"

namespace vr {

namespace {
static constexpr base::TimeDelta kConsentDialogDisplayingTime =
    base::TimeDelta::FromMilliseconds(250);
}  // namespace

FakeXRSessionRequestConsentManager::FakeXRSessionRequestConsentManager(
    XRSessionRequestConsentManager* consent_manager,
    UserResponse user_response)
    : consent_manager_(consent_manager), user_response_(user_response) {}

FakeXRSessionRequestConsentManager::~FakeXRSessionRequestConsentManager() =
    default;

TabModalConfirmDialog*
FakeXRSessionRequestConsentManager::ShowDialogAndGetConsent(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> response_callback) {
  auto* confirm_dialog = consent_manager_->ShowDialogAndGetConsent(
      web_contents, std::move(response_callback));

  // Allow the dialog to show at least for a little while before simulating
  // the action.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitWhenIdleClosure(), kConsentDialogDisplayingTime);
  run_loop.Run();

  switch (user_response_) {
    case UserResponse::kClickAllowButton:
      confirm_dialog->AcceptTabModalDialog();
      break;
    case UserResponse::kClickCancelButton:
      confirm_dialog->CancelTabModalDialog();
      break;
    case UserResponse::kCloseDialog:
      confirm_dialog->CloseDialog();
      break;
  }
  return confirm_dialog;
}

}  // namespace vr
