// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_METRICS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_METRICS_H_

#include <limits.h>

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin_metrics {

// Track all the ways a profile can become signed out as a histogram.
// Enum SigninSignoutProfile.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SignoutReason
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum ProfileSignout : int {
  // The value used within unit tests.
  SIGNOUT_TEST = 0,
  // The preference or policy controlling if signin is valid has changed.
  SIGNOUT_PREF_CHANGED = 0,
  // The valid pattern for signing in to the Google service changed.
  GOOGLE_SERVICE_NAME_PATTERN_CHANGED = 1,
  // The preference or policy controlling if signin is valid changed during
  // the signin process.
  SIGNIN_PREF_CHANGED_DURING_SIGNIN = 2,
  // User clicked to signout from the settings page.
  USER_CLICKED_SIGNOUT_SETTINGS = 3,
  // The signin process was aborted, but signin had succeeded, so signout. This
  // may be due to a server response, policy definition or user action.
  ABORT_SIGNIN = 4,
  // The sync server caused the profile to be signed out.
  SERVER_FORCED_DISABLE = 5,
  // The credentials are being transfered to a new profile, so the old one is
  // signed out.
  TRANSFER_CREDENTIALS = 6,
  // Signed out because credentials are invalid and force-sign-in is enabled.
  AUTHENTICATION_FAILED_WITH_FORCE_SIGNIN = 7,
  // The user disables sync from the DICE UI.
  USER_TUNED_OFF_SYNC_FROM_DICE_UI = 8,
  // Signout forced because the account was removed from the device.
  ACCOUNT_REMOVED_FROM_DEVICE = 9,
  // Signin is no longer allowed when the profile is initialized.
  SIGNIN_NOT_ALLOWED_ON_PROFILE_INIT = 10,
  // Sign out is forced allowed. Only used for tests.
  FORCE_SIGNOUT_ALWAYS_ALLOWED_FOR_TEST = 11,
  // User cleared account cookies when there's no sync consent, which has caused
  // sign out.
  USER_DELETED_ACCOUNT_COOKIES = 12,
  // Signout triggered by MobileIdentityConsistency rollback.
  MOBILE_IDENTITY_CONSISTENCY_ROLLBACK = 13,
  // Sign-out when the account id migration to Gaia ID did not finish,
  ACCOUNT_ID_MIGRATION = 14,
  // iOS Specific. Sign-out forced because the account was removed from the
  // device after a device restore.
  IOS_ACCOUNT_REMOVED_FROM_DEVICE_AFTER_RESTORE = 15,
  // Keep this as the last enum.
  NUM_PROFILE_SIGNOUT_METRICS,
};

// Enum values used for use with "AutoLogin.Reverse" histograms.
enum AccessPointAction {
  // The infobar was shown to the user.
  HISTOGRAM_SHOWN,
  // The user pressed the accept button to perform the suggested action.
  HISTOGRAM_ACCEPTED,
  // The user pressed the reject to turn off the feature.
  HISTOGRAM_REJECTED,
  // The user pressed the X button to dismiss the infobar this time.
  HISTOGRAM_DISMISSED,
  // The user completely ignored the infobar.  Either they navigated away, or
  // they used the page as is.
  HISTOGRAM_IGNORED,
  // The user clicked on the learn more link in the infobar.
  HISTOGRAM_LEARN_MORE,
  // The sync was started with default settings.
  HISTOGRAM_WITH_DEFAULTS,
  // The sync was started with advanced settings.
  HISTOGRAM_WITH_ADVANCED,
  // The sync was started through auto-accept with default settings.
  HISTOGRAM_AUTO_WITH_DEFAULTS,
  // The sync was started through auto-accept with advanced settings.
  HISTOGRAM_AUTO_WITH_ADVANCED,
  // The sync was aborted with an undo button.
  HISTOGRAM_UNDO,
  HISTOGRAM_MAX
};

// Enum values used with the "Signin.OneClickConfirmation" histogram, which
// tracks the actions used in the OneClickConfirmation bubble.
enum ConfirmationUsage {
  HISTOGRAM_CONFIRM_SHOWN,
  HISTOGRAM_CONFIRM_OK,
  HISTOGRAM_CONFIRM_RETURN,
  HISTOGRAM_CONFIRM_ADVANCED,
  HISTOGRAM_CONFIRM_CLOSE,
  HISTOGRAM_CONFIRM_ESCAPE,
  HISTOGRAM_CONFIRM_UNDO,
  HISTOGRAM_CONFIRM_LEARN_MORE,
  HISTOGRAM_CONFIRM_LEARN_MORE_OK,
  HISTOGRAM_CONFIRM_LEARN_MORE_RETURN,
  HISTOGRAM_CONFIRM_LEARN_MORE_ADVANCED,
  HISTOGRAM_CONFIRM_LEARN_MORE_CLOSE,
  HISTOGRAM_CONFIRM_LEARN_MORE_ESCAPE,
  HISTOGRAM_CONFIRM_LEARN_MORE_UNDO,
  HISTOGRAM_CONFIRM_MAX
};

// TODO(gogerald): right now, gaia server needs to distinguish the source from
// signin_metrics::SOURCE_START_PAGE, signin_metrics::SOURCE_SETTINGS and the
// others to show advanced sync setting, remove them after switching to Minute
// Maid sign in flow.
// This was previously used in Signin.SigninSource UMA histogram, but no longer
// used after having below AccessPoint and Reason related histograms.
enum Source {
  SOURCE_START_PAGE = 0,  // This must be first.
  SOURCE_SETTINGS = 3,
  SOURCE_OTHERS = 13,
};

// Enum values which enumerates all access points where sign in could be
// initiated. Not all of them exist on all platforms. They are used with
// "Signin.SigninStartedAccessPoint" and "Signin.SigninCompletedAccessPoint"
// histograms.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SigninAccessPoint
enum class AccessPoint : int {
  ACCESS_POINT_START_PAGE = 0,
  ACCESS_POINT_NTP_LINK = 1,
  ACCESS_POINT_MENU = 2,
  ACCESS_POINT_SETTINGS = 3,
  ACCESS_POINT_SUPERVISED_USER = 4,
  ACCESS_POINT_EXTENSION_INSTALL_BUBBLE = 5,
  ACCESS_POINT_EXTENSIONS = 6,
  // ACCESS_POINT_APPS_PAGE_LINK = 7, no longer used.
  ACCESS_POINT_BOOKMARK_BUBBLE = 8,
  ACCESS_POINT_BOOKMARK_MANAGER = 9,
  ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN = 10,
  ACCESS_POINT_USER_MANAGER = 11,
  ACCESS_POINT_DEVICES_PAGE = 12,
  ACCESS_POINT_CLOUD_PRINT = 13,
  ACCESS_POINT_CONTENT_AREA = 14,
  ACCESS_POINT_SIGNIN_PROMO = 15,
  ACCESS_POINT_RECENT_TABS = 16,
  // This should never have been used to get signin URL.
  ACCESS_POINT_UNKNOWN = 17,
  ACCESS_POINT_PASSWORD_BUBBLE = 18,
  ACCESS_POINT_AUTOFILL_DROPDOWN = 19,
  ACCESS_POINT_NTP_CONTENT_SUGGESTIONS = 20,
  ACCESS_POINT_RESIGNIN_INFOBAR = 21,
  ACCESS_POINT_TAB_SWITCHER = 22,
  // ACCESS_POINT_FORCE_SIGNIN_WARNING is no longer used.
  ACCESS_POINT_SAVE_CARD_BUBBLE = 24,
  ACCESS_POINT_MANAGE_CARDS_BUBBLE = 25,
  ACCESS_POINT_MACHINE_LOGON = 26,
  ACCESS_POINT_GOOGLE_SERVICES_SETTINGS = 27,
  ACCESS_POINT_SYNC_ERROR_CARD = 28,
  ACCESS_POINT_FORCED_SIGNIN = 29,
  ACCESS_POINT_ACCOUNT_RENAMED = 30,
  ACCESS_POINT_WEB_SIGNIN = 31,
  ACCESS_POINT_SAFETY_CHECK = 32,
  ACCESS_POINT_KALEIDOSCOPE = 33,
  ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR = 34,
  ACCESS_POINT_MAX,  // This must be last.
};

// Enum values which enumerates all access points where transactional reauth
// could be initiated. Transactional reauth is used when the user already has
// a valid refresh token but a system still wants to verify user's identity.
enum class ReauthAccessPoint {
  // The code expects kUnknown to be the first, so it should not be reordered.
  kUnknown = 0,

  // Account password storage opt-in:
  kAutofillDropdown = 1,
  // The password save bubble, which included the destination picker (set to
  // "Save to your Google Account").
  kPasswordSaveBubble = 2,
  kPasswordSettings = 3,
  kGeneratePasswordDropdown = 4,
  kGeneratePasswordContextMenu = 5,
  kPasswordMoveBubble = 6,
  // The password save bubble *without* a destination picker, i.e. the password
  // was already saved locally.
  kPasswordSaveLocallyBubble = 7,

  kMaxValue = kPasswordSaveLocallyBubble
};

// Enum values which enumerates all user actions on the sign-in promo.
enum class PromoAction : int {
  PROMO_ACTION_NO_SIGNIN_PROMO = 0,
  // The user selected the default account.
  PROMO_ACTION_WITH_DEFAULT,
  // On desktop, the user selected an account that is not the default. On
  // mobile, the user selected the generic "Use another account" button.
  PROMO_ACTION_NOT_DEFAULT,
  // Non personalized promo, when there is no account on the device.
  PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT,
  // The user clicked on the "Add account" button, when there are already
  // accounts on the device. (desktop only, the button does not exist on
  // mobile).
  PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT
};

#if defined(OS_ANDROID) || defined(OS_IOS)
// This class is used to record user action that was taken after
// receiving the header from Gaia in the web sign-in flow.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AccountConsistencyPromoAction
enum class AccountConsistencyPromoAction : int {
  // Promo is not shown as there are no accounts on device.
  SUPPRESSED_NO_ACCOUNTS = 0,
  // User has dismissed the promo by tapping back button.
  DISMISSED_BACK = 1,
  // User has tapped |Add account to device| from expanded account list.
  ADD_ACCOUNT_STARTED = 2,

  // Deprecated 05/2021, since the Incognito option has been removed from
  // account picker bottomsheet.
  // STARTED_INCOGNITO_SESSION = 3,

  // User has selected the default account and signed in with it
  SIGNED_IN_WITH_DEFAULT_ACCOUNT = 4,
  // User has selected one of the non default accounts and signed in with it.
  SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT = 5,
  // The promo was shown to user.
  SHOWN = 6,
  // Promo is not shown due to sign-in being disallowed either by an enterprise
  // policy
  // or by |Allow Chrome sign-in| toggle.
  SUPPRESSED_SIGNIN_NOT_ALLOWED = 7,
  // User has added an account and signed in with this account.
  // When this metric is recorded, we won't record
  // SIGNED_IN_WITH_DEFAULT_ACCOUNT or
  // SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT.
  SIGNED_IN_WITH_ADDED_ACCOUNT = 8,
  // User has dismissed the promo by tapping on the scrim above the bottom
  // sheet.
  DISMISSED_SCRIM = 9,
  // User has dismissed the promo by swiping down the bottom sheet.
  DISMISSED_SWIPE_DOWN = 10,
  // User has dismissed the promo by other means.
  DISMISSED_OTHER = 11,
  // The auth error screen was shown to the user.
  AUTH_ERROR_SHOWN = 12,
  // The generic error screen was shown to the user.
  GENERIC_ERROR_SHOWN = 13,
  // User has dismissed the promo by tapping on the dismissal button in the
  // bottom sheet.
  DISMISSED_BUTTON = 14,
  // User has completed the account addition flow triggered from the bottom
  // sheet.
  ADD_ACCOUNT_COMPLETED = 15,
  // The bottom sheet was suppressed as the user hit consecutive active
  // dismissal limit.
  SUPPRESSED_CONSECUTIVE_DISMISSALS = 16,
  // The timeout erreur was shown to the user.
  TIMEOUT_ERROR_SHOWN = 17,
  MAX = 18,
};
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

// Enum values which enumerates all reasons to start sign in process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Please keep in Sync with "SigninReason" in
// src/tools/metrics/histograms/enums.xml.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SigninReason
enum class Reason : int {
  kSigninPrimaryAccount = 0,
  kAddSecondaryAccount = 1,
  kReauthentication = 2,
  // REASON_UNLOCK = 3,  // DEPRECATED, profile unlocking was removed.
  // This should never have been used to get signin URL.
  kUnknownReason = 4,
  kForcedSigninPrimaryAccount = 5,
  // Used to simply login and acquire a login scope token without actually
  // signing into any profiles on Chrome. This allows the chrome signin page to
  // work in incognito mode.
  kFetchLstOnly = 6,
  kMaxValue = kFetchLstOnly,
};

// Enum values used for "Signin.AccountReconcilorState.OnGaiaResponse"
// histogram, which records the state of the AccountReconcilor when GAIA returns
// a specific response.
enum AccountReconcilorState {
  // The AccountReconcilor has finished running and is up to date.
  ACCOUNT_RECONCILOR_OK,
  // The AccountReconcilor is running and gathering information.
  ACCOUNT_RECONCILOR_RUNNING,
  // The AccountReconcilor encountered an error and stopped.
  ACCOUNT_RECONCILOR_ERROR,
  // The account reconcilor will start running soon.
  ACCOUNT_RECONCILOR_SCHEDULED,
  // Always the last enumerated type.
  ACCOUNT_RECONCILOR_HISTOGRAM_COUNT,
};

// Values of histogram comparing account id and email.
enum class AccountEquality : int {
  // Expected case when the user is not switching accounts.
  BOTH_EQUAL = 0,
  // Expected case when the user is switching accounts.
  BOTH_DIFFERENT,
  // The user has changed at least two email account names. This is actually
  // a different account, even though the email matches.
  ONLY_SAME_EMAIL,
  // The user has changed the email of their account, but the account is
  // actually the same.
  ONLY_SAME_ID,
  // The last account id was not present, email equality was used. This should
  // happen once to all old clients. Does not differentiate between same and
  // different accounts.
  EMAIL_FALLBACK,
  // Always the last enumerated type.
  HISTOGRAM_COUNT,
};

// Values of Signin.AccountType histogram. This histogram records if the user
// uses a gmail account or a managed account when signing in.
enum class SigninAccountType : int {
  // Gmail account.
  kRegular = 0,
  // Managed account.
  kManaged = 1,
  // Always the last enumerated type.
  kMaxValue = kManaged,
};

// When the user is give a choice of deleting their profile or not when signing
// out, the |kDeleted| or |kKeeping| metric should be used. If the user is not
// given any option, then use the |kIgnoreMetric| value should be used.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
enum class SignoutDelete : int {
  kDeleted = 0,
  kKeeping,
  kIgnoreMetric,
};

// This is the relationship between the account used to sign into chrome, and
// the account(s) used to sign into the content area/cookie jar. This enum
// gets messy because we're trying to capture quite a few things, if there was
// a match or not, how many accounts were in the cookie jar, and what state
// those cookie jar accounts were in (signed out vs signed in). Note that it's
// not possible to have the same account multiple times in the cookie jar.
enum class AccountRelation : int {
  // No signed in or out accounts in the content area. Cannot have a match.
  EMPTY_COOKIE_JAR = 0,
  // The cookie jar contains only a single signed out account that matches.
  NO_SIGNED_IN_SINGLE_SIGNED_OUT_MATCH,
  // The cookie jar contains only signed out accounts, one of which matches.
  NO_SIGNED_IN_ONE_OF_SIGNED_OUT_MATCH,
  // The cookie jar contains one or more signed out accounts, none match.
  NO_SIGNED_IN_WITH_SIGNED_OUT_NO_MATCH,
  // The cookie jar contains only a single signed in account that matches.
  SINGLE_SIGNED_IN_MATCH_NO_SIGNED_OUT,
  // There's only one signed in account which matches, and there are one or
  // more signed out accounts.
  SINGLE_SINGED_IN_MATCH_WITH_SIGNED_OUT,
  // There's more than one signed in account, one of which matches. There
  // could be any configuration of signed out accounts.
  ONE_OF_SIGNED_IN_MATCH_ANY_SIGNED_OUT,
  // There's one or more signed in accounts, none of which match. However
  // there is a match in the signed out accounts.
  WITH_SIGNED_IN_ONE_OF_SIGNED_OUT_MATCH,
  // There's one or more signed in accounts and any configuration of signed
  // out accounts. However, none of the accounts match.
  WITH_SIGNED_IN_NO_MATCH,
  // Always the last enumerated type.
  HISTOGRAM_COUNT,
};

// Various sources for refresh token operations (e.g. update or revoke
// credentials).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SourceForRefreshTokenOperation {
  kUnknown = 0,
  kTokenService_LoadCredentials = 1,
  // DEPRECATED
  // kSupervisedUser_InitSync = 2,
  kInlineLoginHandler_Signin = 3,
  kPrimaryAccountManager_ClearAccount = 4,
  kPrimaryAccountManager_LegacyPreDiceSigninFlow = 5,
  kUserMenu_RemoveAccount = 6,
  kUserMenu_SignOutAllAccounts = 7,
  kSettings_Signout = 8,
  kSettings_PauseSync = 9,
  kAccountReconcilor_GaiaCookiesDeletedByUser = 10,
  kAccountReconcilor_GaiaCookiesUpdated = 11,
  kAccountReconcilor_Reconcile = 12,
  kDiceResponseHandler_Signin = 13,
  kDiceResponseHandler_Signout = 14,
  kDiceTurnOnSyncHelper_Abort = 15,
  kMachineLogon_CredentialProvider = 16,
  kTokenService_ExtractCredentials = 17,
  // DEPRECATED on 09/2021 (used for force migration to DICE)
  // kAccountReconcilor_RevokeTokensNotInCookies = 18,
  kLogoutTabHelper_PrimaryPageChanged = 19,

  kMaxValue = kLogoutTabHelper_PrimaryPageChanged,
};

