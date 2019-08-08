// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/test/test_install_finalizer.h"

#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/web_application_info.h"

namespace web_app {

TestInstallFinalizer::TestInstallFinalizer() {}

TestInstallFinalizer::~TestInstallFinalizer() = default;

void TestInstallFinalizer::FinalizeInstall(
    const WebApplicationInfo& web_app_info,
    const FinalizeOptions& options,
    InstallFinalizedCallback callback) {
  AppId app_id = GenerateAppIdFromURL(web_app_info.app_url);
  if (next_app_id_.has_value()) {
    app_id = next_app_id_.value();
    next_app_id_.reset();
  }

  InstallResultCode code = InstallResultCode::kSuccess;
  if (next_result_code_.has_value()) {
    code = next_result_code_.value();
    next_result_code_.reset();
  }

  // Store a copy for inspecting in tests.
  web_app_info_copy_ = std::make_unique<WebApplicationInfo>(web_app_info);
  finalized_policy_install_ = false;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), app_id, code));

  if (options.policy_installed)
    finalized_policy_install_ = true;
}

bool TestInstallFinalizer::CanCreateOsShortcuts() const {
  return true;
}

void TestInstallFinalizer::CreateOsShortcuts(
    const AppId& app_id,
    bool add_to_desktop,
    CreateOsShortcutsCallback callback) {
  ++num_create_os_shortcuts_calls_;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), true /* shortcuts_created */));
}

bool TestInstallFinalizer::CanPinAppToShelf() const {
  return true;
}

void TestInstallFinalizer::PinAppToShelf(const AppId& app_id) {
  ++num_pin_app_to_shelf_calls_;
}

bool TestInstallFinalizer::CanReparentTab(const AppId& app_id,
                                          bool shortcut_created) const {
  return true;
}

void TestInstallFinalizer::ReparentTab(const AppId& app_id,
                                       content::WebContents* web_contents) {
  ++num_reparent_tab_calls_;
}

bool TestInstallFinalizer::CanRevealAppShim() const {
  return true;
}

void TestInstallFinalizer::RevealAppShim(const AppId& app_id) {
  ++num_reveal_appshim_calls_;
}

void TestInstallFinalizer::SetNextFinalizeInstallResult(
    const AppId& app_id,
    InstallResultCode code) {
  next_app_id_ = app_id;
  next_result_code_ = code;
}

}  // namespace web_app
