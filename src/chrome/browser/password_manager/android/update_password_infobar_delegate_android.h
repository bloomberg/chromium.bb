// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_UPDATE_PASSWORD_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_UPDATE_PASSWORD_INFOBAR_DELEGATE_ANDROID_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/password_manager/android/password_manager_infobar_delegate_android.h"
#include "chrome/browser/ui/passwords/manage_passwords_state.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace content {
class WebContents;
}

// An infobar delegate which asks the user if the password should be updated for
// a set of saved credentials for a site.  If several such sets are present, the
// user can choose which one to update.  PasswordManager displays this infobar
// when the user signs into the site with a new password for a known username
// or fills in a password change form.
class UpdatePasswordInfoBarDelegate : public PasswordManagerInfoBarDelegate {
 public:
  static void Create(content::WebContents* web_contents,
                     std::unique_ptr<password_manager::PasswordFormManagerForUI>
                         form_to_update);

  ~UpdatePasswordInfoBarDelegate() override;

  base::string16 GetBranding() const;
  bool is_smartlock_branding_enabled() const {
    return is_smartlock_branding_enabled_;
  }

  // Returns whether the user has multiple saved credentials, of which the
  // infobar affects just one. In this case the infobar should clarify which
  // credential is being affected.
  bool ShowMultipleAccounts() const;

  const std::vector<std::unique_ptr<autofill::PasswordForm>>& GetCurrentForms()
      const;

  // Returns the username of the saved credentials to be updated by default.
  const base::string16& GetDefaultUsername() const;

  // Determines the usernames to be displayed in the update infobar and returns
  // the index of the one selected by default.
  unsigned int GetDisplayUsernames(std::vector<base::string16>* usernames);

  // Exposed for testing.
  static unsigned int GetDisplayUsernames(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>& current_forms,
      const base::string16& default_username,
      std::vector<base::string16>* usernames);

 protected:
  // Makes a ctor available in tests.
  UpdatePasswordInfoBarDelegate(
      content::WebContents* web_contents,
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          form_to_update,
      bool is_smartlock_branding_enabled);

 private:
  // Used to track the results we get from the info bar.
  password_manager::metrics_util::UIDismissalReason infobar_response_;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  void InfoBarDismissed() override;
  bool Cancel() override;

  ManagePasswordsState passwords_state_;
  base::string16 branding_;
  bool is_smartlock_branding_enabled_;

  DISALLOW_COPY_AND_ASSIGN(UpdatePasswordInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_UPDATE_PASSWORD_INFOBAR_DELEGATE_ANDROID_H_
