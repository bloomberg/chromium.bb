// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "components/autofill/content/common/autofill_driver.mojom.h"
#include "components/password_manager/content/browser/content_credential_manager.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/http_auth_manager_impl.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_client_helper.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detection_manager.h"
#include "components/password_manager/core/browser/sync_credentials_filter.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/geometry/rect.h"

class PasswordGenerationPopupObserver;
class PasswordGenerationPopupControllerImpl;
class Profile;

namespace autofill {
namespace password_generation {
struct PasswordGenerationUIData;
}  // namespace password_generation
}  // namespace autofill

namespace content {
class WebContents;
}

// ChromePasswordManagerClient implements the PasswordManagerClient interface.
class ChromePasswordManagerClient
    : public password_manager::PasswordManagerClient,
      public password_manager::PasswordManagerClientHelperDelegate,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ChromePasswordManagerClient>,
      public autofill::mojom::PasswordGenerationDriver,
      public content::RenderWidgetHost::InputEventObserver {
 public:
  ~ChromePasswordManagerClient() override;

  // PasswordManagerClient implementation.
  bool IsSavingAndFillingEnabled(const GURL& url) const override;
  bool IsFillingEnabled(const GURL& url) const override;
  bool IsFillingFallbackEnabled(const GURL& url) const override;
  void PostHSTSQueryForHost(
      const GURL& origin,
      password_manager::HSTSCallback callback) const override;
  bool OnCredentialManagerUsed() override;
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool is_update) override;
  void ShowManualFallbackForSaving(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool has_generated_password,
      bool is_update) override;
  void HideManualFallbackForSaving() override;
  void FocusedInputChanged(
      password_manager::PasswordManagerDriver* driver,
      autofill::mojom::FocusedFieldType focused_field_type) override;
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin,
      const CredentialsCallback& callback) override;
  void GeneratePassword() override;
  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin) override;
  void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<autofill::PasswordForm> form) override;
  void NotifySuccessfulLoginWithExistingPassword(
      const autofill::PasswordForm& form) override;
  void NotifyStorePasswordCalled() override;
  void AutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          saved_form_manager) override;
  void PasswordWasAutofilled(
      const std::map<base::string16, const autofill::PasswordForm*>&
          best_matches,
      const GURL& origin,
      const std::vector<const autofill::PasswordForm*>* federated_matches)
      const override;
  void AutofillHttpAuth(const autofill::PasswordForm& preferred_match,
                        const password_manager::PasswordFormManagerForUI*
                            form_manager) const override;
  bool IsIsolationForPasswordSitesEnabled() const override;

  PrefService* GetPrefs() const override;
  password_manager::PasswordStore* GetPasswordStore() const override;
  password_manager::SyncState GetPasswordSyncState() const override;
  bool WasLastNavigationHTTPError() const override;
  net::CertStatus GetMainFrameCertStatus() const override;
  bool IsIncognito() const override;
  const password_manager::PasswordManager* GetPasswordManager() const override;
  password_manager::HttpAuthManager* GetHttpAuthManager() override;
  autofill::AutofillDownloadManager* GetAutofillDownloadManager() override;
  const GURL& GetMainFrameURL() const override;
  bool IsMainFrameSecure() const override;
  const GURL& GetLastCommittedEntryURL() const override;
  void AnnotateNavigationEntry(bool has_password_field) override;
  std::string GetPageLanguage() const override;
  const password_manager::CredentialsFilter* GetStoreResultFilter()
      const override;
  const password_manager::LogManager* GetLogManager() const override;
  password_manager::PasswordRequirementsService*
  GetPasswordRequirementsService() override;
  favicon::FaviconService* GetFaviconService() override;
  bool IsUnderAdvancedProtection() const override;
  void UpdateFormManagers() override;
  void NavigateToManagePasswordsPage(
      password_manager::ManagePasswordsReferrer referrer) override;
  bool IsNewTabPage() const override;

  // autofill::mojom::PasswordGenerationDriver overrides.
  void AutomaticGenerationAvailable(
      const autofill::password_generation::PasswordGenerationUIData& ui_data)
      override;
  void ShowPasswordEditingPopup(const gfx::RectF& bounds,
                                const autofill::PasswordForm& form) override;
  void GenerationAvailableForForm(const autofill::PasswordForm& form) override;
  void PasswordGenerationRejectedByTyping() override;
  void PresaveGeneratedPassword(
      const autofill::PasswordForm& password_form) override;
  void PasswordNoLongerGenerated(
      const autofill::PasswordForm& password_form) override;
  void FrameWasScrolled() override;
  void GenerationElementLostFocus() override;

#if defined(FULL_SAFE_BROWSING)
  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override;
  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override;

  void CheckProtectedPasswordEntry(
      password_manager::metrics_util::PasswordType reused_password_type,
      const std::string& username,
      const std::vector<std::string>& matching_domains,
      bool password_field_exists) override;

  void LogPasswordReuseDetectedEvent() override;
