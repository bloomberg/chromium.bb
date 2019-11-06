// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/infobars/tab_sharing_infobar_delegate.h"

#include <utility>

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* TabSharingInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const base::string16& shared_tab_name,
    const base::string16& app_name) {
  DCHECK(infobar_service);
  return infobar_service->AddInfoBar(
      infobar_service->CreateConfirmInfoBar(base::WrapUnique(
          new TabSharingInfoBarDelegate(shared_tab_name, app_name))));
}

TabSharingInfoBarDelegate::TabSharingInfoBarDelegate(
    base::string16 shared_tab_name,
    base::string16 app_name)
    : shared_tab_name_(std::move(shared_tab_name)),
      app_name_(std::move(app_name)) {}

infobars::InfoBarDelegate::InfoBarIdentifier
TabSharingInfoBarDelegate::GetIdentifier() const {
  return TAB_SHARING_INFOBAR_DELEGATE;
}

base::string16 TabSharingInfoBarDelegate::GetMessageText() const {
  if (shared_tab_name_.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL, app_name_);
  }
  return l10n_util::GetStringFUTF16(
      IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_TAB_LABEL, shared_tab_name_,
      app_name_);
}

base::string16 TabSharingInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK)
                                       ? IDS_TAB_SHARING_INFOBAR_SHARE_BUTTON
                                       : IDS_TAB_SHARING_INFOBAR_STOP_BUTTON);
}

int TabSharingInfoBarDelegate::GetButtons() const {
  return shared_tab_name_.empty() ? BUTTON_CANCEL : BUTTON_OK | BUTTON_CANCEL;
}

bool TabSharingInfoBarDelegate::IsCloseable() const {
  return false;
}
