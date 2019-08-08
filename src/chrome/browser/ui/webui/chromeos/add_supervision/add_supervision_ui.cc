// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_ui.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"

namespace chromeos {

const char kAddSupervisionURL[] =
    "https://families.google.com/supervision/chrome";
const char kAddSupervisionEventOriginFilter[] = "https://families.google.com";

AddSupervisionUI::AddSupervisionUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Register the Mojo API handler.
  AddHandlerToRegistry(base::BindRepeating(
      &AddSupervisionUI::BindAddSupervisionHandler, base::Unretained(this)));

  // Set up the basic page framework.
  SetupResources();
}

void AddSupervisionUI::SetupResources() {
  Profile* profile = Profile::FromWebUI(web_ui());
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIAddSupervisionHost));

  source->AddResourcePath("post_message_api.js",
                          IDR_ADD_SUPERVISION_POST_MESSAGE_API_JS);
  source->AddResourcePath("add_supervision_api_server.js",
                          IDR_ADD_SUPERVISION_API_SERVER_JS);
  source->AddResourcePath("add_supervision.js", IDR_ADD_SUPERVISION_JS);
  source->AddLocalizedString("pageTitle", IDS_ADD_SUPERVISION_PAGE_TITLE);

  // Full paths (relative to src) are important for Mojom generated files.
  source->AddResourcePath(
      "chrome/browser/ui/webui/chromeos/add_supervision/"
      "add_supervision.mojom-lite.js",
      IDR_ADD_SUPERVISION_MOJOM_LITE_JS);

  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_ADD_SUPERVISION_HTML);
  source->AddString("webviewUrl", kAddSupervisionURL);
  source->AddString("eventOriginFilter", kAddSupervisionEventOriginFilter);

  content::WebUIDataSource::Add(profile, source.release());
}

AddSupervisionUI::~AddSupervisionUI() = default;

void AddSupervisionUI::BindAddSupervisionHandler(
    add_supervision::mojom::AddSupervisionHandlerRequest request) {
  mojo_api_handler_.reset(
      new AddSupervisionHandler(std::move(request), web_ui()));
}

}  // namespace chromeos
