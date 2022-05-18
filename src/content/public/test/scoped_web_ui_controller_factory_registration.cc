// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/scoped_web_ui_controller_factory_registration.h"

#include "base/strings/strcat.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/webui_config.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

ScopedWebUIControllerFactoryRegistration::
    ScopedWebUIControllerFactoryRegistration(
        content::WebUIControllerFactory* factory,
        content::WebUIControllerFactory* factory_to_replace)
    : factory_(factory), factory_to_replace_(factory_to_replace) {
  if (factory_to_replace_) {
    content::WebUIControllerFactory::UnregisterFactoryForTesting(
        factory_to_replace_);
  }
  content::WebUIControllerFactory::RegisterFactory(factory_);
}

ScopedWebUIControllerFactoryRegistration::
    ~ScopedWebUIControllerFactoryRegistration() {
  content::WebUIControllerFactory::UnregisterFactoryForTesting(factory_);
  // If we unregistered a registered factory, re-register it to keep global
  // state clean for future tests.
  if (factory_to_replace_)
    content::WebUIControllerFactory::RegisterFactory(factory_to_replace_);
}

ScopedWebUIConfigRegistration::ScopedWebUIConfigRegistration(
    std::unique_ptr<WebUIConfig> webui_config)
    : webui_config_origin_(url::Origin::Create(GURL(
          base::StrCat({webui_config->scheme(), url::kStandardSchemeSeparator,
                        webui_config->host()})))) {
  if (webui_config_origin_.scheme() == kChromeUIScheme) {
    WebUIConfigMap::GetInstance().AddWebUIConfig(std::move(webui_config));
    return;
  }
  if (webui_config_origin_.scheme() == kChromeUIUntrustedScheme) {
    WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
        std::move(webui_config));
    return;
  }
  NOTREACHED();
}

ScopedWebUIConfigRegistration::~ScopedWebUIConfigRegistration() {
  WebUIConfigMap::GetInstance().RemoveForTesting(webui_config_origin_);
}

void CheckForLeakedWebUIRegistrations::OnTestStart(
    const testing::TestInfo& test_info) {
  // Call GetInstance() to ensure WebUIConfig registers its
  // WebUIControllerFactory before we get the number of registered factories.
  initial_size_of_webui_config_map_ =
      WebUIConfigMap::GetInstance().GetSizeForTesting();
  initial_num_factories_registered_ =
      content::WebUIControllerFactory::GetNumRegisteredFactoriesForTesting();
}

void CheckForLeakedWebUIRegistrations::OnTestEnd(
    const testing::TestInfo& test_info) {
  EXPECT_EQ(initial_size_of_webui_config_map_,
            WebUIConfigMap::GetInstance().GetSizeForTesting())
      << "A WebUIConfig was registered by a test but never unregistered. This "
         "can cause flakiness in later tests. Please use "
         "ScopedWebUIConfigRegistration to ensure that registered configs are "
         "unregistered.";

  EXPECT_EQ(
      initial_num_factories_registered_,
      content::WebUIControllerFactory::GetNumRegisteredFactoriesForTesting())
      << "A WebUIControllerFactory was registered by a test but never "
         "unregistered. This can cause flakiness in later tests. Please use "
         "ScopedWebUIControllerFactoryRegistration to ensure that registered "
         "factories are unregistered.";
}

}  // namespace content
