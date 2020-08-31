// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/global_confirm_info_bar.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"

namespace extensions {

namespace {

using Delegates = std::map<std::string, ExtensionDevToolsInfoBarDelegate*>;
base::LazyInstance<Delegates>::Leaky g_delegates = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
std::unique_ptr<ExtensionDevToolsInfoBarDelegate::CallbackList::Subscription>
ExtensionDevToolsInfoBarDelegate::Create(const std::string& extension_id,
                                         const std::string& extension_name,
                                         base::OnceClosure destroyed_callback) {
  Delegates& delegates = g_delegates.Get();
  const auto it = delegates.find(extension_id);
  if (it != delegates.end())
    return it->second->RegisterDestroyedCallback(std::move(destroyed_callback));

  // Can't use std::make_unique<>(), constructor is private.
  auto delegate = base::WrapUnique(
      new ExtensionDevToolsInfoBarDelegate(extension_id, extension_name));
  delegates[extension_id] = delegate.get();
  std::unique_ptr<CallbackList::Subscription> subscription =
      delegate->RegisterDestroyedCallback(std::move(destroyed_callback));
  GlobalConfirmInfoBar::Show(std::move(delegate));
  return subscription;
}

ExtensionDevToolsInfoBarDelegate::~ExtensionDevToolsInfoBarDelegate() {
  callback_list_.Notify();
  const size_t erased = g_delegates.Get().erase(extension_id_);
  DCHECK(erased);
}

infobars::InfoBarDelegate::InfoBarIdentifier
ExtensionDevToolsInfoBarDelegate::GetIdentifier() const {
  return EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE;
}

bool ExtensionDevToolsInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

base::string16 ExtensionDevToolsInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_DEV_TOOLS_INFOBAR_LABEL,
                                    extension_name_);
}

gfx::ElideBehavior ExtensionDevToolsInfoBarDelegate::GetMessageElideBehavior()
    const {
  // The important part of the message text above is at the end:
  // "... is debugging the browser". If the extension name is very long,
  // we'd rather truncate it instead. See https://crbug.com/823194.
  return gfx::ELIDE_HEAD;
}

int ExtensionDevToolsInfoBarDelegate::GetButtons() const {
  return BUTTON_CANCEL;
}

ExtensionDevToolsInfoBarDelegate::ExtensionDevToolsInfoBarDelegate(
    std::string extension_id,
    const std::string& extension_name)
    : extension_id_(std::move(extension_id)),
      extension_name_(base::UTF8ToUTF16(extension_name)) {}

std::unique_ptr<ExtensionDevToolsInfoBarDelegate::CallbackList::Subscription>
ExtensionDevToolsInfoBarDelegate::RegisterDestroyedCallback(
    base::OnceClosure destroyed_callback) {
  return callback_list_.Add(std::move(destroyed_callback));
}

}  // namespace extensions
