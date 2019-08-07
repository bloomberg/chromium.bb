// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "url/gurl.h"

namespace send_tab_to_self {

// static
std::unique_ptr<SendTabToSelfInfoBarDelegate>
SendTabToSelfInfoBarDelegate::Create(const SendTabToSelfEntry* entry) {
  return base::WrapUnique(new SendTabToSelfInfoBarDelegate(entry));
}

SendTabToSelfInfoBarDelegate::~SendTabToSelfInfoBarDelegate() {}

base::string16 SendTabToSelfInfoBarDelegate::GetInfobarMessage() const {
  // TODO(crbug.com/944602): Define real string.
  NOTIMPLEMENTED();
  return base::UTF8ToUTF16("Open");
}

void SendTabToSelfInfoBarDelegate::OpenTab() {
  NOTIMPLEMENTED();
}

void SendTabToSelfInfoBarDelegate::InfoBarDismissed() {
  NOTIMPLEMENTED();
}

infobars::InfoBarDelegate::InfoBarIdentifier
SendTabToSelfInfoBarDelegate::GetIdentifier() const {
  return SEND_TAB_TO_SELF_INFOBAR_DELEGATE;
}

SendTabToSelfInfoBarDelegate::SendTabToSelfInfoBarDelegate(
    const SendTabToSelfEntry* entry) {
  entry_ = entry;
}

}  // namespace send_tab_to_self
