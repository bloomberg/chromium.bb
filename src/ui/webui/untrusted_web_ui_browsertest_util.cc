// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/untrusted_web_ui_browsertest_util.h"

#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

namespace ui {

TestUntrustedWebUIControllerFactory::TestUntrustedWebUIControllerFactory() =
    default;

TestUntrustedWebUIControllerFactory::~TestUntrustedWebUIControllerFactory() =
    default;

const ui::UntrustedWebUIControllerFactory::WebUIConfigMap&
TestUntrustedWebUIControllerFactory::GetWebUIConfigMap() {
  return configs_;
}

TestUntrustedWebUIConfig::TestUntrustedWebUIConfig(base::StringPiece host)
    : WebUIConfig(content::kChromeUIUntrustedScheme, host) {}

TestUntrustedWebUIConfig::TestUntrustedWebUIConfig(
    base::StringPiece host,
    const content::TestUntrustedDataSourceHeaders& headers)
    : WebUIConfig(content::kChromeUIUntrustedScheme, host), headers_(headers) {}

TestUntrustedWebUIConfig::~TestUntrustedWebUIConfig() = default;

std::unique_ptr<content::WebUIController>
TestUntrustedWebUIConfig::CreateWebUIController(content::WebUI* web_ui) {
  return std::make_unique<TestUntrustedWebUIController>(web_ui, host(),
                                                        headers_);
}

TestUntrustedWebUIController::TestUntrustedWebUIController(
    content::WebUI* web_ui,
    const std::string& host,
    const content::TestUntrustedDataSourceHeaders& headers)
    : ui::UntrustedWebUIController(web_ui) {
  content::AddUntrustedDataSource(web_ui->GetWebContents()->GetBrowserContext(),
                                  host, headers);
}

TestUntrustedWebUIController::~TestUntrustedWebUIController() = default;

}  // namespace ui
