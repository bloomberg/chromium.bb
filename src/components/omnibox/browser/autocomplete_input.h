// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_INPUT_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_INPUT_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

class AutocompleteSchemeClassifier;

// The user input for an autocomplete query.  Allows copying.
class AutocompleteInput {
 public:
  AutocompleteInput();
  // |text| represents the input query.
  //
  // |current_page_classification| represents the type of page the user is
  // viewing and manner in which the user is accessing the omnibox; it's
  // more than simply the URL.  It includes, for example, whether the page
  // is a search result page doing search term replacement or not.
  //
  // |scheme_classifier| is passed to Parse() to help determine the type of
  // input this is; see comments there.
  AutocompleteInput(const base::string16& text,
                    metrics::OmniboxEventProto::PageClassification
                        current_page_classification,
                    const AutocompleteSchemeClassifier& scheme_classifier);
  // This constructor adds |cursor_position|, related to |text|.
  // |cursor_position| represents the location of the cursor within the
  // query |text|. It may be set to base::string16::npos if the input
  // doesn't come directly from the user's typing.
  AutocompleteInput(const base::string16& text,
                    size_t cursor_position,
                    metrics::OmniboxEventProto::PageClassification
                        current_page_classification,
                    const AutocompleteSchemeClassifier& scheme_classifier);
  // This constructor adds |desired_tld|, related to |text|. |desired_tld|
  // is the user's desired TLD, if one is not already present in the text to
  // autocomplete. When this is non-empty, it also implies that "www."
  // should be prepended to the domain where possible. The |desired_tld|
  // should not contain a leading '.' (use "com" instead of ".com").
  AutocompleteInput(const base::string16& text,
                    size_t cursor_position,
                    const std::string& desired_tld,
                    metrics::OmniboxEventProto::PageClassification
                        current_page_classification,
                    const AutocompleteSchemeClassifier& scheme_classifier);
  AutocompleteInput(const AutocompleteInput& other);
  ~AutocompleteInput();

  // Converts |type| to a string representation.  Used in logging.
  static std::string TypeToString(metrics::OmniboxInputType type);

  // Parses |text| (including an optional |desired_tld|) and returns the type of
  // input this will be interpreted as.  |scheme_classifier| is used to check
  // the scheme in |text| is known and registered in the current environment.
  // The components of the input are stored in the output parameter |parts|, if
  // it is non-NULL. The scheme is stored in |scheme| if it is non-NULL. The
  // canonicalized URL is stored in |canonicalized_url|; however, this URL is
  // not guaranteed to be valid, especially if the parsed type is, e.g., QUERY.
  static metrics::OmniboxInputType Parse(
      const base::string16& text,
      const std::string& desired_tld,
      const AutocompleteSchemeClassifier& scheme_classifier,
      url::Parsed* parts,
      base::string16* scheme,
      GURL* canonicalized_url);

  // Parses |text| and fill |scheme| and |host| by the positions of them.
  // The results are almost as same as the result of Parse(), but if the scheme
  // is view-source, this function returns the positions of scheme and host
  // in the URL qualified by "view-source:" prefix.
  static void ParseForEmphasizeComponents(
      const base::string16& text,
      const AutocompleteSchemeClassifier& scheme_classifier,
      url::Component* scheme,
      url::Component* host);

