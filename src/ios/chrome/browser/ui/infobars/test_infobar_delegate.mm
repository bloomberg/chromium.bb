// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"

#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/infobar.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestInfoBarDelegate::TestInfoBarDelegate(NSString* infobar_message)
    : infobar_message_(infobar_message) {}

bool TestInfoBarDelegate::Create(infobars::InfoBarManager* infobar_manager) {
  DCHECK(infobar_manager);
  return !!infobar_manager->AddInfoBar(infobar_manager->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(this)));
}

TestInfoBarDelegate::InfoBarIdentifier TestInfoBarDelegate::GetIdentifier()
    const {
  return TEST_INFOBAR;
}

base::string16 TestInfoBarDelegate::GetMessageText() const {
  return base::SysNSStringToUTF16(infobar_message_);
}

int TestInfoBarDelegate::GetButtons() const {
  return ConfirmInfoBarDelegate::BUTTON_OK;
}
