// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_FINALIZER_H_

#include <memory>

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

struct WebApplicationInfo;

namespace web_app {

enum class InstallResultCode;

// An abstract finalizer for the installation process, represents the last step.
// Takes WebApplicationInfo as input, writes data to disk (e.g icons, shortcuts)
// and registers an app.
class InstallFinalizer {
 public:
  using InstallFinalizedCallback =
      base::OnceCallback<void(const AppId& app_id, InstallResultCode code)>;

  virtual void FinalizeInstall(std::unique_ptr<WebApplicationInfo> web_app_info,
                               InstallFinalizedCallback callback) = 0;

  virtual ~InstallFinalizer() = default;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_FINALIZER_H_
