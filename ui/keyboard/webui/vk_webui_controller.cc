// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/webui/vk_webui_controller.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/service_registry.h"
#include "grit/keyboard_resources.h"
#include "grit/keyboard_resources_map.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/public/cpp/system/core.h"
#include "ui/keyboard/keyboard_constants.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/keyboard/webui/vk_mojo_handler.h"

namespace keyboard {

namespace {

content::WebUIDataSource* CreateKeyboardUIDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kKeyboardHost);

  size_t count = 0;
  const GritResourceMap* resources = GetKeyboardExtensionResources(&count);
  source->SetDefaultResource(IDR_KEYBOARD_INDEX);

  const std::string keyboard_host = base::StringPrintf("%s/", kKeyboardHost);
  for (size_t i = 0; i < count; ++i) {
    size_t offset = 0;
    // The webui URL needs to skip the 'keyboard/' at the front of the resource
    // names, since it is part of the data-source name.
    if (StartsWithASCII(std::string(resources[i].name), keyboard_host, false))
      offset = keyboard_host.length();
    source->AddResourcePath(resources[i].name + offset, resources[i].value);
  }
  return source;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// VKWebUIController:

VKWebUIController::VKWebUIController(content::WebUI* web_ui)
    : WebUIController(web_ui), weak_factory_(this) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, CreateKeyboardUIDataSource());
  content::WebUIDataSource::AddMojoDataSource(browser_context)->AddResourcePath(
      "ui/keyboard/webui/keyboard.mojom", IDR_KEYBOARD_MOJO_GEN_JS);
}

VKWebUIController::~VKWebUIController() {
}

void VKWebUIController::RenderViewCreated(content::RenderViewHost* host) {
  host->GetMainFrame()->GetServiceRegistry()->AddService<KeyboardUIHandlerMojo>(
      base::Bind(&VKWebUIController::CreateAndStoreUIHandler,
                 weak_factory_.GetWeakPtr()));
}

void VKWebUIController::CreateAndStoreUIHandler(
    mojo::InterfaceRequest<KeyboardUIHandlerMojo> request) {
  ui_handler_ = scoped_ptr<VKMojoHandler>(
      mojo::WeakBindToRequest(new VKMojoHandler(), &request));
}

////////////////////////////////////////////////////////////////////////////////
// VKWebUIControllerFactory:

content::WebUI::TypeID VKWebUIControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) const {
  if (url == GURL(kKeyboardURL))
    return const_cast<VKWebUIControllerFactory*>(this);

  return content::WebUI::kNoWebUI;
}

bool VKWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) const {
  return GetWebUIType(browser_context, url) != content::WebUI::kNoWebUI;
}

bool VKWebUIControllerFactory::UseWebUIBindingsForURL(
    content::BrowserContext* browser_context,
    const GURL& url) const {
  return UseWebUIForURL(browser_context, url);
}

content::WebUIController* VKWebUIControllerFactory::CreateWebUIControllerForURL(
    content::WebUI* web_ui,
    const GURL& url) const {
  if (url == GURL(kKeyboardURL))
    return new VKWebUIController(web_ui);
  return NULL;
}

// static
VKWebUIControllerFactory* VKWebUIControllerFactory::GetInstance() {
  return Singleton<VKWebUIControllerFactory>::get();
}

// protected
VKWebUIControllerFactory::VKWebUIControllerFactory() {
}

VKWebUIControllerFactory::~VKWebUIControllerFactory() {
}

}  // namespace keyboard
