// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/search_suggestion_parser.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_i18n.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/url_formatter/url_fixer.h"
#include "components/url_formatter/url_formatter.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/device_form_factor.h"
#include "url/url_constants.h"

namespace {

AutocompleteMatchType::Type GetAutocompleteMatchType(const std::string& type) {
  if (type == "CALCULATOR")
    return AutocompleteMatchType::CALCULATOR;
  if (type == "ENTITY")
    return AutocompleteMatchType::SEARCH_SUGGEST_ENTITY;
  if (type == "TAIL")
    return AutocompleteMatchType::SEARCH_SUGGEST_TAIL;
  if (type == "PERSONALIZED_QUERY")
    return AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED;
  if (type == "PROFILE")
    return AutocompleteMatchType::SEARCH_SUGGEST_PROFILE;
  if (type == "NAVIGATION")
    return AutocompleteMatchType::NAVSUGGEST;
  if (type == "PERSONALIZED_NAVIGATION")
    return AutocompleteMatchType::NAVSUGGEST_PERSONALIZED;
  return AutocompleteMatchType::SEARCH_SUGGEST;
}

// Convert the supplied Json::Value representation of list-of-lists-of-integers
// to a vector-of-vecrors-of-integers, containing (ideally) one vector of
// integers per match.
// The logic here does not validate if the length of top level vector is same as
// number of returned matches and will supply empty vector for any item that is
// either invalid or missing.
// The function will always return a valid and properly sized vector of vectors,
// equal in length to |expected_size|, even if the input |subtypes_value| is not
// valid.
std::vector<std::vector<int>> ParseMatchSubtypes(
    const base::Value* subtypes_value,
    size_t expected_size) {
  std::vector<std::vector<int>> result(expected_size);

  if (subtypes_value == nullptr || !subtypes_value->is_list())
    return result;
  const auto& subtypes_list = subtypes_value->GetList();

  if (!subtypes_list.empty() && subtypes_list.size() != expected_size) {
    LOG(WARNING) << "The length of reported subtypes (" << subtypes_list.size()
                 << ") does not match the expected length (" << expected_size
                 << ')';
  }

  const auto num_items = std::min(expected_size, subtypes_list.size());
  for (auto index = 0u; index < num_items; index++) {
    const auto& subtypes_item = subtypes_list[index];
    // Permissive: ignore subtypes that are not in a form of a list.
    if (!subtypes_item.is_list())
      continue;

    const auto& subtype_list = subtypes_item.GetList();
    auto& result_subtypes = result[index];
    result_subtypes.reserve(subtype_list.size());

    for (const auto& subtype : subtype_list) {
      // Permissive: Skip over any item that is not an integer.
      if (!subtype.is_int())
        continue;
      result_subtypes.emplace_back(subtype.GetInt());
    }
  }

  return result;
}

}  // namespace

// Value chosen based on SuggestionGroupIds::INVALID in suggestion_config.proto.
const int SearchSuggestionParser::kNoSuggestionGroupId = -1;

// SearchSuggestionParser::Result ----------------------------------------------

SearchSuggestionParser::Result::Result(bool from_keyword,
                                       int relevance,
                                       bool relevance_from_server,
                                       AutocompleteMatchType::Type type,
                                       std::vector<int> subtypes,
                                       const std::string& deletion_url)
    : from_keyword_(from_keyword),
      type_(type),
      subtypes_(std::move(subtypes)),
      relevance_(relevance),
      relevance_from_server_(relevance_from_server),
      received_after_last_keystroke_(true),
      deletion_url_(deletion_url) {}

SearchSuggestionParser::Result::Result(const Result& other) = default;

SearchSuggestionParser::Result::~Result() {}

// SearchSuggestionParser::SuggestResult ---------------------------------------

SearchSuggestionParser::SuggestResult::SuggestResult(
    const std::u16string& suggestion,
    AutocompleteMatchType::Type type,
    std::vector<int> subtypes,
    bool from_keyword,
    int relevance,
    bool relevance_from_server,
    const std::u16string& input_text)
    : SuggestResult(suggestion,
                    type,
                    std::move(subtypes),
                    suggestion,
                    /*match_contents_prefix=*/std::u16string(),
                    /*annotation=*/std::u16string(),
                    /*suggest_query_params=*/"",
                    /*deletion_url=*/"",
                    /*image_dominant_color=*/"",
                    /*image_url=*/"",
                    from_keyword,
                    relevance,
                    relevance_from_server,
                    /*should_prefetch=*/false,
                    input_text) {}