// Different types of reporting. This is used as a histogram suffix.
enum class ReportingType { PERIODIC, ON_CHANGE };

// Result for fetching account capabilities from the system library, used to
// record histogram Signin.AccountCapabilities.GetFromSystemLibraryResult.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.metrics
enum class FetchAccountCapabilitiesFromSystemLibraryResult {
  // Errors common to iOS and Android.
  kSuccess = 0,
  kErrorGeneric = 1,

  // Errors from 10 to 19 are reserved for Android.
  kApiRequestFailed = 10,
  kApiError = 11,
  kApiNotPermitted = 12,
  kApiUnknownCapability = 13,
  kApiFailedToSync = 14,
  kApiNotAvailable = 15,

  // Errors after 20 are reserved for iOS.
  kErrorMissingCapability = 20,
  kErrorUnexpectedValue = 21,

  kMaxValue = kErrorUnexpectedValue
};

// -----------------------------------------------------------------------------
// Histograms
// -----------------------------------------------------------------------------

// Tracks the access point of sign in.
void LogSigninAccessPointStarted(AccessPoint access_point,
                                 PromoAction promo_action);
void LogSigninAccessPointCompleted(AccessPoint access_point,
                                   PromoAction promo_action);

// Tracks the reason of sign in.
void LogSigninReason(Reason reason);

