// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_share_dialog.h"

#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/smb_client/smb_service.h"
#include "chrome/browser/chromeos/smb_client/smb_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_handler.h"
#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_shares_localized_strings_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace smb_dialog {
namespace {

constexpr int kSmbShareDialogHeight = 564;

void AddSmbSharesStrings(content::WebUIDataSource* html_source) {
  // Add strings specific to smb_dialog.
  smb_dialog::AddLocalizedStrings(html_source);

  // Add additional strings that are not specific to smb_dialog.
  static const struct {
    const char* name;
    int id;
  } localized_strings[] = {
      {"addSmbShare", IDS_SETTINGS_DOWNLOADS_SMB_SHARES_ADD_SHARE},
      {"add", IDS_ADD},
      {"cancel", IDS_CANCEL},
  };
  for (const auto& entry : localized_strings) {
    html_source->AddLocalizedString(entry.name, entry.id);
  }
}

}  // namespace

// static
void SmbShareDialog::Show() {
  SmbShareDialog* dialog = new SmbShareDialog();
  dialog->ShowSystemDialog();
}

SmbShareDialog::SmbShareDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUISmbShareURL),
                              base::string16() /* title */) {}

SmbShareDialog::~SmbShareDialog() = default;

void SmbShareDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(SystemWebDialogDelegate::kDialogWidth, kSmbShareDialogHeight);
}

SmbShareDialogUI::SmbShareDialogUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISmbShareHost);

  AddSmbSharesStrings(source);

  Profile* const profile = Profile::FromWebUI(web_ui);
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);

  source->AddBoolean("isActiveDirectoryUser",
                     user && user->IsActiveDirectoryUser());

  // Check if Kerberos was and still is enabled via policy at the creation time
  // of SmbService. As a result, if Kerberos policy was enabled during a user
  // session, SSO option will not be available/visible.
  // TODO(crbug.com/1040138): subscribe to pref. changes in SmbService and setup
  // Kerberos after it is enabled during a user session.
  const chromeos::smb_client::SmbService* const smb_service =
      chromeos::smb_client::SmbServiceFactory::Get(profile);
  bool is_kerberos_enabled =
      smb_service && smb_service->IsKerberosEnabledViaPolicy();
  source->AddBoolean("isKerberosEnabled", is_kerberos_enabled);

  source->UseStringsJs();
  source->SetDefaultResource(IDR_SMB_SHARES_DIALOG_CONTAINER_HTML);
  source->AddResourcePath("smb_share_dialog.js", IDR_SMB_SHARES_DIALOG_JS);

  web_ui->AddMessageHandler(std::make_unique<SmbHandler>(
      Profile::FromWebUI(web_ui), base::DoNothing()));

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

SmbShareDialogUI::~SmbShareDialogUI() = default;

}  // namespace smb_dialog
}  // namespace chromeos
