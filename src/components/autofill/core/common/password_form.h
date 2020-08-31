// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_H__
#define COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_H__

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/gaia_id_hash.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/renderer_id.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

// Pair of a value and the name of the element that contained this value.
using ValueElementPair = std::pair<base::string16, base::string16>;

// Vector of possible username values and corresponding field names.
using ValueElementVector = std::vector<ValueElementPair>;

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
  using Scheme = mojom::PasswordForm_Scheme;
  using Type = mojom::PasswordForm_Type;
  using GenerationUploadStatus = mojom::PasswordForm_GenerationUploadStatus;

  Scheme scheme = Scheme::kHtml;

  // The "Realm" for the sign-on. This is scheme, host, port for SCHEME_HTML.
  // Dialog based forms also contain the HTTP realm. Android based forms will
  // contain a string of the form "android://<hash of cert>@<package name>"
  //
  // The signon_realm is effectively the primary key used for retrieving
  // data from the database, so it must not be empty.
  std::string signon_realm;

  // An origin URL consists of the scheme, host, port and path; the rest is
  // stripped. This is the primary data used by the PasswordManager to decide
  // (in longest matching prefix fashion) whether or not a given PasswordForm
  // result from the database is a good fit for a particular form on a page.
  // This should not be empty except for Android based credentials.
  // TODO(melandory): origin should be renamed in order to be consistent with
  // GURL definition of origin.
  GURL origin;

  // The action target of the form; like |origin| URL consists of the scheme,
  // host, port and path; the rest is stripped. This is the primary data used by
  // the PasswordManager for form autofill; that is, the action of the saved
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
  //
  // When parsing an HTML form, this must always be set.
  base::string16 submit_element;

  // True if renderer ids for username and password fields are present. Only set
  // on form parsing, and not persisted.
  // TODO(https://crbug.com/831123): Remove this field when old parsing is
  // removed and filling by renderer ids is by default.
  bool has_renderer_ids = false;

  // The name of the username input element. Optional (improves scoring).
  //
  // When parsing an HTML form, this must always be set.
  base::string16 username_element;

  // The renderer id of the username input element. It is set during the new
  // form parsing and not persisted.
  FieldRendererId username_element_renderer_id;

  // True if the server-side classification believes that the field may be
  // pre-filled with a placeholder in the value attribute. It is set during
  // form parsing and not persisted.
  bool username_may_use_prefilled_placeholder = false;

  // Whether the |username_element| has an autocomplete=username attribute. This
  // is only used in parsed HTML forms.
  bool username_marked_by_site = false;

  // The username. Optional.
  //
  // When parsing an HTML form, this is typically empty unless the site
  // has implemented some form of autofill.
  base::string16 username_value;

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
  base::string16 password_element;

  // The renderer id of the password input element. It is set during the new
  // form parsing and not persisted.
  FieldRendererId password_element_renderer_id;

  // The current password. Must be non-empty for PasswordForm instances that are
  // meant to be persisted to the password store.
  //
  // When parsing an HTML form, this is typically empty.
  base::string16 password_value;

  // The current encrypted password. Must be non-empty for PasswordForm
  // instances retrieved from the password store or coming in a
  // PasswordStoreChange that is not of type REMOVE.
  std::string encrypted_password;

  // If the form was a sign-up or a change password form, the name of the input
  // element corresponding to the new password. Optional, and not persisted.
  base::string16 new_password_element;

  // The renderer id of the new password input element. It is set during the new
  // form parsing and not persisted.
  FieldRendererId new_password_element_renderer_id;

  // The confirmation password element. Optional, only set on form parsing, and
  // not persisted.
  base::string16 confirmation_password_element;

  // The renderer id of the confirmation password input element. It is set
  // during the new form parsing and not persisted.
  FieldRendererId confirmation_password_element_renderer_id;

  // The new password. Optional, and not persisted.
  base::string16 new_password_value;

  // Whether the |new_password_element| has an autocomplete=new-password
  // attribute. This is only used in parsed HTML forms.
  bool new_password_marked_by_site = false;

  // When the login was last used by the user to login to the site. Defaults to
  // |date_created|, except for passwords that were migrated from the now
  // deprecated |preferred| flag. Their default is set when migrating the login
  // database to have the "date_last_used" column.
  //
  // When parsing an HTML form, this is not used.
  base::Time date_last_used;

  // When the login was saved (by chrome).
  //
  // When parsing an HTML form, this is not used.
  base::Time date_created;

  // When the login was downloaded from the sync server. For local passwords is
  // not used.
  //
  // When parsing an HTML form, this is not used.
  base::Time date_synced;

  // Tracks if the user opted to never remember passwords for this form. Default
  // to false.
  //
  // When parsing an HTML form, this is not used.
  bool blacklisted_by_user = false;

  // The form type.
  Type type = Type::kManual;

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
  FormData form_data;

  // What information has been sent to the Autofill server about this form.
  GenerationUploadStatus generation_upload_status =
      GenerationUploadStatus::kNoSignalSent;

  // These following fields are set by a website using the Credential Manager
  // API. They will be empty and remain unused for sites which do not use that
  // API.
  //
  // User friendly name to show in the UI.
  base::string16 display_name;

  // The URL of this credential's icon, such as the user's avatar, to display
  // in the UI.
  // TODO(msramek): This field was previously named |avatar_url|. It is still
  // named this way in the password store backends (e.g. the avatar_url column
  // in the SQL DB of LoginDatabase) and for the purposes of syncing
  // (i.e in PasswordSpecificsData). Rename these occurrences.
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

  // If true, this is a credential saved through an Android application, and
  // found using affiliation-based match.
  bool is_affiliation_based_match = false;

  // The type of the event that was taken as an indication that this form is
  // being or has already been submitted. This field is not persisted and filled
  // out only for submitted forms.
  mojom::SubmissionIndicatorEvent submission_event =
      mojom::SubmissionIndicatorEvent::NONE;

  // True iff heuristics declined this form for normal saving or filling (e.g.
  // only credit card fields were found). But this form can be saved or filled
  // only with the fallback.
  bool only_for_fallback = false;

  // True iff the new password field was found with server hints or autocomplete
  // attributes. Only set on form parsing for filling, and not persisted. Used
  // as signal for password generation eligibility.
  bool is_new_password_reliable = false;

  // Serialized to prefs, so don't change numeric values!
  enum class Store {
    // Default value.
    kNotSet = 0,
    // Credential came from the profile (i.e. local) storage.
    kProfileStore = 1,
    // Credential came from the Gaia-account-scoped storage.
    kAccountStore = 2
  };
  Store in_store = Store::kNotSet;

  // Vector of hashes of the gaia id for users who prefer not to move this
  // password form to their account. This list is used to suppress the move
  // prompt for those users.
  std::vector<GaiaIdHash> moving_blocked_for_list;

  // Return true if we consider this form to be a change password form.
  // We use only client heuristics, so it could include signup forms.
  bool IsPossibleChangePasswordForm() const;

  // Return true if we consider this form to be a change password form
  // without username field. We use only client heuristics, so it could
  // include signup forms.
  bool IsPossibleChangePasswordFormWithoutUsername() const;

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

  // Returns whether this form is stored in the account-scoped store, i.e.
  // whether |in_store == Store::kAccountStore|.
  bool IsUsingAccountStore() const;

  // Returns true when |password_value| or |new_password_value| are non-empty.
  bool HasNonEmptyPasswordValue() const;

  // Equality operators for testing.
  bool operator==(const PasswordForm& form) const;
  bool operator!=(const PasswordForm& form) const;

  PasswordForm();
  PasswordForm(const PasswordForm& other);
  PasswordForm(PasswordForm&& other);
  ~PasswordForm();

  PasswordForm& operator=(const PasswordForm& form);
  PasswordForm& operator=(PasswordForm&& form);
};

// True if the unique keys for the forms are the same. The unique key is
// (origin, username_element, username_value, password_element, signon_realm).
bool ArePasswordFormUniqueKeysEqual(const PasswordForm& left,
                                    const PasswordForm& right);

// Converts a vector of ValueElementPair to string.
base::string16 ValueElementVectorToString(
    const ValueElementVector& value_element_pairs);

// For testing.
std::ostream& operator<<(std::ostream& os, const PasswordForm& form);
std::ostream& operator<<(std::ostream& os, PasswordForm* form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_H__
