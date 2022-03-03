// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/titled_url_match_utils.h"

#include <numeric>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/titled_url_node.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/history_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/url_formatter/url_formatter.h"

namespace {

// Concatenates |ancestors| in reverse order and using '/' as the delimiter.
std::u16string ConcatAncestorsTitles(
    std::vector<base::StringPiece16> ancestors) {
  return ancestors.empty()
             ? std::u16string()
             : std::accumulate(std::next(ancestors.rbegin()), ancestors.rend(),
                               std::u16string(*ancestors.rbegin()),
                               [](std::u16string& a, base::StringPiece16& b) {
                                 return a + u"/" + std::u16string(b);
                               });
}

}  // namespace

namespace bookmarks {

AutocompleteMatch TitledUrlMatchToAutocompleteMatch(
    const TitledUrlMatch& titled_url_match,
    AutocompleteMatchType::Type type,
    int relevance,
    AutocompleteProvider* provider,
    const AutocompleteSchemeClassifier& scheme_classifier,
    const AutocompleteInput& input,
    const std::u16string& fixed_up_input_text) {
  const std::u16string title = titled_url_match.node->GetTitledUrlNodeTitle();
  const GURL& url = titled_url_match.node->GetTitledUrlNodeUrl();
  const std::u16string path = ConcatAncestorsTitles(
      titled_url_match.node->GetTitledUrlNodeAncestorTitles());

  // The AutocompleteMatch we construct is non-deletable because the only way to
  // support this would be to delete the underlying object that created the
  // titled_url_match. E.g., for the bookmark provider this would mean deleting
  // the underlying bookmark, which is unlikely to be what the user intends.
  AutocompleteMatch match(provider, relevance, false, type);
  match.destination_url = url;
  match.RecordAdditionalInfo("Title", title);
  match.RecordAdditionalInfo("URL", url.spec());
  match.RecordAdditionalInfo("Path", path);

  bool match_in_scheme = false;
  bool match_in_subdomain = false;
  AutocompleteMatch::GetMatchComponents(url,
                                        titled_url_match.url_match_positions,
                                        &match_in_scheme, &match_in_subdomain);
  auto format_types = AutocompleteMatch::GetFormatTypes(
      input.parts().scheme.len > 0 || match_in_scheme, match_in_subdomain);
  const std::u16string formatted_url = url_formatter::FormatUrl(
      url, format_types, net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);

  if (OmniboxFieldTrial::kBookmarkPathsUiReplaceUrl.Get()) {
    match.contents = path;
  } else if (OmniboxFieldTrial::kBookmarkPathsUiDynamicReplaceUrl.Get()) {
    match.contents = !titled_url_match.has_ancestor_match &&
                             !titled_url_match.url_match_positions.empty()
                         ? formatted_url
                         : path;
  } else {
    match.contents = formatted_url;
  }

  // Bookmark classification diverges from relevance scoring. Specifically,
  // 1) All occurrences of the input contribute to relevance; e.g. for the input
  // 'pre', the bookmark 'pre prefix' will be scored higher than 'pre suffix'.
  // For classification though, if the input is a prefix of the suggestion text,
  // only the prefix will be bolded; e.g. the 1st bookmark will display '[pre]
  // prefix' as opposed to '[pre] [pre]fix'. This divergence allows consistency
  // with other providers' and google.com's bolding.
  // 2) Non-complete-word matches less than 3 characters long do not contribute
  // to relevance; e.g. for the input 'a pr', the bookmark 'a pr prefix' will be
  // scored the same as 'a pr suffix'. For classification though, both
  // occurrences will be bolded, 'a [pr] [pr]efix'.
  auto contents_terms = FindTermMatches(input.text(), match.contents);
  match.contents_class = ClassifyTermMatches(
      contents_terms, match.contents.length(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);

  if (OmniboxFieldTrial::kBookmarkPathsUiReplaceTitle.Get()) {
    match.description = path + u"/" + title;
  } else if (OmniboxFieldTrial::kBookmarkPathsUiAppendAfterTitle.Get()) {
    match.description = title + u" : " + path;
  } else {
    match.description = title;
  }

  base::TrimWhitespace(match.description, base::TRIM_LEADING,
                       &match.description);
  auto description_terms = FindTermMatches(input.text(), match.description);
  match.description_class = ClassifyTermMatches(
      description_terms, match.description.length(),
      ACMatchClassification::MATCH, ACMatchClassification::NONE);

  // The inline_autocomplete_offset should be adjusted based on the formatting
  // applied to |fill_into_edit|.
  size_t inline_autocomplete_offset = URLPrefix::GetInlineAutocompleteOffset(
      input.text(), fixed_up_input_text, false, base::UTF8ToUTF16(url.spec()));
  auto fill_into_edit_format_types = url_formatter::kFormatUrlOmitDefaults;
  if (match_in_scheme)
    fill_into_edit_format_types &= ~url_formatter::kFormatUrlOmitHTTP;
  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url,
          url_formatter::FormatUrl(url, fill_into_edit_format_types,
                                   net::UnescapeRule::SPACES, nullptr, nullptr,
                                   &inline_autocomplete_offset),
          scheme_classifier, &inline_autocomplete_offset);

  if (match.TryRichAutocompletion(match.contents, match.description, input)) {
    // If rich autocompletion applies, we skip trying the alternatives below.
  } else if (inline_autocomplete_offset != std::u16string::npos) {
    match.inline_autocompletion =
        match.fill_into_edit.substr(inline_autocomplete_offset);
    match.SetAllowedToBeDefault(input);
  }

  return match;
}

}  // namespace bookmarks
