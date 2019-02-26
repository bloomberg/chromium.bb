// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_INSTALL_FINALIZER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"

struct WebApplicationInfo;

namespace web_app {

class TestInstallFinalizer final : public InstallFinalizer {
 public:
  TestInstallFinalizer();
  ~TestInstallFinalizer() override;

  // InstallFinalizer:
  void FinalizeInstall(std::unique_ptr<WebApplicationInfo> web_app_info,
                       InstallFinalizedCallback callback) override;

  std::unique_ptr<WebApplicationInfo> web_app_info() {
    return std::move(web_app_info_);
  }

 private:
  std::unique_ptr<WebApplicationInfo> web_app_info_;

  DISALLOW_COPY_AND_ASSIGN(TestInstallFinalizer);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_INSTALL_FINALIZER_H_
