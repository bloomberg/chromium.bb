// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_

#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/stub_credentials_filter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

// Use this class as a base for mock or test clients to avoid stubbing
// uninteresting pure virtual methods. All the implemented methods are just
// trivial stubs.  Do NOT use in production, only use in tests.
class StubPasswordManagerClient : public PasswordManagerClient {
 public:
  StubPasswordManagerClient();

  StubPasswordManagerClient(const StubPasswordManagerClient&) = delete;
  StubPasswordManagerClient& operator=(const StubPasswordManagerClient&) =
      delete;

  ~StubPasswordManagerClient() override;

  // PasswordManagerClient:
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManagerForUI> form_to_save,
      bool update_password) override;
  void PromptUserToMovePasswordToAccount(
      std::unique_ptr<PasswordFormManagerForUI> form_to_move) override;
  void ShowManualFallbackForSaving(
      std::unique_ptr<PasswordFormManagerForUI> form_to_save,
      bool has_generated_password,
      bool update_password) override;
  void HideManualFallbackForSaving() override;
  void FocusedInputChanged(
      password_manager::PasswordManagerDriver* driver,
      autofill::FieldRendererId focused_field_id,
      autofill::mojom::FocusedFieldType focused_field_type) override;
  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<PasswordForm>> local_forms,
      const url::Origin& origin,
      CredentialsCallback callback) override;
  void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<PasswordForm>> local_forms,
      const url::Origin& origin) override;
  void NotifyUserCouldBeAutoSignedIn(std::unique_ptr<PasswordForm>) override;
  void NotifySuccessfulLoginWithExistingPassword(
      std::unique_ptr<PasswordFormManagerForUI> submitted_manager) override;
  void NotifyStorePasswordCalled() override;
  void AutomaticPasswordSave(
      std::unique_ptr<PasswordFormManagerForUI> saved_manager) override;
  PrefService* GetPrefs() const override;
  PasswordStoreInterface* GetProfilePasswordStore() const override;
  PasswordStoreInterface* GetAccountPasswordStore() const override;
  PasswordReuseManager* GetPasswordReuseManager() const override;
  PasswordScriptsFetcher* GetPasswordScriptsFetcher() override;
  const GURL& GetLastCommittedURL() const override;
  url::Origin GetLastCommittedOrigin() const override;
  const CredentialsFilter* GetStoreResultFilter() const override;
  const autofill::LogManager* GetLogManager() const override;
  const MockPasswordFeatureManager* GetPasswordFeatureManager() const override;
  MockPasswordFeatureManager* GetPasswordFeatureManager();
  bool IsAutofillAssistantUIVisible() const override;
  version_info::Channel GetChannel() const override;

  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override;

#if defined(ON_FOCUS_PING_ENABLED)
  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override;
#endif

  void CheckProtectedPasswordEntry(
      metrics_util::PasswordType reused_password_type,
      const std::string& username,
      const std::vector<MatchingReusedCredential>& matching_reused_credentials,
      bool password_field_exists) override;

  void LogPasswordReuseDetectedEvent() override;

  ukm::SourceId GetUkmSourceId() override;
  PasswordManagerMetricsRecorder* GetMetricsRecorder() override;
  signin::IdentityManager* GetIdentityManager() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network::mojom::NetworkContext* GetNetworkContext() const override;
  bool IsIsolationForPasswordSitesEnabled() const override;
  bool IsNewTabPage() const override;
  FieldInfoManager* GetFieldInfoManager() const override;

 private:
  const StubCredentialsFilter credentials_filter_;
  testing::NiceMock<MockPasswordFeatureManager> password_feature_manager_;
  autofill::StubLogManager log_manager_;
  ukm::SourceId ukm_source_id_;
  absl::optional<PasswordManagerMetricsRecorder> metrics_recorder_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_
