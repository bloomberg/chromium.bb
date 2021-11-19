// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_features.h"

#include "build/build_config.h"

namespace password_manager {

// NOTE: It is strongly recommended to use UpperCamelCase style for feature
//       names, e.g. "MyGreatFeature".
namespace features {

// Enables Biometrics for the Touch To Fill feature. This only effects Android.
const base::Feature kBiometricTouchToFill = {"BiometricTouchToFill",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables submission detection for forms dynamically cleared but not removed
// from the page.
const base::Feature kDetectFormSubmissionOnFormClear = {
    "DetectFormSubmissionOnFormClear",
#if defined(OS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT
#else
    base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enables the editing of passwords in Chrome settings.
const base::Feature kEditPasswordsInSettings = {
    "EditPasswordsInSettings", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables UI that allows the user to create a strong password even if the field
// wasn't parsed as a new password field.
// TODO(crbug/1181254): Remove once it's launched.
const base::Feature kEnableManualPasswordGeneration = {
    "EnableManualPasswordGeneration", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables UI in settings that allows the user to move multiple passwords to the
// account storage.
const base::Feature kEnableMovingMultiplePasswordsToAccount = {
    "EnableMovingMultiplePasswordsToAccount", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the overwriting of prefilled username fields if the server predicted
// the field to contain a placeholder value.
const base::Feature kEnableOverwritingPlaceholderUsernames{
    "EnableOverwritingPlaceholderUsernames", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a second, Gaia-account-scoped password store for users who are signed
// in but not syncing.
const base::Feature kEnablePasswordsAccountStorage = {
    "EnablePasswordsAccountStorage",
#if defined(OS_ANDROID) || defined(OS_IOS)
    base::FEATURE_DISABLED_BY_DEFAULT
#else
    base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

const base::Feature KEnablePasswordGenerationForClearTextFields = {
    "EnablePasswordGenerationForClearTextFields",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables filling password on a website when there is saved password on
// affiliated website.
const base::Feature kFillingAcrossAffiliatedWebsites{
    "FillingAcrossAffiliatedWebsites", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load, with highlighting of fields.
const base::Feature kFillOnAccountSelect = {"fill-on-account-select",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables finding a confirmation password field during saving by inspecting the
// values of the fields. Used as a kill switch.
// TODO(crbug.com/1164861): Remove once confirmed to be safe (around M92 or so).
const base::Feature kInferConfirmationPasswordField = {
    "InferConfirmationPasswordField", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables password leak detection for unauthenticated users.
const base::Feature kLeakDetectionUnauthenticated = {
    "LeakDetectionUnauthenticated", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables password change flow from leaked password dialog.
const base::Feature kPasswordChange = {"PasswordChange",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables offering automated password change only for compromised credentials
// that are recently used. Only has an effect if PasswordChangeInSettings is
// also enabled.
const base::Feature kPasswordChangeOnlyRecentCredentials = {
    "PasswordChangeOnlyRecentCredentials", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables password change flow from bulk leak check in settings.
const base::Feature kPasswordChangeInSettings = {
    "PasswordChangeInSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls the ability to import passwords from Chrome's settings page.
const base::Feature kPasswordImport = {"PasswordImport",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables password reuse detection.
const base::Feature kPasswordReuseDetectionEnabled = {
    "PasswordReuseDetectionEnabled", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables a revised opt-in flow for the account-scoped password storage.
const base::Feature kPasswordsAccountStorageRevisedOptInFlow = {
    "PasswordsAccountStorageRevisedOptInFlow",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables password scripts fetching for the |PasswordChangeInSettings| feature.
const base::Feature kPasswordScriptsFetching = {
    "PasswordScriptsFetching", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables showing UI which allows users to easily revert their choice to
// never save passwords on a certain website.
const base::Feature kRecoverFromNeverSaveAndroid = {
    "RecoverFromNeverSaveAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables reparsing server predictions once the password form manager notices a
// dynamic form change.
const base::Feature kReparseServerPredictionsFollowingFormChange = {
    "ReparseServerPredictionsFollowingFormChange",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables considering secondary server field predictions during form parsing.
const base::Feature kSecondaryServerFieldPredictions = {
    "SecondaryServerFieldPredictions", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the addition of passwords in Chrome Settings.
// TODO(crbug/1226008): Remove once it's launched.
const base::Feature kSupportForAddPasswordsInSettings = {
    "SupportForAddPasswordsInSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Treat heuritistics to find new password fields as reliable. This enables
// password generation on more forms, but could lead to false positives.
const base::Feature kTreatNewPasswordHeuristicsAsReliable = {
    "TreatNewPasswordHeuristicsAsReliable", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Enables use of Google Mobile Services for password storage. Chrome's local
// database will be unused but kept in sync for local passwords.
const base::Feature kUnifiedPasswordManagerAndroid{
    "UnifiedPasswordManagerAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables migration of passwords from built in storage to Google Mobile
// Services.
const base::Feature kUnifiedPasswordManagerMigration{
    "UnifiedPasswordManagerMigration", base::FEATURE_DISABLED_BY_DEFAULT};

// Sends shadow traffic to Google Mobile Services for password storage. This
// allows to check stability without switching away from the local storage as
// source of truth.
const base::Feature kUnifiedPasswordManagerShadowAndroid{
    "UnifiedPasswordManagerShadowAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the built-in sync functionality in PasswordSyncBridge becomes
// unused, meaning that SyncService/SyncEngine will no longer download or
// upload changes to/from the Sync server. Instead, an external Android-specific
// backend will be used to achieve similar behavior.
const base::Feature kUnifiedPasswordManagerSyncUsingAndroidBackendOnly{
    "UnifiedPasswordManagerSyncUsingAndroidBackendOnly",
    base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables support of sending votes on username first flow. The votes are sent
// on single username forms and are based on user interaction with the save
// prompt.
const base::Feature kUsernameFirstFlow = {"UsernameFirstFlow",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables support of filling and saving on username first flow.
const base::Feature kUsernameFirstFlowFilling = {
    "UsernameFirstFlowFilling", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables support of sending additional votes on username first flow. The votes
// are sent on single password forms and contain information about preceding
// single username forms.
const base::Feature kUsernameFirstFlowFallbackCrowdsourcing = {
    "UsernameFirstFlowFallbackCrowdsourcing",
    base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Current migration version to Google Mobile Services. If version saved in pref
// is lower than 'kMigrationVersion' passwords will be re-uploaded.
extern const base::FeatureParam<int> kMigrationVersion = {
    &kUnifiedPasswordManagerMigration, "migration_version", 0};
#endif

// Field trial identifier for password generation requirements.
const char kGenerationRequirementsFieldTrial[] =
    "PasswordGenerationRequirements";

// The file version number of password requirements files. If the prefix length
// changes, this version number needs to be updated.
// Default to 0 in order to get an empty requirements file.
const char kGenerationRequirementsVersion[] = "version";

// Length of a hash prefix of domain names. This is used to shard domains
// across multiple files.
// Default to 0 in order to put all domain names into the same shard.
const char kGenerationRequirementsPrefixLength[] = "prefix_length";

// Timeout (in milliseconds) for password requirements lookups. As this is a
// network request in the background that does not block the UI, the impact of
// high values is not strong.
// Default to 5000 ms.
const char kGenerationRequirementsTimeout[] = "timeout";

// Enables showing leaked dialog after every successful form submission.
const char kPasswordChangeWithForcedDialogAfterEverySuccessfulSubmission[] =
    "should_force_dialog_after_every_sucessful_form_submission";

// Enables showing leaked warning for every site while doing bulk leak check in
// settings.
const char kPasswordChangeInSettingsWithForcedWarningForEverySite[] =
    "should_force_warning_for_every_site_in_settings";

}  // namespace features

}  // namespace password_manager
