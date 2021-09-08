// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_tags_util.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"

namespace app_list {
namespace {

int ACMatchStyleToTagStyle(int styles) {
  int tag_styles = 0;
  if (styles & ACMatchClassification::URL)
    tag_styles |= ash::SearchResultTag::URL;
  if (styles & ACMatchClassification::MATCH)
    tag_styles |= ash::SearchResultTag::MATCH;
  if (styles & ACMatchClassification::DIM)
    tag_styles |= ash::SearchResultTag::DIM;

  return tag_styles;
}

}  // namespace

void ACMatchClassificationsToTags(const std::u16string& text,
                                  const ACMatchClassifications& text_classes,
                                  ChromeSearchResult::Tags* tags) {
  int tag_styles = ash::SearchResultTag::NONE;
  size_t tag_start = 0;

  for (size_t i = 0; i < text_classes.size(); ++i) {
    const ACMatchClassification& text_class = text_classes[i];

    // Closes current tag.
    if (tag_styles != ash::SearchResultTag::NONE) {
      tags->push_back(
          ash::SearchResultTag(tag_styles, tag_start, text_class.offset));
      tag_styles = ash::SearchResultTag::NONE;
    }

    if (text_class.style == ACMatchClassification::NONE)
      continue;

    tag_start = text_class.offset;
    tag_styles = ACMatchStyleToTagStyle(text_class.style);
  }

  if (tag_styles != ash::SearchResultTag::NONE) {
    tags->push_back(ash::SearchResultTag(tag_styles, tag_start, text.length()));
  }
}

ChromeSearchResult::Tags CalculateTags(const std::u16string& query,
                                       const std::u16string& text) {
  const auto matches = FindTermMatches(query, text);
  const auto classes =
      ClassifyTermMatches(matches, text.length(),
                          /*match_style=*/ACMatchClassification::MATCH,
                          /*non_match_style=*/ACMatchClassification::NONE);

  ChromeSearchResult::Tags tags;
  ACMatchClassificationsToTags(text, classes, &tags);
  return tags;
}

}  // namespace app_list
