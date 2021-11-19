// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/gaia_id_hash.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

// PasswordForm primary key which is used in the database.
using FormPrimaryKey = base::StrongAlias<class FormPrimaryKeyTag, int>;

// Pair of a value and the name of the element that contained this value.
using ValueElementPair = std::pair<std::u16string, std::u16string>;

// Vector of possible username values and corresponding field names.
using ValueElementVector = std::vector<ValueElementPair>;

using IsMuted = base::StrongAlias<class IsMutedTag, bool>;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class InsecureType {
  // If the credentials was leaked by a data breach.
  kLeaked = 0,
  // If the credentials was entered on a phishing site.
  kPhished = 1,
  // If the password is weak.
  kWeak = 2,
  // If the password is reused for other accounts.
  kReused = 3,
  kMaxValue = kReused
};

// Metadata for insecure credentials
struct InsecurityMetadata {
  InsecurityMetadata();
  InsecurityMetadata(base::Time create_time, IsMuted is_muted);
  InsecurityMetadata(const InsecurityMetadata& rhs);
  ~InsecurityMetadata();

  // The date when the record was created.
  base::Time create_time;
  // Whether the problem was explicitly muted by the user.
  IsMuted is_muted{false};
};

bool operator==(const InsecurityMetadata& lhs, const InsecurityMetadata& rhs);

// The PasswordForm struct encapsulates information about a login form,
// which can be an HTML form or a dialog with username/password text fields.
//
// The Web Data database stores saved username/passwords and associated form
// metdata using a PasswordForm struct, typically one that was created from
// a parsed HTMLFormElement or LoginDialog, but the saved entries could have
// also been created by imported data from another browser.
//
// The PasswordManager implements a fuzzy-matching algorithm to compare saved
// PasswordForm entries against PasswordForms that were created from a parsed
// HTML or dialog form. As one might expect, the more data contained in one
// of the saved PasswordForms, the better the job the PasswordManager can do
// in matching it against the actual form it was saved on, and autofill
// accurately. But it is not always possible, especially when importing from
// other browsers with different data models, to copy over all the information
// about a particular "saved password entry" to our PasswordForm
// representation.
//
// The field descriptions in the struct specification below are intended to
// describe which fields are not strictly required when adding a saved password
// entry to the database and how they can affect the matching process.
struct PasswordForm {
  // Enum to differentiate between HTML form based authentication, and dialogs
  // using basic or digest schemes. Default is kHtml. Only PasswordForms of the
  // same Scheme will be matched/autofilled against each other.
  enum class Scheme {
    kHtml,
    kBasic,
    kDigest,
    kOther,
    kUsernameOnly,
    kMinValue = kHtml,
    kMaxValue = kUsernameOnly,
  };

  // Enum to differentiate between manually filled forms, forms with auto-
  // generated passwords, forms generated from the Credential Management
  // API and credentials manually added from setting.
  //
  // Always append new types at the end. This enum is converted to int and
  // stored in password store backends, so it is important to keep each
  // value assigned to the same integer.
  enum class Type {
    kFormSubmission = 0,
    kGenerated = 1,
    kApi = 2,
    kManuallyAdded = 3,
    kMinValue = kFormSubmission,
    kMaxValue = kManuallyAdded,
  };

  // Enum to keep track of what information has been sent to the server about
  // this form regarding password generation.
  enum class GenerationUploadStatus {
    kNoSignalSent,
    kPositiveSignalSent,
    kNegativeSignalSent,
    kMinValue = kNoSignalSent,
    kMaxValue = kNegativeSignalSent,
  };

  Scheme scheme = Scheme::kHtml;

  // The "Realm" for the sign-on. This is scheme, host, port for SCHEME_HTML.
  // Dialog based forms also contain the HTTP realm. Android based forms will
  // contain a string of the form "android://<hash of cert>@<package name>"
  //
  // The signon_realm is effectively the primary key used for retrieving
  // data from the database, so it must not be empty.
  std::string signon_realm;

