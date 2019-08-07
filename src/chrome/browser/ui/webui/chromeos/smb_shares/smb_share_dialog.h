// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_SHARE_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_SHARE_DIALOG_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {
namespace smb_dialog {

class SmbShareDialog : public SystemWebDialogDelegate {
 public:
  // Shows the dialog.
  static void Show();

 protected:
  SmbShareDialog();
  ~SmbShareDialog() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;

  DISALLOW_COPY_AND_ASSIGN(SmbShareDialog);
};

class SmbShareDialogUI : public ui::WebDialogUI {
 public:
  explicit SmbShareDialogUI(content::WebUI* web_ui);
  ~SmbShareDialogUI() override;

  DISALLOW_COPY_AND_ASSIGN(SmbShareDialogUI);
};

}  // namespace smb_dialog
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_SHARE_DIALOG_H_
