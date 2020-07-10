// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_CREDENTIALS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_CREDENTIALS_DIALOG_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {
namespace smb_dialog {

class SmbCredentialsDialog : public SystemWebDialogDelegate {
 public:
  // Shows the dialog.
  static void Show(int32_t mount_id, const std::string& share_path);

 protected:
  SmbCredentialsDialog(int32_t mount_id, const std::string& share_path);
  ~SmbCredentialsDialog() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;

 private:
  int32_t mount_id_;
  std::string share_path_;

  DISALLOW_COPY_AND_ASSIGN(SmbCredentialsDialog);
};

class SmbCredentialsDialogUI : public ui::WebDialogUI {
 public:
  explicit SmbCredentialsDialogUI(content::WebUI* web_ui);
  ~SmbCredentialsDialogUI() override;

  DISALLOW_COPY_AND_ASSIGN(SmbCredentialsDialogUI);
};

}  // namespace smb_dialog
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_CREDENTIALS_DIALOG_H_
