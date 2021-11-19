// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/realbox/realbox_handler.h"

#include <string>
#include <unordered_map>

#include <gtest/gtest.h>
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/gfx/vector_icon_types.h"

class RealboxHandlerIconTest : public testing::TestWithParam<bool> {
 public:
  RealboxHandlerIconTest() = default;
};

INSTANTIATE_TEST_SUITE_P(, RealboxHandlerIconTest, testing::Bool());

// Tests that all Omnibox vector icons map to an equivalent SVG for use in the
// NTP Realbox.
TEST_P(RealboxHandlerIconTest, VectorIcons) {
  for (int type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
       type != AutocompleteMatchType::NUM_TYPES; type++) {
    AutocompleteMatch match;
    match.type = static_cast<AutocompleteMatchType::Type>(type);
    const bool is_bookmark = GetParam();
    const gfx::VectorIcon& vector_icon = match.GetVectorIcon(is_bookmark);
    const std::string& svg_name =
        RealboxHandler::AutocompleteMatchVectorIconToResourceName(vector_icon);
    if (vector_icon.name == omnibox::kBlankIcon.name) {
      // An empty resource name is effectively a blank icon.
      ASSERT_TRUE(svg_name.empty());
    } else if (vector_icon.name == omnibox::kPedalIcon.name) {
      // Pedals are not supported in the NTP Realbox.
      ASSERT_TRUE(svg_name.empty());
    } else if (is_bookmark) {
      ASSERT_EQ("chrome://resources/images/icon_bookmark.svg", svg_name);
    } else {
      ASSERT_FALSE(svg_name.empty());
    }
  }
  for (int answer_type = SuggestionAnswer::ANSWER_TYPE_DICTIONARY;
       answer_type != SuggestionAnswer::ANSWER_TYPE_TOTAL_COUNT;
       answer_type++) {
    EXPECT_FALSE(
        base::FeatureList::IsEnabled(omnibox::kNtpRealboxSuggestionAnswers));
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(omnibox::kNtpRealboxSuggestionAnswers);
    EXPECT_TRUE(
        base::FeatureList::IsEnabled(omnibox::kNtpRealboxSuggestionAnswers));
    AutocompleteMatch match;
    SuggestionAnswer answer;
    answer.set_type(answer_type);
    match.answer = answer;
    const bool is_bookmark = GetParam();
    const gfx::VectorIcon& vector_icon = match.GetVectorIcon(is_bookmark);
    const std::string& svg_name =
        RealboxHandler::AutocompleteMatchVectorIconToResourceName(vector_icon);
    if (is_bookmark) {
      ASSERT_EQ("chrome://resources/images/icon_bookmark.svg", svg_name);
    } else {
      ASSERT_FALSE(svg_name.empty());
      ASSERT_NE("search.svg", svg_name);
    }
  }

  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals =
      GetPedalImplementations(true, true);
  for (auto const& it : pedals) {
    const scoped_refptr<OmniboxPedal> pedal = it.second;
    const gfx::VectorIcon& vector_icon = pedal->GetVectorIcon();
    const std::string& svg_name =
        RealboxHandler::PedalVectorIconToResourceName(vector_icon);
    ASSERT_FALSE(svg_name.empty());
  }
}
