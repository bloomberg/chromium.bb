// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "components/url_formatter/spoof_checks/skeleton_generator.h"
#include "net/extras/preload_data/decoder.h"

#include "third_party/icu/source/common/unicode/uniset.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/common/unicode/uversion.h"

// 'icu' does not work. Use U_ICU_NAMESPACE.
namespace U_ICU_NAMESPACE {

class UnicodeString;

}  // namespace U_ICU_NAMESPACE

struct USpoofChecker;

namespace url_formatter {
FORWARD_DECLARE_TEST(UrlFormatterTest, IDNToUnicode);

using Skeletons = base::flat_set<std::string>;

// Represents a top domain entry in the trie.
struct TopDomainEntry {
  // The domain name.
  std::string domain;
  // True if the domain is in the top 500.
  bool is_top_500 = false;
};

// A helper class for IDN Spoof checking, used to ensure that no IDN input is
// spoofable per Chromium's standard of spoofability. For a more thorough
// explanation of how spoof checking works in Chromium, see
// http://dev.chromium.org/developers/design-documents/idn-in-google-chrome .
class IDNSpoofChecker {
 public:
  struct HuffmanTrieParams {
    const uint8_t* huffman_tree;
    size_t huffman_tree_size;
    const uint8_t* trie;
    size_t trie_bits;
    size_t trie_root_position;
  };

  IDNSpoofChecker();
  ~IDNSpoofChecker();

  // Returns true if |label| is safe to display as Unicode. In the event of
  // library failure, all IDN inputs will be treated as unsafe.
  // See the function body for details on the specific safety checks performed.
  // top_level_domain_unicode can be empty if top_level_domain is not well
  // formed punycode.
  bool SafeToDisplayAsUnicode(base::StringPiece16 label,
                              base::StringPiece top_level_domain,
                              base::StringPiece16 top_level_domain_unicode);

  // Returns the matching top domain if |hostname| or the last few components of
  // |hostname| looks similar to one of top domains listed in domains.list.
  // Returns empty result if |hostname| is a top domain itself, or is a
  // subdomain of a top domain.
  // Two checks are done:
  //   1. Calculate the skeleton of |hostname| based on the Unicode confusable
  //   character list and look it up in the pre-calculated skeleton list of
  //   top domains.
  //   2. Look up the diacritic-free version of |hostname| in the list of
  //   top domains. Note that non-IDN hostnames will not get here.
  TopDomainEntry GetSimilarTopDomain(base::StringPiece16 hostname);

  // Returns skeleton strings computed from |hostname|. This function can apply
  // extra mappings to some characters to produce multiple skeletons.
  Skeletons GetSkeletons(base::StringPiece16 hostname);

  // Returns a top domain from the top 10K list matching the given |skeleton|.
  TopDomainEntry LookupSkeletonInTopDomains(const std::string& skeleton);

  // Used for unit tests.
  static void SetTrieParamsForTesting(const HuffmanTrieParams& trie_params);
  static void RestoreTrieParamsForTesting();

 private:
  // Store information about various language scripts whose letters can be used
  // to make whole-script-confusable spoofs (e.g. ѕсоре[.]com where all letters
  // in ѕсоре are Cyrillic).
  struct WholeScriptConfusable {
    WholeScriptConfusable(
        std::unique_ptr<icu::UnicodeSet> arg_all_letters,
        std::unique_ptr<icu::UnicodeSet> arg_latin_lookalike_letters,
        const std::vector<std::string>& allowed_tlds);
    ~WholeScriptConfusable();

    // Captures all letters belonging to this script. See kScriptNameCodeList in
    // blink/renderer/platform/text/locale_to_script_mapping.cc for script
    // codes.
    std::unique_ptr<icu::UnicodeSet> all_letters;
    // The subset of all_letters that look like Latin ASCII letters. A domain
    // label entirely made of them is blocked as a simplified
    // whole-script-spoofable, unless the TLD of the domain is explicitly
    // allowed by |allowed_tlds|.
    std::unique_ptr<icu::UnicodeSet> latin_lookalike_letters;
    // List of top level domains where whole-script-confusable domains are
    // allowed for this script.
    const std::vector<std::string> allowed_tlds;
  };

  // Returns true if all the letters belonging to |script| in |label| also
  // belong to a set of Latin lookalike letters for that script.
  static bool IsLabelWholeScriptConfusableForScript(
      const WholeScriptConfusable& script,
      const icu::UnicodeString& label);
  // Returns true if |tld| is a top level domain most likely to contain a large
  // number of domains in |script| (as in, written script). |tld_unicode| can be
  // empty if |tld| is not well formed punycode.
  static bool IsWholeScriptConfusableAllowedForTLD(
      const WholeScriptConfusable& wsc,
      base::StringPiece tld,
      base::StringPiece16 tld_unicode);

  // Sets allowed characters in IDN labels and turns on USPOOF_CHAR_LIMIT.
  void SetAllowedUnicodeSet(UErrorCode* status);

  // Returns true if the string is entirely made up of either digits or
  // characters that look like digits (but not exclusively actual digits).
  bool IsDigitLookalike(const icu::UnicodeString& label);

  USpoofChecker* checker_;
  icu::UnicodeSet deviation_characters_;
  icu::UnicodeSet non_ascii_latin_letters_;
  icu::UnicodeSet kana_letters_exceptions_;
  icu::UnicodeSet combining_diacritics_exceptions_;
  icu::UnicodeSet digits_;
  icu::UnicodeSet digit_lookalikes_;
  icu::UnicodeSet lgc_letters_n_ascii_;
  icu::UnicodeSet icelandic_characters_;

  std::unique_ptr<SkeletonGenerator> skeleton_generator_;

  // List of scripts containing whole-script-confusable information.
  std::vector<std::unique_ptr<WholeScriptConfusable>> wholescriptconfusables_;

  IDNSpoofChecker(const IDNSpoofChecker&) = delete;
  void operator=(const IDNSpoofChecker&) = delete;
};

}  // namespace url_formatter

#endif  // COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_H_
