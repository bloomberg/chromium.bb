// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_WEB_UI_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_WEB_UI_CONTROLLER_FACTORY_H_

#include <memory>
#include <string>
#include <utility>

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/test/test_utils.h"

// WebUIControllerFactory that creates a TestWebUIController, which serves the
// resources needed for a minimal System Web App (a page with a web manifest and
// an icon).
class TestSystemWebAppWebUIControllerFactory
    : public content::WebUIControllerFactory {
 public:
  explicit TestSystemWebAppWebUIControllerFactory(std::string source_name);
  TestSystemWebAppWebUIControllerFactory(
      const TestSystemWebAppWebUIControllerFactory&) = delete;
  TestSystemWebAppWebUIControllerFactory& operator=(
      const TestSystemWebAppWebUIControllerFactory&) = delete;

  void set_manifest(std::string manifest) { manifest_ = std::move(manifest); }

  // content::WebUIControllerFactory
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override;

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override;

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override;
  bool UseWebUIBindingsForURL(content::BrowserContext* browser_context,
                              const GURL& url) override;

 private:
  std::string source_name_;
  std::string manifest_;
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_SYSTEM_WEB_APP_WEB_UI_CONTROLLER_FACTORY_H_