  // An URL consists of the scheme, host, port and path; the rest is stripped.
  // This is the primary data used by the PasswordManager to decide (in longest
  // matching prefix fashion) whether or not a given PasswordForm result from
  // the database is a good fit for a particular form on a page.
  //
  // This should not be empty except for Android based credentials.
  GURL url;

  // The action target of the form; like |url|, consists of the scheme, host,
  // port and path; the rest is stripped. This is the primary data used by the
  // PasswordManager for form autofill; that is, the action of the saved
  // credentials must match the action of the form on the page to be autofilled.
  // If this is empty / not available, it will result in a "restricted" IE-like
  // autofill policy, where we wait for the user to type in their username
  // before autofilling the password. In these cases, after successful login the
  // action URL will automatically be assigned by the PasswordManager.
  //
  // When parsing an HTML form, this must always be set.
  GURL action;

  // The web realm affiliated with the Android application, if the form is an
  // Android credential. Otherwise, the string is empty. If there are several
  // realms affiliated with the application, an arbitrary realm is chosen. The
  // field is filled out when the PasswordStore injects affiliation and branding
  // information, i.e. in InjectAffiliationAndBrandingInformation. If there was
  // no prior call to this method, the string is empty.
  std::string affiliated_web_realm;

  // The display name (e.g. Play Store name) of the Android application if the
  // form is an Android credential. Otherwise, the string is empty. The field is
  // filled out when the PasswordStore injects affiliation and branding
  // information, i.e. in InjectAffiliationAndBrandingInformation. If there was
  // no prior call to this method, the string is empty.
  std::string app_display_name;

  // The icon URL (e.g. Play Store icon URL) of the Android application if the
  // form is an Android credential. Otherwise, the URL is empty. The field is
  // filled out when the PasswordStore injects affiliation and branding
  // information, i.e. in InjectAffiliationAndBrandingInformation. If there was
  // no prior call to this method, the URL is empty.
  GURL app_icon_url;

  // The name of the submit button used. Optional; only used in scoring
  // of PasswordForm results from the database to make matches as tight as
  // possible.
  std::u16string submit_element;

  // The name of the username input element.
  std::u16string username_element;

  // The renderer id of the username input element. It is set during the new
  // form parsing and not persisted.
  autofill::FieldRendererId username_element_renderer_id;

  // True if the server-side classification was successful.
  bool server_side_classification_successful = false;

  // True if the server-side classification believes that the field may be
  // pre-filled with a placeholder in the value attribute. It is set during
  // form parsing and not persisted.
  bool username_may_use_prefilled_placeholder = false;

  // When parsing an HTML form, this is typically empty unless the site
  // has implemented some form of autofill.
  std::u16string username_value;

  // This member is populated in cases where we there are multiple input
  // elements that could possibly be the username. Used when our heuristics for
  // determining the username are incorrect. Optional.
  ValueElementVector all_possible_usernames;

  // This member is populated in cases where we there are multiple possible
  // password values. Used in pending password state, to populate a dropdown
  // for possible passwords. Contains all possible passwords. Optional.
  ValueElementVector all_possible_passwords;

  // True if |all_possible_passwords| have autofilled value or its part.
  bool form_has_autofilled_value = false;

  // The name of the input element corresponding to the current password.
  // Optional (improves scoring).
  //
  // When parsing an HTML form, this will always be set, unless it is a sign-up
  // form or a change password form that does not ask for the current password.
  // In these two cases the |new_password_element| will always be set.
  std::u16string password_element;

  // The renderer id of the password input element. It is set during the new
  // form parsing and not persisted.
  autofill::FieldRendererId password_element_renderer_id;

  // The current password. Must be non-empty for PasswordForm instances that are
  // meant to be persisted to the password store.
  //
  // When parsing an HTML form, this is typically empty.
  std::u16string password_value;

