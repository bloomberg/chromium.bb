// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/usb_internals_resources.h"
#include "chrome/grit/usb_internals_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

UsbInternalsUI::UsbInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://usb-internals source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIUsbInternalsHost);

  static constexpr webui::ResourcePath kPaths[] = {
      {"usb_device.mojom-webui.js", IDR_USB_DEVICE_MOJOM_WEBUI_JS},
      {"usb_enumeration_options.mojom-webui.js",
       IDR_USB_ENUMERATION_OPTIONS_MOJOM_WEBUI_JS},
      {"usb_manager.mojom-webui.js", IDR_USB_DEVICE_MANAGER_MOJOM_WEBUI_JS},
      {"usb_manager_client.mojom-webui.js",
       IDR_USB_DEVICE_MANAGER_CLIENT_MOJOM_WEBUI_JS},
      {"usb_manager_test.mojom-webui.js",
       IDR_USB_DEVICE_MANAGER_TEST_MOJOM_WEBUI_JS},
  };
  source->AddResourcePaths(kPaths);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kUsbInternalsResources, kUsbInternalsResourcesSize),
      IDR_USB_INTERNALS_USB_INTERNALS_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::RequireTrustedTypesFor,
      "require-trusted-types-for 'script';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types usb-test-static;");

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

WEB_UI_CONTROLLER_TYPE_IMPL(UsbInternalsUI)

UsbInternalsUI::~UsbInternalsUI() {}

void UsbInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::UsbInternalsPageHandler> receiver) {
  page_handler_ =
      std::make_unique<UsbInternalsPageHandler>(std::move(receiver));
}
