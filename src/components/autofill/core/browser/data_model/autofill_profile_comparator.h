// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_PROFILE_COMPARATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_PROFILE_COMPARATOR_H_

#include <memory>
#include <set>

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/data_model/address.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/contact_info.h"
#include "components/autofill/core/common/autofill_l10n_util.h"

namespace autofill {

// A utility class to assist in the comparison of AutofillProfile data.
class AutofillProfileComparator {
 public:
  explicit AutofillProfileComparator(const base::StringPiece& app_locale);
  ~AutofillProfileComparator();

  enum WhitespaceSpec { RETAIN_WHITESPACE, DISCARD_WHITESPACE };

  // Returns true if |text1| matches |text2|. The following normalization
  // techniques are applied to the given texts before comparing.
  //
  // (1) Diacritics are removed, e.g. حَ to ح, ビ to ヒ, and é to e;
  // (2) Leading and trailing whitespace and punctuation are ignored; and
  // (3) Characters are converted to lowercase.
  //
  // If |whitespace_spec| is DISCARD_WHITESPACE, then punctuation and whitespace
  // are discarded. For example, the postal codes "B15 3TR" and "B153TR" and
  // street addresses "16 Bridge St."" and "16 Bridge St" are considered equal.
  //
  // If |whitespace_spec| is RETAIN_WHITESPACE, then the postal codes "B15 3TR"
  // and "B153TR" are not considered equal, but "16 Bridge St."" and "16 Bridge
  // St" are because trailing whitespace and punctuation are ignored.
  bool Compare(base::StringPiece16 text1,
               base::StringPiece16 text2,
               WhitespaceSpec whitespace_spec = DISCARD_WHITESPACE) const;

  // Returns true if |text| is empty or contains only skippable characters. A
  // character is skippable if it is punctuation or white space.
  bool HasOnlySkippableCharacters(base::StringPiece16 text) const;

  // Returns a copy of |text| with uppercase converted to lowercase and
  // diacritics removed.
  //
  // If |whitespace_spec| is RETAIN_WHITESPACE, punctuation is converted to
  // spaces, and extraneous whitespace is trimmed and collapsed. For example,
  // "Jean- François" becomes "jean francois".
  //
  // If |whitespace_spec| is DISCARD_WHITESPACE, punctuation and whitespace are
  // discarded. For example, +1 (234) 567-8900 becomes 12345678900.
  base::string16 NormalizeForComparison(
      base::StringPiece16 text,
      WhitespaceSpec whitespace_spec = RETAIN_WHITESPACE) const;

  // Returns true if |p1| and |p2| are viable merge candidates. This means that
  // their names, addresses, email addreses, company names, and phone numbers
  // are all pairwise equivalent or mergeable.
  //
  // Note that mergeability is non-directional; merging two profiles will likely
  // incorporate data from both profiles.
  bool AreMergeable(const AutofillProfile& p1, const AutofillProfile& p2) const;

  // Populates |name_info| with the result of merging the names in |p1| and
  // |p2|. Returns true if successful. Expects that |p1| and |p2| have already
  // been found to be mergeable.
  //
  // Heuristic: If one name is empty, select the other; othwerwise, attempt to
  // parse the names in each profile and determine if one name can be derived
  // from the other. For example, J Smith can be derived from John Smith, so
  // prefer the latter.
  bool MergeNames(const AutofillProfile& p1,
                  const AutofillProfile& p2,
                  NameInfo* name_info) const;

  // Returns true if |full_name_2| is a variant of |full_name_1|.
  //
  // This function generates all variations of |full_name_1| and returns true if
  // one of these variants is equal to |full_name_2|. For example, this function
  // will return true if |full_name_2| is "john q public" and |full_name_1| is
  // "john quincy public" because |full_name_2| can be derived from
  // |full_name_1| by using the middle initial. Note that the reverse is not
  // true, "john quincy public" is not a name variant of "john q public".
  //
  // Note: Expects that |full_name| is already normalized for comparison.
  bool IsNameVariantOf(const base::string16& full_name_1,
                       const base::string16& full_name_2) const;

  // Populates |email_info| with the result of merging the email addresses in
  // |p1| and |p2|. Returns true if successful. Expects that |p1| and |p2| have
  // already been found to be mergeable.
  //
  // Heuristic: If one email address is empty, use the other; otherwise, prefer
  // the most recently used version of the email address.
  bool MergeEmailAddresses(const AutofillProfile& p1,
                           const AutofillProfile& p2,
                           EmailInfo* email_info) const;

  // Populates |company_info| with the result of merging the company names in
  // |p1| and |p2|. Returns true if successful. Expects that |p1| and |p2| have
  // already been found to be mergeable.
  //
  // Heuristic: If one is empty, use the other; otherwise, if the tokens in one
  // company name are a superset of those in the other, prefer the former; and,
  // as a tiebreaker, prefer the most recently used version of the company name.
  bool MergeCompanyNames(const AutofillProfile& p1,
                         const AutofillProfile& p2,
                         CompanyInfo* company_info) const;

  // Populates |phone_number| with the result of merging the phone numbers in
  // |p1| and |p2|. Returns true if successful. Expects that |p1| and |p2| have
  // already been found to be mergeable.
  //
  // Heuristic: Populate the missing parts of each number from the other.
  bool MergePhoneNumbers(const AutofillProfile& p1,
                         const AutofillProfile& p2,
                         PhoneNumber* phone_number) const;