  // The current encrypted password. Must be non-empty for PasswordForm
  // instances retrieved from the password store or coming in a
  // PasswordStoreChange that is not of type REMOVE.
  std::string encrypted_password;

  // If the form was a sign-up or a change password form, the name of the input
  // element corresponding to the new password. Optional, and not persisted.
  std::u16string new_password_element;

  // The renderer id of the new password input element. It is set during the new
  // form parsing and not persisted.
  autofill::FieldRendererId new_password_element_renderer_id;

  // The confirmation password element. Optional, only set on form parsing, and
  // not persisted.
  std::u16string confirmation_password_element;

  // The renderer id of the confirmation password input element. It is set
  // during the new form parsing and not persisted.
  autofill::FieldRendererId confirmation_password_element_renderer_id;

  // The new password. Optional, and not persisted.
  std::u16string new_password_value;

  // When the login was last used by the user to login to the site. Defaults to
  // |date_created|, except for passwords that were migrated from the now
  // deprecated |preferred| flag. Their default is set when migrating the login
  // database to have the "date_last_used" column.
  //
  // When parsing an HTML form, this is not used.
  base::Time date_last_used;

  // When the password value was last changed. The date can be unset on the old
  // credentials because the passwords wasn't modified yet. The code must keep
  // it in mind and fallback to 'date_last_used' or 'date_created'.
  //
  // When parsing an HTML form, this is not used.
  base::Time date_password_modified;

  // When the login was saved (by chrome).
  //
  // When parsing an HTML form, this is not used.
  base::Time date_created;

  // Tracks if the user opted to never remember passwords for this form. Default
  // to false.
  //
  // When parsing an HTML form, this is not used.
  bool blocked_by_user = false;

  // The form type.
  Type type = Type::kFormSubmission;

  // The number of times that this username/password has been used to
  // authenticate the user.
  //
  // When parsing an HTML form, this is not used.
  int times_used = 0;

  // Autofill representation of this form. Used to communicate with the
  // Autofill servers if necessary. Currently this is only used to help
  // determine forms where we can trigger password generation.
  //
  // When parsing an HTML form, this is normally set.
  autofill::FormData form_data;

  // What information has been sent to the Autofill server about this form.
  GenerationUploadStatus generation_upload_status =
      GenerationUploadStatus::kNoSignalSent;

  // These following fields are set by a website using the Credential Manager
  // API. They will be empty and remain unused for sites which do not use that
  // API.
  //
  // User friendly name to show in the UI.
  std::u16string display_name;

  // The URL of this credential's icon, such as the user's avatar, to display
  // in the UI.
  GURL icon_url;

  // The origin of identity provider used for federated login.
  url::Origin federation_origin;

  // If true, Chrome will not return this credential to a site in response to
  // 'navigator.credentials.request()' without user interaction.
  // Once user selects this credential the flag is reseted.
  bool skip_zero_click = false;

  // If true, this form was parsed using Autofill predictions.
  bool was_parsed_using_autofill_predictions = false;

  // If true, this match was found using public suffix matching.
  bool is_public_suffix_match = false;

  // If true, this is a credential found using affiliation-based match.
  bool is_affiliation_based_match = false;

  // The type of the event that was taken as an indication that this form is
  // being or has already been submitted. This field is not persisted and filled
  // out only for submitted forms.
  autofill::mojom::SubmissionIndicatorEvent submission_event =
      autofill::mojom::SubmissionIndicatorEvent::NONE;

  // True iff heuristics declined this form for normal saving or filling (e.g.
  // only credit card fields were found). But this form can be saved or filled
  // only with the fallback.
  bool only_for_fallback = false;

  // True iff the new password field was found with server hints or autocomplete
  // attributes or the kTreatNewPasswordHeuristicsAsReliable feature is enabled.
  // Only set on form parsing for filling, and not persisted. Used as signal for
  // password generation eligibility.
  bool is_new_password_reliable = false;

  // True iff the form may be filled with webauthn credentials from an active
  // webauthn request.
  bool accepts_webauthn_credentials = false;

