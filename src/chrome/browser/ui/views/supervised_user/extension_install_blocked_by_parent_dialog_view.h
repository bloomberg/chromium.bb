// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SUPERVISED_USER_EXTENSION_INSTALL_BLOCKED_BY_PARENT_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SUPERVISED_USER_EXTENSION_INSTALL_BLOCKED_BY_PARENT_DIALOG_VIEW_H_

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

namespace extensions {
class Extension;
}  // namespace extensions

// Modal dialog that shows when a child user attempts to install an extension
// but blocked by their parent.
class ExtensionInstallBlockedByParentDialogView
    : public views::DialogDelegateView {
 public:
  // Constructor for dialog shown when a parent blocks extension/app
  // installation for a child.  Do not call this directly. Instead,
  // use ShowExtensionInstallBlockedByParentDialog in browser_dialogs.h.
  // |action| is used to determine the strings to display in the dialog.
  // |extension| is used to customize the dialog for the extension type.
  // |window| is the window the dialog will modally attach to.
  // |done_callback| will be called when the dialog is dismissed by the user.
  ExtensionInstallBlockedByParentDialogView(
      chrome::ExtensionInstalledBlockedByParentDialogAction action,
      const extensions::Extension* extension,
      gfx::NativeWindow window,
      base::OnceClosure done_callback);
  ExtensionInstallBlockedByParentDialogView(
      const ExtensionInstallBlockedByParentDialogView&) = delete;
  ExtensionInstallBlockedByParentDialogView operator=(
      const ExtensionInstallBlockedByParentDialogView&) = delete;
  ~ExtensionInstallBlockedByParentDialogView() override;

 private:
  // views::DialogDelegateView
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;

  void ConfigureTitle();
  void CreateContents();
  base::string16 GetExtensionTypeString();

  const extensions::Extension* extension_ = nullptr;
  chrome::ExtensionInstalledBlockedByParentDialogAction action_;
  gfx::ImageSkia icon_;
  base::OnceClosure done_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SUPERVISED_USER_EXTENSION_INSTALL_BLOCKED_BY_PARENT_DIALOG_VIEW_H_