  // Populates |address| with the result of merging the addresses in |p1| and
  // |p2|. Returns true if successful. Expects that |p1| and |p2| have already
  // been found to be mergeable.
  //
  // Heuristic: Populate the missing parts of each address from the other.
  // Prefer the abbreviated state, the shorter zip code and routing code, the
  // more verbost city, dependent locality, and address.
  bool MergeAddresses(const AutofillProfile& p1,
                      const AutofillProfile& p2,
                      Address* address) const;

  // App locale used when this comparator instance was created.
  const std::string app_locale() const { return app_locale_; }

  // Merges |new_profile| into one of the |existing_profiles| if possible;
  // otherwise appends |new_profile| to the end of that list. Fills
  // |merged_profiles| with the result. Returns the |guid| of the new or updated
  // profile.
  static std::string MergeProfile(
      const AutofillProfile& new_profile,
      const std::vector<std::unique_ptr<AutofillProfile>>& existing_profiles,
      const std::string& app_locale,
      std::vector<AutofillProfile>* merged_profiles);

 protected:
  // The result type returned by CompareTokens.
  enum CompareTokensResult {
    DIFFERENT_TOKENS,
    SAME_TOKENS,
    S1_CONTAINS_S2,
    S2_CONTAINS_S1,
  };

  // Returns the set of unique tokens in |s|. Note that the string data backing
  // |s| is expected to have a lifetime which exceeds the call to UniqueTokens.
  static std::set<base::StringPiece16> UniqueTokens(base::StringPiece16 s);

  // Compares the unique tokens in s1 and s2.
  static CompareTokensResult CompareTokens(base::StringPiece16 s1,
                                           base::StringPiece16 s2);

  // Returns the value of |t| from |p1| or |p2| depending on which is non-empty.
  // This method expects that the value is either the same in |p1| and |p2| or
  // empty in one of them.
  base::string16 GetNonEmptyOf(const AutofillProfile& p1,
                               const AutofillProfile& p2,
                               AutofillType t) const;

  // Generate the set of full/initial variants for |name_part|, where
  // |name_part| is the user's first or middle name. For example, given "jean
  // francois" (the normalized for comparison form of "Jean-François") this
  // function returns the set:
  //
  //   { "", "f", "francois,
  //     "j", "j f", "j francois",
  //     "jean", "jean f", "jean francois", "jf" }
  //
  // Note: Expects that |name| is already normalized for comparison.
  static std::set<base::string16> GetNamePartVariants(
      const base::string16& name_part);

  // Returns true if |p1| and |p2| have names which are equivalent for the
  // purposes of merging the two profiles. This means one of the names is
  // empty, the names are the same, or one name is a variation of the other.
  // The name comparison is insensitive to case, punctuation and diacritics.
  //
  // Note that this method does not provide any guidance on actually merging
  // the names.
  bool HaveMergeableNames(const AutofillProfile& p1,
                          const AutofillProfile& p2) const;

  // Returns true if |p1| and |p2| have email addresses which are equivalent for
  // the purposes of merging the two profiles. This means one of the email
  // addresses is empty, or the email addresses are the same (modulo case).
  //
  // Note that this method does not provide any guidance on actually merging
  // the email addresses.
  bool HaveMergeableEmailAddresses(const AutofillProfile& p1,
                                   const AutofillProfile& p2) const;

  // Returns true if |p1| and |p2| have company names which are equivalent for
  // the purposes of merging the two profiles. This means one of the company
  // names is empty, or the normalized company names are the same (modulo case).
  //
  // Note that this method does not provide any guidance on actually merging
  // the company names.
  bool HaveMergeableCompanyNames(const AutofillProfile& p1,
                                 const AutofillProfile& p2) const;

  // Returns true if |p1| and |p2| have phone numbers which are equivalent for
  // the purposes of merging the two profiles. This means one of the phone
  // numbers is empty, or the phone numbers match modulo formatting
  // differences or missing information. For example, if the phone numbers are
  // the same but one has an extension, country code, or area code and the other
  // does not.
  //
  // Note that this method does not provide any guidance on actually merging
  // the company names.
  bool HaveMergeablePhoneNumbers(const AutofillProfile& p1,
                                 const AutofillProfile& p2) const;

  // Returns true if |p1| and |p2| have addresses which are equivalent for the
  // purposes of merging the two profiles. This means one of the addresses is
  // empty, or the addresses are a match. A number of normalization and
  // comparison heuristics are employed to determine if the addresses match.
  //
  // Note that this method does not provide any guidance on actually merging
  // the email addresses.
  bool HaveMergeableAddresses(const AutofillProfile& p1,
                              const AutofillProfile& p2) const;

  // Populates |name_info| with the result of merging the Chinese, Japanese or
  // Korean names in |p1| and |p2|. Returns true if successful. Expects that
  // |p1| and |p2| have already been found to be mergeable, and have CJK names.
  bool MergeCJKNames(const AutofillProfile& p1,
                     const AutofillProfile& p2,
                     NameInfo* info) const;

 private:
  l10n::CaseInsensitiveCompare case_insensitive_compare_;
  const std::string app_locale_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileComparator);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_PROFILE_COMPARATOR_H_