  // Serialized to prefs, so don't change numeric values!
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Store {
    // Default value.
    kNotSet = 0,
    // Credential came from the profile (i.e. local) storage.
    kProfileStore = 1 << 0,
    // Credential came from the Gaia-account-scoped storage.
    kAccountStore = 1 << 1,
    kMaxValue = kAccountStore
  };

  // Please use IsUsingAccountStore and IsUsingProfileStore to check in which
  // store the form is present.
  // TODO(crbug.com/1201643): Rename to in_stores to reflect possibility of
  // password presence in both stores.
  Store in_store = Store::kNotSet;

  // Vector of hashes of the gaia id for users who prefer not to move this
  // password form to their account. This list is used to suppress the move
  // prompt for those users.
  std::vector<autofill::GaiaIdHash> moving_blocked_for_list;

  // A mapping from the credential insecurity type (e.g. leaked, phished),
  // to its metadata (e.g. time it was discovered, whether alerts are muted).
  base::flat_map<InsecureType, InsecurityMetadata> password_issues;

  // Return true if we consider this form to be a change password form and not
  // a signup form. It's based on local heuristics and may be inaccurate.
  bool IsLikelyChangePasswordForm() const;

  // Returns true if current password element is set.
  bool HasUsernameElement() const;

  // Returns true if current password element is set.
  bool HasPasswordElement() const;

  // Returns true if current password element is set.
  bool HasNewPasswordElement() const;

  // True iff |federation_origin| isn't empty.
  bool IsFederatedCredential() const;

  // True if username element is set and password and new password elements are
  // not set.
  bool IsSingleUsername() const;

  // Returns whether this form is stored in the account-scoped store.
  bool IsUsingAccountStore() const;

  // Returns whether this form is stored in the profile-scoped store.
  bool IsUsingProfileStore() const;

  // Returns true when |password_value| or |new_password_value| are non-empty.
  bool HasNonEmptyPasswordValue() const;

  // Utility method to check whether the form represents an insecure credential
  // of insecure type `type`.
  bool IsInsecureCredential(InsecureType insecure_type) const;

  PasswordForm();
  PasswordForm(const PasswordForm& other);
  PasswordForm(PasswordForm&& other);
  ~PasswordForm();

  PasswordForm& operator=(const PasswordForm& form);
  PasswordForm& operator=(PasswordForm&& form);
};

// True if the unique keys for the forms are the same. The unique key is
// (url, username_element, username_value, password_element, signon_realm).
inline auto PasswordFormUniqueKey(const PasswordForm& f) {
  return std::tie(f.signon_realm, f.url, f.username_element, f.username_value,
                  f.password_element);
}
bool ArePasswordFormUniqueKeysEqual(const PasswordForm& left,
                                    const PasswordForm& right);

// For testing.
#if defined(UNIT_TEST)
// An exact equality comparison of all the fields is only useful for tests.
// Production code should be using `ArePasswordFormUniqueKeysEqual` instead.
bool operator==(const PasswordForm& lhs, const PasswordForm& rhs);
bool operator!=(const PasswordForm& lhs, const PasswordForm& rhs);

std::ostream& operator<<(std::ostream& os, PasswordForm::Scheme scheme);
std::ostream& operator<<(std::ostream& os, const PasswordForm& form);
std::ostream& operator<<(std::ostream& os, PasswordForm* form);
#endif

constexpr PasswordForm::Store operator&(PasswordForm::Store lhs,
                                        PasswordForm::Store rhs) {
  return static_cast<PasswordForm::Store>(static_cast<int>(lhs) &
                                          static_cast<int>(rhs));
}

constexpr PasswordForm::Store operator|(PasswordForm::Store lhs,
                                        PasswordForm::Store rhs) {
  return static_cast<PasswordForm::Store>(static_cast<int>(lhs) |
                                          static_cast<int>(rhs));
}

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_H_
