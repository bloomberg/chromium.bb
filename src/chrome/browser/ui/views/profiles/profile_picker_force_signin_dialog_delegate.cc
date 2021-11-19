// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_force_signin_dialog_delegate.h"

#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/profiles/profile_picker_force_signin_dialog_host.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
ProfilePickerForceSigninDialogDelegate::ProfilePickerForceSigninDialogDelegate(
    ProfilePickerForceSigninDialogHost* host,
    std::unique_ptr<views::WebView> web_view,
    const GURL& url)
    : host_(host) {
  SetHasWindowSizeControls(true);
  SetTitle(IDS_PROFILES_GAIA_SIGNIN_TITLE);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetModalType(ui::MODAL_TYPE_WINDOW);
  RegisterDeleteDelegateCallback(
      base::BindOnce(&ProfilePickerForceSigninDialogDelegate::OnDialogDestroyed,
                     base::Unretained(this)));
  set_use_custom_frame(false);

  web_view_ = AddChildView(std::move(web_view));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  web_view_->GetWebContents()->SetDelegate(this);

  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_view_->GetWebContents(),
      autofill::ChromeAutofillClient::FromWebContents(
          web_view_->GetWebContents()));

  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_view_->GetWebContents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      web_view_->GetWebContents())
      ->SetDelegate(this);

  web_view_->LoadInitialURL(url);

  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::PROFILE_PICKER_FORCE_SIGNIN);
}

ProfilePickerForceSigninDialogDelegate::
    ~ProfilePickerForceSigninDialogDelegate() = default;

gfx::Size ProfilePickerForceSigninDialogDelegate::CalculatePreferredSize()
    const {
  return gfx::Size(ProfilePickerForceSigninDialog::kDialogWidth,
                   ProfilePickerForceSigninDialog::kDialogHeight);
}

void ProfilePickerForceSigninDialogDelegate::DisplayErrorMessage() {
  web_view_->LoadInitialURL(GURL(chrome::kChromeUISigninErrorURL));
}

bool ProfilePickerForceSigninDialogDelegate::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Prevents the context menu from being shown. While the signin page could do
  // this just with JS, there could be a brief moment before a context menu
  // handler could be registered when the user could still open a context menu,
  // so we block the attempt here. Note that the signin page is responsible for
  // preventing context menus in any <webview> contents it creates.
  return true;
}

web_modal::WebContentsModalDialogHost*
ProfilePickerForceSigninDialogDelegate::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView ProfilePickerForceSigninDialogDelegate::GetHostView() const {
  return GetWidget()->GetNativeView();
}

gfx::Point ProfilePickerForceSigninDialogDelegate::GetDialogPosition(
    const gfx::Size& size) {
  gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
  return gfx::Point(std::max(0, (widget_size.width() - size.width()) / 2),
                    std::max(0, (widget_size.height() - size.height()) / 2));
}

gfx::Size ProfilePickerForceSigninDialogDelegate::GetMaximumDialogSize() {
  return GetWidget()->GetWindowBoundsInScreen().size();
}

void ProfilePickerForceSigninDialogDelegate::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {}

void ProfilePickerForceSigninDialogDelegate::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {}

views::View* ProfilePickerForceSigninDialogDelegate::GetInitiallyFocusedView() {
  return static_cast<views::View*>(web_view_);
}

void ProfilePickerForceSigninDialogDelegate::CloseDialog() {
  OnDialogDestroyed();
  GetWidget()->Close();
}

void ProfilePickerForceSigninDialogDelegate::OnDialogDestroyed() {
  if (host_) {
    host_->OnDialogDestroyed();
    host_ = nullptr;
  }
}

BEGIN_METADATA(ProfilePickerForceSigninDialogDelegate,
               views::DialogDelegateView)
END_METADATA
