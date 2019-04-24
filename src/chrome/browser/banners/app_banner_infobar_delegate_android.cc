// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_infobar_delegate_android.h"

#include <utility>

#include "chrome/browser/banners/app_banner_ui_delegate_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/app_banner_infobar_android.h"
#include "content/public/browser/web_contents.h"

namespace banners {

// static
bool AppBannerInfoBarDelegateAndroid::Create(
    content::WebContents* web_contents,
    std::unique_ptr<AppBannerUiDelegateAndroid> ui_delegate) {
  return InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(std::make_unique<AppBannerInfoBarAndroid>(
          std::unique_ptr<AppBannerInfoBarDelegateAndroid>(
              new AppBannerInfoBarDelegateAndroid(std::move(ui_delegate)))));
}

AppBannerInfoBarDelegateAndroid::~AppBannerInfoBarDelegateAndroid() {
  ui_delegate_.reset();
}

const AppBannerUiDelegateAndroid*
AppBannerInfoBarDelegateAndroid::GetUiDelegate() const {
  return ui_delegate_.get();
}

bool AppBannerInfoBarDelegateAndroid::Accept() {
  return ui_delegate_->InstallApp(
      InfoBarService::WebContentsFromInfoBar(infobar()));
}

base::string16 AppBannerInfoBarDelegateAndroid::GetMessageText() const {
  // The message text for the infobar is fetched directly from |ui_delegate_| by
  // the infobar.
  return base::string16();
}

AppBannerInfoBarDelegateAndroid::AppBannerInfoBarDelegateAndroid(
    std::unique_ptr<AppBannerUiDelegateAndroid> ui_delegate)
    : ui_delegate_(std::move(ui_delegate)) {
}

infobars::InfoBarDelegate::InfoBarIdentifier
AppBannerInfoBarDelegateAndroid::GetIdentifier() const {
  return APP_BANNER_INFOBAR_DELEGATE;
}

void AppBannerInfoBarDelegateAndroid::InfoBarDismissed() {
  ui_delegate_->OnUiCancelled();
}

int AppBannerInfoBarDelegateAndroid::GetButtons() const {
  return BUTTON_OK;
}

bool AppBannerInfoBarDelegateAndroid::LinkClicked(
    WindowOpenDisposition disposition) {
  ui_delegate_->ShowNativeAppDetails();

  return true;
}

}  // namespace banners