SearchSuggestionParser::SuggestResult::SuggestResult(
    const std::u16string& suggestion,
    AutocompleteMatchType::Type type,
    std::vector<int> subtypes,
    const std::u16string& match_contents,
    const std::u16string& match_contents_prefix,
    const std::u16string& annotation,
    const std::string& additional_query_params,
    const std::string& deletion_url,
    const std::string& image_dominant_color,
    const std::string& image_url,
    bool from_keyword,
    int relevance,
    bool relevance_from_server,
    bool should_prefetch,
    const std::u16string& input_text)
    : Result(from_keyword,
             relevance,
             relevance_from_server,
             type,
             std::move(subtypes),
             deletion_url),
      suggestion_(suggestion),
      match_contents_prefix_(match_contents_prefix),
      annotation_(annotation),
      additional_query_params_(additional_query_params),
      image_dominant_color_(image_dominant_color),
      image_url_(GURL(image_url)),
      should_prefetch_(should_prefetch) {
  match_contents_ = match_contents;
  DCHECK(!match_contents_.empty());
  ClassifyMatchContents(true, input_text);
}

SearchSuggestionParser::SuggestResult::SuggestResult(
    const SuggestResult& result) = default;

SearchSuggestionParser::SuggestResult::~SuggestResult() {}

SearchSuggestionParser::SuggestResult& SearchSuggestionParser::SuggestResult::
operator=(const SuggestResult& rhs) = default;

void SearchSuggestionParser::SuggestResult::ClassifyMatchContents(
    const bool allow_bolding_all,
    const std::u16string& input_text) {
  DCHECK(!match_contents_.empty());

  // In case of zero-suggest results, do not highlight matches.
  if (input_text.empty()) {
    match_contents_class_ = {
        ACMatchClassification(0, ACMatchClassification::NONE)};
    return;
  }

  std::u16string lookup_text = input_text;
  if (type_ == AutocompleteMatchType::SEARCH_SUGGEST_TAIL) {
    const size_t contents_index =
        suggestion_.length() - match_contents_.length();
    // Ensure the query starts with the input text, and ends with the match
    // contents, and the input text has an overlap with contents.
    if (base::StartsWith(suggestion_, input_text,
                         base::CompareCase::SENSITIVE) &&
        base::EndsWith(suggestion_, match_contents_,
                       base::CompareCase::SENSITIVE) &&
        (input_text.length() > contents_index)) {
      lookup_text = input_text.substr(contents_index);
    }
  }
  // Do a case-insensitive search for |lookup_text|.
  std::u16string::const_iterator lookup_position = std::search(
      match_contents_.begin(), match_contents_.end(), lookup_text.begin(),
      lookup_text.end(), SimpleCaseInsensitiveCompareUCS2());
  if (!allow_bolding_all && (lookup_position == match_contents_.end())) {
    // Bail if the code below to update the bolding would bold the whole
    // string.  Note that the string may already be entirely bolded; if
    // so, leave it as is.
    return;
  }

  // Note we discard our existing match_contents_class_ with this call.
  match_contents_class_ = AutocompleteProvider::ClassifyAllMatchesInString(
      input_text, match_contents_, true);
}

void SearchSuggestionParser::SuggestResult::SetAnswer(
    const SuggestionAnswer& answer) {
  answer_ = answer;
}

int SearchSuggestionParser::SuggestResult::CalculateRelevance(
    const AutocompleteInput& input,
    bool keyword_provider_requested) const {
  if (!from_keyword_ && keyword_provider_requested)
    return 100;
  return ((input.type() == metrics::OmniboxInputType::URL) ? 300 : 600);
}

// SearchSuggestionParser::NavigationResult ------------------------------------