// Logs to UMA histograms how many accounts are in the browser for this
// profile.
void RecordAccountsPerProfile(int total_number_accounts);

// Logs duration of a single execution of AccountReconciler to UMA histograms.
// |duration| - How long execution of AccountReconciler took.
// |successful| - True if AccountReconciler was successful.
void LogSigninAccountReconciliationDuration(base::TimeDelta duration,
                                            bool successful);

// Track a profile signout.
void LogSignout(ProfileSignout source_metric, SignoutDelete delete_metric);

// Tracks whether the external connection results were all fetched before
// the gaia cookie manager service tried to use them with merge session.
// |time_to_check_connections| is the time it took to complete.
void LogExternalCcResultFetches(
    bool fetches_completed,
    const base::TimeDelta& time_to_check_connections);

// Track when the current authentication error changed.
void LogAuthError(const GoogleServiceAuthError& auth_error);

// Records the AccountReconcilor |state| when GAIA returns a specific response.
// If |state| is different than ACCOUNT_RECONCILOR_OK it means the user will
// be shown a different set of accounts in the content-area and the settings UI.
void LogAccountReconcilorStateOnGaiaResponse(AccountReconcilorState state);

// Records the AccountEquality metric when an investigator compares the current
// and previous id/emails during a signin.
void LogAccountEquality(AccountEquality equality);

