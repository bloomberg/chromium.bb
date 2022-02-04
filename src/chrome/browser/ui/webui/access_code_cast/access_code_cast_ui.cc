// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_ui.h"

#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/access_code_cast_resources.h"
#include "chrome/grit/access_code_cast_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "ui/base/webui/web_ui_util.h"

using media_router::AccessCodeCastHandler;

///////////////////////////////////////////////////////////////////////////////
//  AccessCodeCast dialog:
///////////////////////////////////////////////////////////////////////////////

AccessCodeCastDialog::AccessCodeCastDialog(
    content::BrowserContext* context,
    const media_router::CastModeSet& cast_mode_set,
    content::WebContents* web_contents)
    : context_(context),
      cast_mode_set_(cast_mode_set),
      web_contents_(web_contents) {
  DCHECK(context_) << "Must have a context!";
  DCHECK(!cast_mode_set_.empty())
      << "Must have at least one available casting mode!";
  DCHECK(*cast_mode_set_.begin() ==
             media_router::MediaCastMode::DESKTOP_MIRROR ||
         web_contents_)
      << "Web contents must be set for non desktop-mode casting!";
  set_can_resize(false);
}

void AccessCodeCastDialog::Show(const media_router::CastModeSet& cast_mode_set,
                                content::WebContents* web_contents) {
  AccessCodeCastDialog::Show(web_contents ? web_contents->GetMainFrame()
                                                ->GetOutermostMainFrame()
                                                ->GetNativeView()
                                          : nullptr,
                             web_contents
                                 ? web_contents->GetBrowserContext()
                                 : ProfileManager::GetActiveUserProfile(),
                             cast_mode_set, web_contents);
}

void AccessCodeCastDialog::Show(gfx::NativeView parent,
                                content::BrowserContext* context,
                                const media_router::CastModeSet& cast_mode_set,
                                content::WebContents* web_contents) {
  chrome::ShowWebDialog(
      parent, context,
      new AccessCodeCastDialog(context, cast_mode_set, web_contents));
}

ui::ModalType AccessCodeCastDialog::GetDialogModalType() const {
  // If there are no web_contents_, that means that the dialog was launched
  // from the system tray, so therefore it shuold be a system dialog.
  return web_contents_ ? ui::MODAL_TYPE_WINDOW : ui::MODAL_TYPE_SYSTEM;
}

std::u16string AccessCodeCastDialog::GetDialogTitle() const {
  return std::u16string();
}

GURL AccessCodeCastDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIAccessCodeCastURL);
}

void AccessCodeCastDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void AccessCodeCastDialog::GetDialogSize(gfx::Size* size) const {
  // TODO(b/202529859): Replace these with final values
  const int kDefaultWidth = 512;
  const int kDefaultHeight = 480;
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string AccessCodeCastDialog::GetDialogArgs() const {
  base::DictionaryValue args;
  std::string json;
  base::JSONWriter::Write(args, &json);
  return json;
}

void AccessCodeCastDialog::OnDialogShown(content::WebUI* webui) {
  webui_ = webui;
  AccessCodeCastUI* controller =
      webui_->GetController()->GetAs<AccessCodeCastUI>();
  controller->SetCastModeSet(cast_mode_set_);
  controller->SetBrowserContext(context_);
  controller->SetWebContents(web_contents_);
}

void AccessCodeCastDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void AccessCodeCastDialog::OnCloseContents(content::WebContents* source,
                                           bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool AccessCodeCastDialog::ShouldShowDialogTitle() const {
  return false;
}

bool AccessCodeCastDialog::ShouldShowCloseButton() const {
  return false;
}

AccessCodeCastDialog::FrameKind
AccessCodeCastDialog::GetWebDialogFrameKind() const {
  return FrameKind::kDialog;
}

// Ensure the WebUI dialog has camera access
void AccessCodeCastDialog::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr /* extension */);
}

bool AccessCodeCastDialog::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  return true;
}

AccessCodeCastDialog::~AccessCodeCastDialog() = default;

///////////////////////////////////////////////////////////////////////////////
//  AccessCodeCast UI controller:
///////////////////////////////////////////////////////////////////////////////
AccessCodeCastUI::AccessCodeCastUI(content::WebUI* web_ui)
    : MojoWebDialogUI(web_ui) {
  auto source = base::WrapUnique(
      content::WebUIDataSource::Create(chrome::kChromeUIAccessCodeCastHost));
  webui::SetupWebUIDataSource(
      source.get(),
      base::make_span(kAccessCodeCastResources, kAccessCodeCastResourcesSize),
      IDR_ACCESS_CODE_CAST_INDEX_HTML);

  static constexpr webui::LocalizedString kStrings[] = {
      {"back", IDS_ACCESS_CODE_CAST_BACK},
      {"cast", IDS_ACCESS_CODE_CAST_CAST},
      {"close", IDS_CLOSE},
      {"dialogTitle", IDS_ACCESS_CODE_CAST_DIALOG_TITLE},
      {"errorAccessCode", IDS_ACCESS_CODE_CAST_ERROR_ACCESS_CODE},
      {"errorNetwork", IDS_ACCESS_CODE_CAST_ERROR_NETWORK},
      {"errorPermission", IDS_ACCESS_CODE_CAST_ERROR_PERMISSION},
      {"errorTooManyRequests", IDS_ACCESS_CODE_CAST_ERROR_TOO_MANY_REQUESTS},
      {"errorUnknown", IDS_ACCESS_CODE_CAST_ERROR_UNKNOWN},
      {"useCamera", IDS_ACCESS_CODE_CAST_USE_CAMERA},
  };

  source->AddLocalizedStrings(kStrings);
  source->AddBoolean("qrScannerEnabled", false);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source.release());
}

AccessCodeCastUI::~AccessCodeCastUI() = default;

void AccessCodeCastUI::SetCastModeSet(
    const media_router::CastModeSet& cast_mode_set) {
  cast_mode_set_ = cast_mode_set;
}

void AccessCodeCastUI::SetBrowserContext(content::BrowserContext* context) {
  context_ = context;
}

void AccessCodeCastUI::SetWebContents(content::WebContents* web_contents) {
  web_contents_ = web_contents;
}

void AccessCodeCastUI::BindInterface(
    mojo::PendingReceiver<access_code_cast::mojom::PageHandlerFactory>
        receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void AccessCodeCastUI::CreatePageHandler(
    mojo::PendingRemote<access_code_cast::mojom::Page> page,
    mojo::PendingReceiver<access_code_cast::mojom::PageHandler> receiver) {
  DCHECK(page);

  // We only get a MediaRouter if the browser context is present. This is to
  // prevent our js unit tests from failing.
  media_router::MediaRouter* router =
      context_
          ? media_router::MediaRouterFactory::GetApiForBrowserContext(context_)
          : nullptr;

  page_handler_ = std::make_unique<AccessCodeCastHandler>(
      std::move(receiver), std::move(page),
      context_ ? Profile::FromBrowserContext(context_)
               : Profile::FromWebUI(web_ui()),
      router, cast_mode_set_, web_contents_);
}

WEB_UI_CONTROLLER_TYPE_IMPL(AccessCodeCastUI)