  // Code that wants to format URLs with a format flag including
  // net::kFormatUrlOmitTrailingSlashOnBareHostname risk changing the meaning if
  // the result is then parsed as AutocompleteInput.  Such code can call this
  // function with the URL and its formatted string, and it will return a
  // formatted string with the same meaning as the original URL (i.e. it will
  // re-append a slash if necessary).  Because this uses Parse() under the hood
  // to determine the meaning of the different strings, callers need to supply a
  // |scheme_classifier| to pass to Parse(). If |offset| is non-null, it will
  // be updated with any changes that shift it.
  static base::string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const base::string16& formatted_url,
      const AutocompleteSchemeClassifier& scheme_classifier,
      size_t* offset);

  // Returns the number of non-empty components in |parts| besides the host.
  static int NumNonHostComponents(const url::Parsed& parts);

  // Returns whether |text| begins "http:" or "view-source:http:".
  static bool HasHTTPScheme(const base::string16& text);

  // User-provided text to be completed.
  const base::string16& text() const { return text_; }

  // Returns 0-based cursor position within |text_| or base::string16::npos if
  // not used.
  size_t cursor_position() const { return cursor_position_; }

  // Use of this setter is risky, since no other internal state is updated
  // besides |text_|, |cursor_position_| and |parts_|.  Only callers who know
  // that they're not changing the type/scheme/etc. should use this.
  void UpdateText(const base::string16& text,
                  size_t cursor_position,
                  const url::Parsed& parts);

  // The current URL, or an invalid GURL if not applicable or available.
  const GURL& current_url() const { return current_url_; }
  // Providers that trigger on focus need the current URL to produce a match
  // that, when displayed, contain the URL of the current page.
  void set_current_url(const GURL& current_url) { current_url_ = current_url; }

  // The title of the current page, corresponding to the current URL, or empty
  // if this is not available.
  const base::string16& current_title() const { return current_title_; }
  // This is sometimes set as the description if returning a
  // URL-what-you-typed match for the current URL.
  void set_current_title(const base::string16& title) {
    current_title_ = title;
  }

  // The type of page that is currently behind displayed and how it is
  // displayed (e.g., with search term replacement or without).
  metrics::OmniboxEventProto::PageClassification current_page_classification()
      const {
    return current_page_classification_;
  }

  // The type of input supplied.
  metrics::OmniboxInputType type() const { return type_; }

  // Returns parsed URL components.
  const url::Parsed& parts() const { return parts_; }

  // The scheme parsed from the provided text; only meaningful when type_ is
  // URL.
  const base::string16& scheme() const { return scheme_; }

  // The input as an URL to navigate to, if possible.
  const GURL& canonicalized_url() const { return canonicalized_url_; }

  // The user's desired TLD.
  const std::string& desired_tld() const { return desired_tld_; }

  // Returns whether inline autocompletion should be prevented.
  bool prevent_inline_autocomplete() const {
    return prevent_inline_autocomplete_;
  }
  // |prevent_inline_autocomplete| is true if the generated result set should
  // not require inline autocomplete for the default match. This is difficult
  // to explain in the abstract; the practical use case is that after the user
  // deletes text in the edit, the HistoryURLProvider should make sure not to
  // promote a match requiring inline autocomplete too highly.
  void set_prevent_inline_autocomplete(bool prevent_inline_autocomplete) {
    prevent_inline_autocomplete_ = prevent_inline_autocomplete;
  }

  // Returns whether, given an input string consisting solely of a substituting
  // keyword, we should score it like a non-substituting keyword.
  bool prefer_keyword() const { return prefer_keyword_; }
  // |prefer_keyword| should be true when the keyword UI is onscreen; this
  // will bias the autocomplete result set toward the keyword provider when
  // the input string is a bare keyword.
  void set_prefer_keyword(bool prefer_keyword) {
    prefer_keyword_ = prefer_keyword;
  }

  // Returns whether this input is allowed to be treated as an exact
  // keyword match.  If not, the default result is guaranteed not to be a
  // keyword search, even if the input is "<keyword> <search string>".
  bool allow_exact_keyword_match() const { return allow_exact_keyword_match_; }
  // |allow_exact_keyword_match| should be false when triggering keyword
  // mode on the input string would be surprising or wrong, e.g.  when
  // highlighting text in a page and telling the browser to search for it or
  // navigate to it. This member only applies to substituting keywords.
  void set_allow_exact_keyword_match(bool allow_exact_keyword_match) {
    allow_exact_keyword_match_ = allow_exact_keyword_match;
  }

  // Provides public read-only access to the method that the user used to
  // get into keyword mode (which includes INVALID if they didn't enter it.)
  metrics::OmniboxEventProto::KeywordModeEntryMethod keyword_mode_entry_method()
      const {
    return keyword_mode_entry_method_;
  }

  // Used by code handling keyword entry to set the method by which the user
  // used to enter it.
  void set_keyword_mode_entry_method(
      metrics::OmniboxEventProto::KeywordModeEntryMethod entry_method) {
    keyword_mode_entry_method_ = entry_method;
  }

  // Returns whether providers should be allowed to make asynchronous requests
  // when processing this input.
  bool want_asynchronous_matches() const { return want_asynchronous_matches_; }
  // If |want_asynchronous_matches| is false, the controller asks the
  // providers to only return matches which are synchronously available,
  // which should mean that all providers will be done immediately.
  void set_want_asynchronous_matches(bool want_asynchronous_matches) {
    want_asynchronous_matches_ = want_asynchronous_matches;
  }

  // Returns whether this input query was triggered due to the omnibox being
  // focused.
  bool from_omnibox_focus() const { return from_omnibox_focus_; }
  // |from_omnibox_focus| should be true when input is created as a result
  // of the omnibox being focused, instead of due to user input changes.
  // Most providers should not provide matches in this case.  Providers
  // which want to display matches on focus can use this flag to know when
  // they can do so.
  void set_from_omnibox_focus(bool from_omnibox_focus) {
    from_omnibox_focus_ = from_omnibox_focus;
  }

  // Returns the terms in |text_| that start with http:// or https:// plus
  // at least one more character, stored without the scheme.  Used in
  // duplicate elimination to detect whether, for a given URL, the user may
  // have started typing that URL with an explicit scheme; see comments on
  // AutocompleteMatch::GURLToStrippedGURL().
  const std::vector<base::string16>& terms_prefixed_by_http_or_https() const {
    return terms_prefixed_by_http_or_https_;
  }

  // Returns the ID of the query tile selected by the user, if any.
  // If no tile was selected, returns base::nullopt.
  base::Optional<std::string> query_tile_id() const { return query_tile_id_; }

  // Called to indicate that the query tile represented by |tile_id| was
  // clicked by the user. In the absence of a |query_tile_id_|, top level tiles
  // will be displayed.
  void set_query_tile_id(const std::string& tile_id) {
    query_tile_id_ = tile_id;
  }

  // Resets all internal variables to the null-constructed state.
  void Clear();

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

 private:
  friend class AutocompleteProviderTest;

  // The common initialization of the non-default constructors, called after
  // the initial fields are set. These remaining parameters are used as inputs
  // to setting the remaining fields.
  void Init(const base::string16& text,
            const AutocompleteSchemeClassifier& scheme_classifier);

  // NOTE: Whenever adding a new field here, please make sure to update Clear()
  // and EstimateMemoryUsage() methods.
  base::string16 text_;
  size_t cursor_position_;
  GURL current_url_;
  base::string16 current_title_;
  metrics::OmniboxEventProto::PageClassification current_page_classification_;
  metrics::OmniboxInputType type_;
  url::Parsed parts_;
  base::string16 scheme_;
  GURL canonicalized_url_;
  std::string desired_tld_;
  bool prevent_inline_autocomplete_;
  bool prefer_keyword_;
  bool allow_exact_keyword_match_;
  metrics::OmniboxEventProto::KeywordModeEntryMethod keyword_mode_entry_method_;
  bool want_asynchronous_matches_;
  bool from_omnibox_focus_;
  std::vector<base::string16> terms_prefixed_by_http_or_https_;
  base::Optional<std::string> query_tile_id_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_INPUT_H_
