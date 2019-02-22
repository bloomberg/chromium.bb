// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_RESULT_CODE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_RESULT_CODE_H_

namespace web_app {

// The result of an attempted web app installation, uninstallation or update.
//
// This is an enum, instead of a struct with multiple fields (e.g. one field for
// success or failure, one field for whether action was taken), because we want
// to track metrics for the overall cross product of the these axes.
//
// TODO(nigeltao): fold BookmarkAppInstallationTask::ResultCode into this class.
enum class InstallResultCode {
  kSuccess,
  kAlreadyInstalled,
  // Catch-all failure category. More-specific failure categories are below.
  kFailedUnknownReason,
  kPreviouslyUninstalled,
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_RESULT_CODE_H_