#endif

  ukm::SourceId GetUkmSourceId() override;
  password_manager::PasswordManagerMetricsRecorder* GetMetricsRecorder()
      override;

  static void CreateForWebContentsWithAutofillClient(
      content::WebContents* contents,
      autofill::AutofillClient* autofill_client);

  // Observer for PasswordGenerationPopup events. Used for testing.
  void SetTestObserver(PasswordGenerationPopupObserver* observer);

  static void BindCredentialManager(
      blink::mojom::CredentialManagerRequest request,
      content::RenderFrameHost* render_frame_host);

  // A helper method to determine whether a save/update bubble can be shown
  // on this |url|.
  static bool CanShowBubbleOnURL(const GURL& url);

#if defined(UNIT_TEST)
  bool was_store_ever_called() const { return was_store_ever_called_; }
  bool has_binding_for_credential_manager() const {
    return content_credential_manager_.HasBinding();
  }
#endif

 protected:
  // Callable for tests.
  ChromePasswordManagerClient(content::WebContents* web_contents,
                              autofill::AutofillClient* autofill_client);

  // content::WebContentsObserver override
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  void OnPaste() override;
  base::string16 GetTextFromClipboard();
#endif

 private:
  friend class content::WebContentsUserData<ChromePasswordManagerClient>;

  // content::WebContentsObserver overrides.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

// TODO(crbug.com/706392): Fix password reuse detection for Android.
#if !defined(OS_ANDROID)
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  // content::RenderWidgetHost::InputEventObserver overrides.
  void OnInputEvent(const blink::WebInputEvent&) override;
#endif

  // Given |bounds| in the renderers coordinate system, return the same bounds
  // in the screens coordinate system.
  gfx::RectF GetBoundsInScreenSpace(const gfx::RectF& bounds);

  // Checks if the current page specified in |url| fulfils the conditions for
  // the password manager to be active on it.
  bool IsPasswordManagementEnabledForCurrentPage(const GURL& url) const;

  // Returns true if this profile has metrics reporting and active sync
  // without custom sync passphrase.
  static bool ShouldAnnotateNavigationEntries(Profile* profile);

  // password_manager::PasswordManagerClientHelperDelegate implementation.
  void PromptUserToEnableAutosignin() override;
  password_manager::PasswordManager* GetPasswordManager() override;

  // |ui_data| is empty in case the renderer failed to start manual generation.
  // In this case nothing should happen.
  void ShowManualPasswordGenerationPopup(
      base::WeakPtr<password_manager::ContentPasswordManagerDriver> driver,
      const base::Optional<
          autofill::password_generation::PasswordGenerationUIData>& ui_data);

  void ShowPasswordGenerationPopup(
      password_manager::ContentPasswordManagerDriver* driver,
      const autofill::password_generation::PasswordGenerationUIData& ui_data,
      bool is_manually_triggered);

  gfx::RectF TransformToRootCoordinates(
      content::RenderFrameHost* frame_host,
      const gfx::RectF& bounds_in_frame_coordinates);

  Profile* const profile_;

  password_manager::PasswordManager password_manager_;
  password_manager::HttpAuthManagerImpl httpauth_manager_;

// TODO(crbug.com/706392): Fix password reuse detection for Android.
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  password_manager::PasswordReuseDetectionManager
      password_reuse_detection_manager_;
#endif

  password_manager::ContentPasswordManagerDriverFactory* driver_factory_;

  // As a mojo service, will be registered into service registry
  // of the main frame host by ChromeContentBrowserClient
  // once main frame host was created.
  password_manager::ContentCredentialManager content_credential_manager_;

  content::WebContentsFrameBindingSet<autofill::mojom::PasswordGenerationDriver>
      password_generation_driver_bindings_;

  // Observer for password generation popup.
  PasswordGenerationPopupObserver* observer_;

  // Controls the popup
  base::WeakPtr<PasswordGenerationPopupControllerImpl> popup_controller_;

  // Set to false to disable password saving (will no longer ask if you
  // want to save passwords and also won't fill the passwords).
  BooleanPrefMember saving_and_filling_passwords_enabled_;

  const password_manager::SyncCredentialsFilter credentials_filter_;

  std::unique_ptr<password_manager::LogManager> log_manager_;

  // Recorder of metrics that is associated with the last committed navigation
  // of the WebContents owning this ChromePasswordManagerClient. May be unset at
  // times. Sends statistics on destruction.
  base::Optional<password_manager::PasswordManagerMetricsRecorder>
      metrics_recorder_;

  // Whether navigator.credentials.store() was ever called from this
  // WebContents. Used for testing.
  bool was_store_ever_called_ = false;

  // Helper for performing logic that is common between
  // ChromePasswordManagerClient and IOSChromePasswordManagerClient.
  password_manager::PasswordManagerClientHelper helper_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ChromePasswordManagerClient);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_