// Records the amount of time since the cookie jar was last changed.
void LogCookieJarStableAge(const base::TimeDelta stable_age,
                           const ReportingType type);

// Records three counts for the number of accounts in the cookie jar.
void LogCookieJarCounts(const int signed_in,
                        const int signed_out,
                        const int total,
                        const ReportingType type);

// Records the relation between the account signed into Chrome, and the
// account(s) present in the cookie jar.
void LogAccountRelation(const AccountRelation relation,
                        const ReportingType type);

// Records if the best guess is that this profile is currently shared or not
// between multiple users.
void LogIsShared(const bool is_shared, const ReportingType type);

// Records the number of signed-in accounts in the cookie jar for the given
// (potentially unconsented) primary account type, characterized by sync being
// enabled (`primary_syncing`) and the account being managed (i.e. enterprise,
// `primary_managed`).
void LogSignedInCookiesCountsPerPrimaryAccountType(int signed_in_accounts_count,
                                                   bool primary_syncing,
                                                   bool primary_managed);

// Records the source that updated a refresh token.
void RecordRefreshTokenUpdatedFromSource(bool refresh_token_is_valid,
                                         SourceForRefreshTokenOperation source);

// Records the source that revoked a refresh token.
void RecordRefreshTokenRevokedFromSource(SourceForRefreshTokenOperation source);

// Records the account type when the user signs in.
void RecordSigninAccountType(signin::ConsentLevel consent_level,
                             bool is_managed_account);

// -----------------------------------------------------------------------------
// User actions
// -----------------------------------------------------------------------------

// Records corresponding sign in user action for an access point.
void RecordSigninUserActionForAccessPoint(AccessPoint access_point,
                                          PromoAction promo_action);

// Records |Signin_ImpressionWithAccount_From*| user action.
void RecordSigninImpressionUserActionForAccessPoint(AccessPoint access_point);

// Records |Signin_Impression{With|No}Account_From*| user action.
void RecordSigninImpressionWithAccountUserActionForAccessPoint(
    AccessPoint access_point,
    bool with_account);

#if defined(OS_IOS)
// Records |Signin.AccountConsistencyPromoAction| histogram.
void RecordConsistencyPromoUserAction(AccountConsistencyPromoAction action);
#endif  // defined(OS_IOS)

}  // namespace signin_metrics

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_METRICS_H_
