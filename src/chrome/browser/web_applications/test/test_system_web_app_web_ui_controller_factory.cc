// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_system_web_app_web_ui_controller_factory.h"

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/web_applications/test/test_system_web_app_url_data_source.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

// WebUIController that serves a System PWA.
class TestSystemWebAppWebUIController : public content::WebUIController {
 public:
  explicit TestSystemWebAppWebUIController(std::string source_name,
                                           const std::string* manifest,
                                           content::WebUI* web_ui)
      : WebUIController(web_ui) {
    web_app::AddTestURLDataSource(
        source_name, manifest, web_ui->GetWebContents()->GetBrowserContext());
  }
  TestSystemWebAppWebUIController(const TestSystemWebAppWebUIController&) =
      delete;
  TestSystemWebAppWebUIController& operator=(
      const TestSystemWebAppWebUIController&) = delete;
};

}  // namespace

TestSystemWebAppWebUIControllerFactory::TestSystemWebAppWebUIControllerFactory(
    std::string source_name)
    : source_name_(std::move(source_name)),
      manifest_(web_app::kSystemAppManifestText) {}

std::unique_ptr<content::WebUIController>
TestSystemWebAppWebUIControllerFactory::CreateWebUIControllerForURL(
    content::WebUI* web_ui,
    const GURL& url) {
  return std::make_unique<TestSystemWebAppWebUIController>(source_name_,
                                                           &manifest_, web_ui);
}

content::WebUI::TypeID TestSystemWebAppWebUIControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) {
  if (url.SchemeIs(content::kChromeUIScheme))
    return reinterpret_cast<content::WebUI::TypeID>(1);

  return content::WebUI::kNoWebUI;
}

bool TestSystemWebAppWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme);
}

bool TestSystemWebAppWebUIControllerFactory::UseWebUIBindingsForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme);
}
