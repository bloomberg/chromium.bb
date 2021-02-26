// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/file_manager/file_manager_ui.h"

#include "base/memory/ptr_util.h"
#include "chromeos/components/file_manager/file_manager_page_handler.h"
#include "chromeos/components/file_manager/url_constants.h"
#include "chromeos/grit/chromeos_file_manager_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/file_manager/grit/file_manager_resources.h"

namespace chromeos {
namespace file_manager {

FileManagerUI::FileManagerUI(content::WebUI* web_ui,
                             std::unique_ptr<FileManagerUIDelegate> delegate)
    : MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  auto source = base::WrapUnique(content::WebUIDataSource::Create(
      chromeos::file_manager::kChromeUIFileManagerHost));
  source->AddLocalizedStrings(*this->delegate()->GetFileManagerAppStrings());
  // The HTML content loaded on chrome://file-manager.
  source->AddResourcePath("", IDR_FILE_MANAGER_SWA_MAIN_HTML);

  // The resources requested by chrome://file-manager HTML.
  // TOD(majewski): Rename main.* to main_swa.*
  source->AddResourcePath("main.css", IDR_FILE_MANAGER_SWA_MAIN_CSS);
  source->AddResourcePath("main.js", IDR_FILE_MANAGER_SWA_MAIN_JS);
  source->AddResourcePath("file_manager.mojom-lite.js",
                          IDR_FILE_MANAGER_SWA_MOJO_LITE_JS);
  source->AddResourcePath("browser_proxy.js",
                          IDR_FILE_MANAGER_SWA_BROWSER_PROXY_JS);

  // Sets up legacy_main_script.js to be the same as Files App
  //   ui/file_manager/file_manager/foreground/js/main_scripts.js
  source->AddResourcePath("legacy_main_scripts.js", IDR_FILE_MANAGER_MAIN_JS);

#if !DCHECK_IS_ON()
  // If a user goes to an invalid url and non-DCHECK mode (DHECK = debug mode)
  // is set, serve a default page so the user sees your default page instead
  // of an unexpected error. But if DCHECK is set, the user will be a
  // developer and be able to identify an error occurred.
  source->SetDefaultResource(IDR_FILE_MANAGER_SWA_MAIN_HTML);
#endif  // !DCHECK_IS_ON()

  // TODO(crbug.com/1098685): Trusted Type remaining WebUI.
  source->DisableTrustedTypesCSP();

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source.release());
}

FileManagerUI::~FileManagerUI() = default;

void FileManagerUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }
  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void FileManagerUI::CreatePageHandler(
    mojo::PendingRemote<mojom::Page> pending_page,
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler) {
  DCHECK(pending_page.is_valid());

  page_handler_ = std::make_unique<FileManagerPageHandler>(
      std::move(pending_page_handler), std::move(pending_page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(FileManagerUI)

}  // namespace file_manager
}  // namespace chromeos
