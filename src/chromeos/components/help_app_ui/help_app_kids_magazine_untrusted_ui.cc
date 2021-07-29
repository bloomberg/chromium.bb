// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/help_app_ui/help_app_kids_magazine_untrusted_ui.h"

#include "base/strings/string_piece.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_help_app_kids_magazine_bundle_resources.h"
#include "chromeos/grit/chromeos_help_app_kids_magazine_bundle_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace chromeos {

namespace {

const char kKidsMagazinePathPrefix[] = "kids_magazine/";

// Function to remove a prefix from an input string. Does nothing if the string
// does not begin with the prefix.
base::StringPiece StripPrefix(base::StringPiece input,
                              base::StringPiece prefix) {
  if (input.find(prefix) == 0) {
    return input.substr(prefix.size());
  }
  return input;
}

content::WebUIDataSource* CreateHelpAppKidsMagazineUntrustedDataSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      kChromeUIHelpAppKidsMagazineUntrustedURL);
  // Set index.html as the default resource.
  source->SetDefaultResource(IDR_HELP_APP_KIDS_MAGAZINE_INDEX_HTML);
  source->DisableTrustedTypesCSP();

  for (size_t i = 0; i < kChromeosHelpAppKidsMagazineBundleResourcesSize; i++) {
    // While the JS and CSS file are stored in /kids_magazine/static/..., the
    // HTML file references /static/... directly. We need to strip the
    // "kids_magazine" prefix from the path.
    source->AddResourcePath(
        StripPrefix(kChromeosHelpAppKidsMagazineBundleResources[i].path,
                    kKidsMagazinePathPrefix),
        kChromeosHelpAppKidsMagazineBundleResources[i].id);
  }

  // Add chrome://help-app and chrome-untrusted://help-app as frame ancestors.
  source->AddFrameAncestor(GURL(kChromeUIHelpAppURL));
  source->AddFrameAncestor(GURL(kChromeUIHelpAppUntrustedURL));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' https://www.gstatic.com;");
  return source;
}

}  // namespace

HelpAppKidsMagazineUntrustedUIConfig::HelpAppKidsMagazineUntrustedUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  chromeos::kChromeUIHelpAppKidsMagazineHost) {}

HelpAppKidsMagazineUntrustedUIConfig::~HelpAppKidsMagazineUntrustedUIConfig() =
    default;

std::unique_ptr<content::WebUIController>
HelpAppKidsMagazineUntrustedUIConfig::CreateWebUIController(
    content::WebUI* web_ui) {
  return std::make_unique<HelpAppKidsMagazineUntrustedUI>(web_ui);
}

HelpAppKidsMagazineUntrustedUI::HelpAppKidsMagazineUntrustedUI(
    content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* untrusted_source =
      CreateHelpAppKidsMagazineUntrustedDataSource();

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, untrusted_source);
}

HelpAppKidsMagazineUntrustedUI::~HelpAppKidsMagazineUntrustedUI() = default;

}  // namespace chromeos