SearchSuggestionParser::NavigationResult::NavigationResult(
    const AutocompleteSchemeClassifier& scheme_classifier,
    const GURL& url,
    AutocompleteMatchType::Type match_type,
    std::vector<int> subtypes,
    const std::u16string& description,
    const std::string& deletion_url,
    bool from_keyword,
    int relevance,
    bool relevance_from_server,
    const std::u16string& input_text)
    : Result(from_keyword,
             relevance,
             relevance_from_server,
             match_type,
             std::move(subtypes),
             deletion_url),
      url_(url),
      formatted_url_(AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url,
          url_formatter::FormatUrl(url,
                                   url_formatter::kFormatUrlOmitDefaults &
                                       ~url_formatter::kFormatUrlOmitHTTP,
                                   net::UnescapeRule::SPACES,
                                   nullptr,
                                   nullptr,
                                   nullptr),
          scheme_classifier,
          nullptr)),
      description_(description) {
  DCHECK(url_.is_valid());
  CalculateAndClassifyMatchContents(true, input_text);
  ClassifyDescription(input_text);
}

SearchSuggestionParser::NavigationResult::NavigationResult(
    const NavigationResult& other) = default;

SearchSuggestionParser::NavigationResult::~NavigationResult() {}

void SearchSuggestionParser::NavigationResult::
    CalculateAndClassifyMatchContents(const bool allow_bolding_nothing,
                                      const std::u16string& input_text) {
  // Start with the trivial nothing-bolded classification.
  DCHECK(url_.is_valid());

  // In case of zero-suggest results, do not highlight matches.
  if (input_text.empty()) {
    // TODO(tommycli): Maybe this should actually return
    // ACMatchClassification::URL. I'm not changing this now because this CL
    // is meant to fix a regression only, but we should consider this for
    // consistency with other |input_text| that matches nothing.
    match_contents_class_ = {
        ACMatchClassification(0, ACMatchClassification::NONE)};
    return;
  }

  // Set contents to the formatted URL while ensuring the scheme and subdomain
  // are kept if the user text seems to include them. E.g., for the user text
  // 'http google.com', the contents should not trim 'http'.
  bool match_in_scheme = false;
  bool match_in_subdomain = false;
  TermMatches term_matches_in_url = FindTermMatches(input_text, formatted_url_);
  // Convert TermMatches (offset, length) to MatchPosition (start, end).
  std::vector<AutocompleteMatch::MatchPosition> match_positions;
  for (auto match : term_matches_in_url)
    match_positions.emplace_back(match.offset, match.offset + match.length);
  AutocompleteMatch::GetMatchComponents(GURL(formatted_url_), match_positions,
                                        &match_in_scheme, &match_in_subdomain);
  auto format_types = AutocompleteMatch::GetFormatTypes(
      GURL(input_text).has_scheme(), match_in_subdomain);

  // Find matches in the potentially new match_contents
  std::u16string match_contents = url_formatter::FormatUrl(
      url_, format_types, net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  TermMatches term_matches = FindTermMatches(input_text, match_contents);

  // Update |match_contents_| and |match_contents_class_| if it's allowed.
  if (allow_bolding_nothing || !term_matches.empty()) {
    match_contents_ = match_contents;
    match_contents_class_ = ClassifyTermMatches(
        term_matches, match_contents.size(),
        ACMatchClassification::MATCH | ACMatchClassification::URL,
        ACMatchClassification::URL);
  }
}

int SearchSuggestionParser::NavigationResult::CalculateRelevance(
    const AutocompleteInput& input,
    bool keyword_provider_requested) const {
  return (from_keyword_ || !keyword_provider_requested) ? 800 : 150;
}

void SearchSuggestionParser::NavigationResult::ClassifyDescription(
    const std::u16string& input_text) {
  TermMatches term_matches = FindTermMatches(input_text, description_);
  description_class_ = ClassifyTermMatches(term_matches, description_.size(),
                                           ACMatchClassification::MATCH,
                                           ACMatchClassification::NONE);
}

// SearchSuggestionParser::Results ---------------------------------------------

SearchSuggestionParser::Results::Results()
    : verbatim_relevance(-1),
      field_trial_triggered(false),
      relevances_from_server(false) {}

SearchSuggestionParser::Results::~Results() {}

void SearchSuggestionParser::Results::Clear() {
  suggest_results.clear();
  navigation_results.clear();
  verbatim_relevance = -1;
  metadata.clear();
}

bool SearchSuggestionParser::Results::HasServerProvidedScores() const {
  if (verbatim_relevance >= 0)
    return true;

  // Right now either all results of one type will be server-scored or they will
  // all be locally scored, but in case we change this later, we'll just check
  // them all.
  for (auto i(suggest_results.begin()); i != suggest_results.end(); ++i) {
    if (i->relevance_from_server())
      return true;
  }
  for (auto i(navigation_results.begin()); i != navigation_results.end(); ++i) {
    if (i->relevance_from_server())
      return true;
  }

  return false;
}

// SearchSuggestionParser ------------------------------------------------------

// static
std::string SearchSuggestionParser::ExtractJsonData(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  const net::HttpResponseHeaders* response_headers = nullptr;
  if (source && source->ResponseInfo())
    response_headers = source->ResponseInfo()->headers.get();
  if (!response_body)
    return std::string();

  std::string json_data = std::move(*response_body);

  // JSON is supposed to be UTF-8, but some suggest service providers send
  // JSON files in non-UTF-8 encodings.  The actual encoding is usually
  // specified in the Content-Type header field.
  if (response_headers) {
    std::string charset;
    if (response_headers->GetCharset(&charset)) {
      std::u16string data_16;
      // TODO(jungshik): Switch to CodePageToUTF8 after it's added.
      if (base::CodepageToUTF16(json_data, charset.c_str(),
                                base::OnStringConversionError::FAIL, &data_16))
        json_data = base::UTF16ToUTF8(data_16);
    }
  }
  return json_data;
}

// static
std::unique_ptr<base::Value> SearchSuggestionParser::DeserializeJsonData(
    base::StringPiece json_data) {
  // The JSON response should be an array.
  for (size_t response_start_index = json_data.find("["), i = 0;
       response_start_index != base::StringPiece::npos && i < 5;
       response_start_index = json_data.find("[", 1), i++) {
    // Remove any XSSI guards to allow for JSON parsing.
    json_data.remove_prefix(response_start_index);

    JSONStringValueDeserializer deserializer(json_data,
                                             base::JSON_ALLOW_TRAILING_COMMAS);
    int error_code = 0;
    std::unique_ptr<base::Value> data =
        deserializer.Deserialize(&error_code, nullptr);
    if (error_code == 0)
      return data;
  }
  return nullptr;
}

// static
bool SearchSuggestionParser::ParseSuggestResults(
    const base::Value& root_val,
    const AutocompleteInput& input,
    const AutocompleteSchemeClassifier& scheme_classifier,
    int default_result_relevance,
    bool is_keyword_result,
    Results* results) {
  if (!root_val.is_list())
    return false;
  base::Value::ConstListView root_list = root_val.GetList();

  if (root_list.empty() || !root_list[0].is_string())
    return false;
  std::u16string query = base::UTF8ToUTF16(root_list[0].GetString());
  if (query != input.text())
    return false;

  if (root_list.size() < 2u || !root_list[1].is_list())
    return false;
  base::Value::ConstListView results_list = root_list[1].GetList();

  // 3rd element: Description list.
  absl::optional<base::Value::ConstListView> descriptions;
  if (root_list.size() > 2u && root_list[2].is_list())
    descriptions = root_list[2].GetList();

  // 4th element: Disregard the query URL list for now.

  // Reset suggested relevance information.
  results->verbatim_relevance = -1;

  // 5th element: Optional key-value pairs from the Suggest server.
  const base::ListValue* types = nullptr;
  const base::ListValue* experiment_stats = nullptr;
  const base::ListValue* suggestion_details = nullptr;
  const base::DictionaryValue* extras = nullptr;
  const base::Value* suggestsubtypes = nullptr;
  int prefetch_index = -1;

  absl::optional<base::Value::ConstListView> subtype_identifiers;
  absl::optional<base::Value::ConstListView> relevances;

  if (root_list.size() > 4u && root_list[4].GetAsDictionary(&extras)) {
    extras->GetList("google:suggesttype", &types);

    suggestsubtypes = extras->FindPath("google:suggestsubtypes");

    // Discard this list if its size does not match that of the suggestions.
    const base::Value* relevances_value =
        extras->FindListKey("google:suggestrelevance");
    if (relevances_value &&
        relevances_value->GetList().size() == results_list.size()) {
      relevances = relevances_value->GetList();
    }
    extras->GetInteger("google:verbatimrelevance",
                       &results->verbatim_relevance);

    // Check if the active suggest field trial (if any) has triggered either
    // for the default provider or keyword provider.
    results->field_trial_triggered = false;
    extras->GetBoolean("google:fieldtrialtriggered",
                       &results->field_trial_triggered);

    results->experiment_stats.clear();
    if (extras->GetList("google:experimentstats", &experiment_stats) &&
        experiment_stats) {
      for (size_t index = 0; index < experiment_stats->GetSize(); index++) {
        const base::Value* experiment_stat = nullptr;
        if (experiment_stats->Get(index, &experiment_stat) && experiment_stat) {
          results->experiment_stats.push_back(experiment_stat->Clone());
        }
      }
    }

    const base::DictionaryValue* header_texts = nullptr;
    if (extras->GetDictionary("google:headertexts", &header_texts) &&
        header_texts) {
      const base::DictionaryValue* headers = nullptr;
      if (header_texts->GetDictionary("a", &headers) && headers) {
        for (auto it : headers->DictItems()) {
          int suggestion_group_id;
          base::StringToInt(it.first, &suggestion_group_id);
          results->headers_map[suggestion_group_id] =
              base::UTF8ToUTF16(it.second.GetString());
        }
      }

      const base::ListValue* hidden_group_ids = nullptr;
      if (header_texts->GetList("h", &hidden_group_ids) && hidden_group_ids) {
        for (const auto& value : hidden_group_ids->GetList()) {
          if (value.is_int())
            results->hidden_group_ids.emplace_back(value.GetInt());
        }
      }
    }

    const base::DictionaryValue* client_data = nullptr;
    if (extras->GetDictionary("google:clientdata", &client_data) && client_data)
      client_data->GetInteger("phi", &prefetch_index);

    if (extras->GetList("google:suggestdetail", &suggestion_details) &&
        suggestion_details->GetSize() != results_list.size())
      suggestion_details = nullptr;

    // Legacy code: Get subtype identifiers.
    const base::Value* subtype_identifiers_value =
        extras->FindListKey("google:subtypeid");
    if (subtype_identifiers_value &&
        subtype_identifiers_value->GetList().size() == results_list.size()) {
      subtype_identifiers = subtype_identifiers_value->GetList();
    }

    // Store the metadata that came with the response in case we need to pass it
    // along with the prefetch query to Instant.
    JSONStringValueSerializer json_serializer(&results->metadata);
    json_serializer.Serialize(*extras);
  }

  // Processed list of match subtypes, one vector per match.
  // Note: ParseMatchSubtypes will handle the cases where the key does not
  // exist or contains malformed data.
  std::vector<std::vector<int>> subtypes =
      ParseMatchSubtypes(suggestsubtypes, results_list.size());

  // Clear the previous results now that new results are available.
  results->suggest_results.clear();
  results->navigation_results.clear();

  std::string type;
  int relevance = default_result_relevance;
  const std::u16string& trimmed_input =
      base::CollapseWhitespace(input.text(), false);
  for (size_t index = 0;
       index < results_list.size() && results_list[index].is_string();
       ++index) {
    std::u16string suggestion =
        base::UTF8ToUTF16(results_list[index].GetString());
    // Google search may return empty suggestions for weird input characters,
    // they make no sense at all and can cause problems in our code.
    suggestion = base::CollapseWhitespace(suggestion, false);
    if (suggestion.empty())
      continue;

    // Apply valid suggested relevance scores; discard invalid lists.
    if (relevances) {
      const auto& val = (*relevances)[index];
      if (!val.is_int()) {
        relevances = absl::nullopt;
      } else {
        relevance = val.GetInt();
      }
    }

    AutocompleteMatchType::Type match_type =
        AutocompleteMatchType::SEARCH_SUGGEST;

    // Legacy code: if the server sends us a single subtype ID, place it beside
    // other subtypes.
    if (subtype_identifiers) {
      int subtype_identifier =
          (*subtype_identifiers)[index].GetIfInt().value_or(0);

      if (subtype_identifier != 0) {
        subtypes[index].emplace_back(subtype_identifier);
      }
    }

    if (types && types->GetString(index, &type))
      match_type = GetAutocompleteMatchType(type);
    const base::DictionaryValue* suggestion_detail = nullptr;
    std::string deletion_url;

    if (suggestion_details &&
        suggestion_details->GetDictionary(index, &suggestion_detail)) {
      suggestion_detail->GetString("du", &deletion_url);
    }

    if ((match_type == AutocompleteMatchType::NAVSUGGEST) ||
        (match_type == AutocompleteMatchType::NAVSUGGEST_PERSONALIZED)) {
      // Do not blindly trust the URL coming from the server to be valid.
      GURL url(url_formatter::FixupURL(base::UTF16ToUTF8(suggestion),
                                       std::string()));
      if (url.is_valid()) {
        std::u16string title;
        if (descriptions.has_value() && index < descriptions.value().size() &&
            descriptions.value()[index].is_string()) {
          title = base::UTF8ToUTF16(descriptions.value()[index].GetString());
        }
        results->navigation_results.push_back(NavigationResult(
            scheme_classifier, url, match_type, subtypes[index], title,
            deletion_url, is_keyword_result, relevance, relevances.has_value(),
            input.text()));
      }
    } else {
      std::u16string annotation;
      std::u16string match_contents = suggestion;
      if (match_type == AutocompleteMatchType::CALCULATOR) {
        const bool has_equals_prefix = !suggestion.compare(0, 2, u"= ");
        if (has_equals_prefix) {
          // Calculator results include a "= " prefix but we don't want to
          // include this in the search terms.
          suggestion.erase(0, 2);
          // Unlikely to happen, but better to be safe.
          if (base::CollapseWhitespace(suggestion, false).empty())
            continue;
        }
        if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP) {
          annotation = has_equals_prefix ? suggestion : match_contents;
          match_contents = query;
        }
      }

      std::u16string match_contents_prefix;
      SuggestionAnswer answer;
      bool answer_parsed_successfully = false;
      std::string image_dominant_color;
      std::string image_url;
      std::string additional_query_params;
      absl::optional<int> suggestion_group_id;

      if (suggestion_details) {
        suggestion_details->GetDictionary(index, &suggestion_detail);
        if (suggestion_detail) {
          suggestion_detail->GetString("t", &match_contents);
          suggestion_detail->GetString("mp", &match_contents_prefix);
          // Error correction for bad data from server.
          if (match_contents.empty())
            match_contents = suggestion;
          suggestion_detail->GetString("a", &annotation);
          suggestion_detail->GetString("dc", &image_dominant_color);
          suggestion_detail->GetString("i", &image_url);
          suggestion_detail->GetString("q", &additional_query_params);

          // Suggestion group Id.
          suggestion_group_id = suggestion_detail->FindIntPath("zl");

          // Extract the Answer, if provided.
          const base::DictionaryValue* answer_json = nullptr;
          std::u16string answer_type;
          if (suggestion_detail->GetDictionary("ansa", &answer_json) &&
              suggestion_detail->GetString("ansb", &answer_type)) {
            if (SuggestionAnswer::ParseAnswer(*answer_json, answer_type,
                                              &answer)) {
              base::UmaHistogramSparse("Omnibox.AnswerParseType",
                                       answer.type());
              answer_parsed_successfully = true;
            }
            UMA_HISTOGRAM_BOOLEAN("Omnibox.AnswerParseSuccess",
                                  answer_parsed_successfully);
          }
        }
      }

      bool should_prefetch = static_cast<int>(index) == prefetch_index;
      results->suggest_results.push_back(SuggestResult(
          suggestion, match_type, subtypes[index],
          base::CollapseWhitespace(match_contents, false),
          match_contents_prefix, annotation, additional_query_params,
          deletion_url, image_dominant_color, image_url, is_keyword_result,
          relevance, relevances.has_value(), should_prefetch, trimmed_input));

      if (suggestion_group_id) {
        results->suggest_results.back().set_suggestion_group_id(
            *suggestion_group_id);
      }
      if (answer_parsed_successfully)
        results->suggest_results.back().SetAnswer(answer);
    }
  }
  results->relevances_from_server = relevances.has_value();
  return true;
}
