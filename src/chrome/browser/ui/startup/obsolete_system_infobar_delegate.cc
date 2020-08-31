// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/obsolete_system_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
void ObsoleteSystemInfoBarDelegate::Create(InfoBarService* infobar_service) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new ObsoleteSystemInfoBarDelegate())));
}

ObsoleteSystemInfoBarDelegate::ObsoleteSystemInfoBarDelegate()
    : ConfirmInfoBarDelegate() {
}

ObsoleteSystemInfoBarDelegate::~ObsoleteSystemInfoBarDelegate() {
}

infobars::InfoBarDelegate::InfoBarIdentifier
ObsoleteSystemInfoBarDelegate::GetIdentifier() const {
  return OBSOLETE_SYSTEM_INFOBAR_DELEGATE;
}

base::string16 ObsoleteSystemInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL ObsoleteSystemInfoBarDelegate::GetLinkURL() const {
  return GURL(ObsoleteSystem::GetLinkURL());
}

base::string16 ObsoleteSystemInfoBarDelegate::GetMessageText() const {
  return ObsoleteSystem::LocalizedObsoleteString();
}

int ObsoleteSystemInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}
