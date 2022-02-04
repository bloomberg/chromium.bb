// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/shortcuts_provider.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/cxx17_backports.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/omnibox/browser/shortcuts_provider_test_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

using base::ASCIIToUTF16;
using ExpectedURLs = std::vector<ExpectedURLAndAllowedToBeDefault>;

namespace {

struct TestShortcutData shortcut_test_db[] = {
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E0", "goog", "www.google.com",
     "http://www.google.com/", AutocompleteMatch::DocumentType::NONE, "Google",
     "0,1,4,0", "Google", "0,3,4,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E1", "slash", "slashdot.org",
     "http://slashdot.org/", AutocompleteMatch::DocumentType::NONE,
     "slashdot.org", "0,3,5,1", "Slashdot - News for nerds, stuff that matters",
     "0,2,5,0", ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL,
     "", 0, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E2", "news", "slashdot.org",
     "http://slashdot.org/", AutocompleteMatch::DocumentType::NONE,
     "slashdot.org", "0,1", "Slashdot - News for nerds, stuff that matters",
     "0,0,11,2,15,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 0, 5},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E3", "news", "sports.yahoo.com",
     "http://sports.yahoo.com/", AutocompleteMatch::DocumentType::NONE,
     "sports.yahoo.com", "0,1",
     "Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more",
     "0,0,23,2,27,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 2, 5},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E4", "news weather",
     "www.cnn.com/index.html", "http://www.cnn.com/index.html",
     AutocompleteMatch::DocumentType::NONE, "www.cnn.com/index.html", "0,1",
     "CNN.com - Breaking News, U.S., World, Weather, Entertainment & Video",
     "0,0,19,2,23,0,38,2,45,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 1, 10},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E5", "nhl scores", "sports.yahoo.com",
     "http://sports.yahoo.com/", AutocompleteMatch::DocumentType::NONE,
     "sports.yahoo.com", "0,1",
     "Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more",
     "0,0,29,2,35,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_BODY, "", 1, 10},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E6", "nhl scores",
     "www.nhl.com/scores/index.html", "http://www.nhl.com/scores/index.html",
     AutocompleteMatch::DocumentType::NONE, "www.nhl.com/scores/index.html",
     "0,1,4,3,7,1", "January 13, 2010 - NHL.com - Scores",
     "0,0,19,2,22,0,29,2,35,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 5, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E7", "just", "www.testsite.com/a.html",
     "http://www.testsite.com/a.html", AutocompleteMatch::DocumentType::NONE,
     "www.testsite.com/a.html", "0,1", "Test - site - just a test",
     "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 5, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E8", "just", "www.testsite.com/b.html",
     "http://www.testsite.com/b.html", AutocompleteMatch::DocumentType::NONE,
     "www.testsite.com/b.html", "0,1", "Test - site - just a test",
     "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 5, 2},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E9", "just", "www.testsite.com/c.html",
     "http://www.testsite.com/c.html", AutocompleteMatch::DocumentType::NONE,
     "www.testsite.com/c.html", "0,1", "Test - site - just a test",
     "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 8, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880EA", "just a",
     "www.testsite.com/d.html", "http://www.testsite.com/d.html",
     AutocompleteMatch::DocumentType::NONE, "www.testsite.com/d.html", "0,1",
     "Test - site - just a test", "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 12, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880EB", "just a t",
     "www.testsite.com/e.html", "http://www.testsite.com/e.html",
     AutocompleteMatch::DocumentType::NONE, "www.testsite.com/e.html", "0,1",
     "Test - site - just a test", "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 12, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880EC", "just a te",
     "www.testsite.com/f.html", "http://www.testsite.com/f.html",
     AutocompleteMatch::DocumentType::NONE, "www.testsite.com/f.html", "0,1",
     "Test - site - just a test", "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 12, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880ED", "ago",
     "www.daysagotest.com/a.html", "http://www.daysagotest.com/a.html",
     AutocompleteMatch::DocumentType::NONE, "www.daysagotest.com/a.html",
     "0,1,8,3,11,1", "Test - site", "0,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880EE", "ago",
     "www.daysagotest.com/b.html", "http://www.daysagotest.com/b.html",
     AutocompleteMatch::DocumentType::NONE, "www.daysagotest.com/b.html",
     "0,1,8,3,11,1", "Test - site", "0,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 2, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880EF", "ago",
     "www.daysagotest.com/c.html", "http://www.daysagotest.com/c.html",
     AutocompleteMatch::DocumentType::NONE, "www.daysagotest.com/c.html",
     "0,1,8,3,11,1", "Test - site", "0,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 3, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F0", "ago",
     "www.daysagotest.com/d.html", "http://www.daysagotest.com/d.html",
     AutocompleteMatch::DocumentType::NONE, "www.daysagotest.com/d.html",
     "0,1,8,3,11,1", "Test - site", "0,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 4, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F2", "abcdef.com", "http://abcdef.com",
     "http://abcdef.com/", AutocompleteMatch::DocumentType::NONE, "Abcdef",
     "0,1,4,0", "Abcdef", "0,3,4,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F3", "query", "query",
     "https://www.google.com/search?q=query",
     AutocompleteMatch::DocumentType::NONE, "query", "0,0", "Google Search",
     "0,4", ui::PAGE_TRANSITION_GENERATED,
     AutocompleteMatchType::SEARCH_HISTORY, "google.com", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F4", "word", "www.word",
     "https://www.google.com/search?q=www.word",
     AutocompleteMatch::DocumentType::NONE, "www.word", "0,0", "Google Search",
     "0,4", ui::PAGE_TRANSITION_GENERATED,
     AutocompleteMatchType::SEARCH_HISTORY, "google.com", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F5", "about:o", "chrome://omnibox",
     "chrome://omnibox/", AutocompleteMatch::DocumentType::NONE,
     "about:omnibox", "0,3,10,1", "", "", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::NAVSUGGEST, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F6", "www/real sp",
     "http://www/real space/long-url-with-space.html",
     "http://www/real%20space/long-url-with-space.html",
     AutocompleteMatch::DocumentType::NONE,
     "www/real space/long-url-with-space.html", "0,3,11,1",
     "Page With Space; Input with Space", "0,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F7", "duplicate",
     "http://duplicate.com", "http://duplicate.com/",
     AutocompleteMatch::DocumentType::NONE, "Duplicate", "0,1", "Duplicate",
     "0,1", ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "",
     1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F8", "dupl", "http://duplicate.com",
     "http://duplicate.com/", AutocompleteMatch::DocumentType::NONE,
     "Duplicate", "0,1", "Duplicate", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F9", "notrailing.com/",
     "http://notrailing.com", "http://notrailing.com/",
     AutocompleteMatch::DocumentType::NONE, "No Trailing Slash", "0,1",
     "No Trailing Slash on fill_into_edit", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880FA", "http:///foo.com",
     "http://foo.com", "http://foo.com/", AutocompleteMatch::DocumentType::NONE,
     "Foo - Typo in Input", "0,1",
     "Foo - Typo in Input Corrected in fill_into_edit", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880FB", "trailing1 ",
     "http://trailing1.com", "http://trailing1.com/",
     AutocompleteMatch::DocumentType::NONE, "Trailing1 - Space in Shortcut",
     "0,1", "Trailing1 - Space in Shortcut", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880FC", "about:trailing2 ",
     "chrome://trailing2blah", "chrome://trailing2blah/",
     AutocompleteMatch::DocumentType::NONE, "Trailing2 - Space in Shortcut",
     "0,1", "Trailing2 - Space in Shortcut", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 100},

    // 4 shortcuts to verify aggregating shortcuts.
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880FD", "wikipedia", "",
     "https://wikipedia.org/wilson7", AutocompleteMatch::DocumentType::NONE, "",
     "0,1", "", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880FE", "wilson7", "",
     "https://wikipedia.org/wilson7", AutocompleteMatch::DocumentType::NONE, "",
     "0,1", "", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 2, 2},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880FF", "winston", "",
     "https://wikipedia.org/winston", AutocompleteMatch::DocumentType::NONE, "",
     "0,1", "", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 3},
    {"BD85DBA2-8C29-49F9-84AE-48E1E9088100", "wilson7", "",
     "https://wikipedia.org/wilson7-other",
     AutocompleteMatch::DocumentType::NONE, "", "0,1", "", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 2, 2},

    // 7 shortcuts to verify the interaction of the provider limit and
    // aggregating shortcuts.
    {"BD85DBA2-8C29-49F9-84AE-48E1E9088101", "zebra1", "",
     "https://wikipedia.org/zebra-a", AutocompleteMatch::DocumentType::NONE, "",
     "0,1", "", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E9088102", "zebra2", "",
     "https://wikipedia.org/zebra-a", AutocompleteMatch::DocumentType::NONE, "",
     "0,1", "", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 2},
    {"BD85DBA2-8C29-49F9-84AE-48E1E9088103", "zebra3", "",
     "https://wikipedia.org/zebra-a", AutocompleteMatch::DocumentType::NONE, "",
     "0,1", "", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 3},
    {"BD85DBA2-8C29-49F9-84AE-48E1E9088104", "zebra4", "",
     "https://wikipedia.org/zebra-a", AutocompleteMatch::DocumentType::NONE, "",
     "0,1", "", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 4},
    {"BD85DBA2-8C29-49F9-84AE-48E1E9088105", "zebra5", "",
     "https://wikipedia.org/zebra-b", AutocompleteMatch::DocumentType::NONE, "",
     "0,1", "", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 6},
    {"BD85DBA2-8C29-49F9-84AE-48E1E9088106", "zebra6", "",
     "https://wikipedia.org/zebra-b", AutocompleteMatch::DocumentType::NONE, "",
     "0,1", "", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 4},
    {"BD85DBA2-8C29-49F9-84AE-48E1E9088107", "zebra7", "",
     "https://wikipedia.org/zebra-c", AutocompleteMatch::DocumentType::NONE, "",
     "0,1", "", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 10},
};

ShortcutsDatabase::Shortcut MakeShortcut(
    std::u16string text,
    const base::Time& last_access_time = base::Time::Now(),
    int number_of_hits = 1) {
  return {std::string(), text,
          ShortcutsDatabase::Shortcut::MatchCore(
              u"www.test.com", GURL("http://www.test.com"),
              AutocompleteMatch::DocumentType::NONE, u"www.test.com",
              "0,1,4,3,8,1", u"A test", "0,0,2,2", ui::PAGE_TRANSITION_TYPED,
              AutocompleteMatchType::HISTORY_URL, std::u16string()),
          last_access_time, number_of_hits};
}

}  // namespace

// ShortcutsProviderTest ------------------------------------------------------

class ShortcutsProviderTest : public testing::Test {
 public:
  ShortcutsProviderTest();

 protected:
  void SetUp() override;
  void TearDown() override;

  // Passthrough to the private function in provider_.
  int CalculateScore(const std::string& terms,
                     const ShortcutsDatabase::Shortcut& shortcut);
  int CalculateAggregateScore(
      const std::string& terms,
      const std::vector<const ShortcutsDatabase::Shortcut*>& shortcuts);

  // ScopedFeatureList needs to be defined before TaskEnvironment, so that it is
  // destroyed after TaskEnvironment, to prevent data races on the
  // ScopedFeatureList.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<ShortcutsProvider> provider_;
};

ShortcutsProviderTest::ShortcutsProviderTest() {
  // `scoped_feature_list_` needs to be initialized as early as possible, to
  // avoid data races caused by tasks on other threads accessing it.
  scoped_feature_list_.InitAndDisableFeature(
      omnibox::kPreserveLongerShortcutsText);
}

void ShortcutsProviderTest::SetUp() {
  client_ = std::make_unique<FakeAutocompleteProviderClient>();

  ASSERT_TRUE(client_->GetShortcutsBackend());
  provider_ = base::MakeRefCounted<ShortcutsProvider>(client_.get());
  PopulateShortcutsBackendWithTestData(client_->GetShortcutsBackend(),
                                       shortcut_test_db,
                                       base::size(shortcut_test_db));
}

void ShortcutsProviderTest::TearDown() {
  provider_ = nullptr;
  client_.reset();
  task_environment_.RunUntilIdle();
}

int ShortcutsProviderTest::CalculateScore(
    const std::string& terms,
    const ShortcutsDatabase::Shortcut& shortcut) {
  const int max_relevance =
      ShortcutsProvider::kShortcutsProviderDefaultMaxRelevance;
  return provider_->CalculateScore(ASCIIToUTF16(terms), shortcut,
                                   max_relevance);
}

int ShortcutsProviderTest::CalculateAggregateScore(
    const std::string& terms,
    const std::vector<const ShortcutsDatabase::Shortcut*>& shortcuts) {
  const int max_relevance =
      ShortcutsProvider::kShortcutsProviderDefaultMaxRelevance;
  return provider_->CalculateAggregateScore(ASCIIToUTF16(terms), shortcuts,
                                            max_relevance);
}

// Actual tests ---------------------------------------------------------------

TEST_F(ShortcutsProviderTest, SimpleSingleMatch) {
  std::u16string text(u"go");
  std::string expected_url("http://www.google.com/");
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u"ogle.com");

  // Same test with prevent inline autocomplete.
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  // The match will have an |inline_autocompletion| set, but the value will not
  // be used because |allowed_to_be_default_match| will be false.
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           u"ogle.com");

  // A pair of analogous tests where the shortcut ends at the end of
  // |fill_into_edit|.  This exercises the inline autocompletion and default
  // match code.
  text = u"abcdef.com";
  expected_url = "http://abcdef.com/";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           std::u16string());
  // With prevent inline autocomplete, the suggestion should be the same
  // (because there is no completion).
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           std::u16string());

  // Another test, simply for a query match type, not a navigation URL match
  // type.
  text = u"que";
  expected_url = "https://www.google.com/search?q=query";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u"ry");

  // Same test with prevent inline autocomplete.
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  // The match will have an |inline_autocompletion| set, but the value will not
  // be used because |allowed_to_be_default_match| will be false.
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           u"ry");

  // A pair of analogous tests where the shortcut ends at the end of
  // |fill_into_edit|.  This exercises the inline autocompletion and default
  // match code.
  text = u"query";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           std::u16string());
  // With prevent inline autocomplete, the suggestion should be the same
  // (because there is no completion).
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           std::u16string());

  // Now the shortcut ends at the end of |fill_into_edit| but has a
  // non-droppable prefix.  ("www.", for instance, is not droppable for
  // queries.)
  text = u"word";
  expected_url = "https://www.google.com/search?q=www.word";
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           std::u16string());
}

// These tests are like those in SimpleSingleMatch but more complex,
// involving URLs that need to be fixed up to match properly.
TEST_F(ShortcutsProviderTest, TrickySingleMatch) {
  // Test that about: URLs are fixed up/transformed to chrome:// URLs.
  std::u16string text(u"about:o");
  std::string expected_url("chrome://omnibox/");
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u"mnibox");

  // Same test with prevent inline autocomplete.
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  // The match will have an |inline_autocompletion| set, but the value will not
  // be used because |allowed_to_be_default_match| will be false.
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           u"mnibox");

  // Test that an input with a space can match URLs with a (escaped) space.
  // This would fail if we didn't try to lookup the un-fixed-up string.
  text = u"www/real sp";
  expected_url = "http://www/real%20space/long-url-with-space.html";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u"ace/long-url-with-space.html");

  // Same test with prevent inline autocomplete.
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  // The match will have an |inline_autocompletion| set, but the value will not
  // be used because |allowed_to_be_default_match| will be false.
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           u"ace/long-url-with-space.html");

  // Test when the user input has a typo that can be fixed up for matching
  // fill_into_edit.  This should still be allowed to be default.
  text = u"http:///foo.com";
  expected_url = "http://foo.com/";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           std::u16string());

  // A foursome of tests to verify that trailing spaces prevent the shortcut
  // from being allowed to be the default match.  For each of two tests, we
  // first verify that the match is allowed to be default without the trailing
  // space but is not allowed to be default with the trailing space.  In both
  // of these with-trailing-space cases, we actually get an
  // inline_autocompletion, though it's never used because the match is
  // prohibited from being default.
  text = u"trailing1";
  expected_url = "http://trailing1.com/";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u".com");
  text = u"trailing1 ";
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u".com");
  text = u"about:trailing2";
  expected_url = "chrome://trailing2blah/";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u"blah");
  text = u"about:trailing2 ";
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u"blah");
}

TEST_F(ShortcutsProviderTest, MultiMatch) {
  std::u16string text(u"NEWS");
  ExpectedURLs expected_urls;
  // Scores high because of completion length.
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://slashdot.org/", false));
  // Scores high because of visit count.
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://sports.yahoo.com/", false));
  // Scores high because of visit count but less match span,
  // which is more important.
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://www.cnn.com/index.html", false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://slashdot.org/", std::u16string());
}

TEST_F(ShortcutsProviderTest, RemoveDuplicates) {
  std::u16string text(u"dupl");
  ExpectedURLs expected_urls;
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://duplicate.com/", true));
  // Make sure the URL only appears once in the output list.
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://duplicate.com/", u"icate.com");
}

TEST_F(ShortcutsProviderTest, TypedCountMatches) {
  std::u16string text(u"just");
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.testsite.com/b.html", false));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.testsite.com/a.html", false));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.testsite.com/c.html", false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://www.testsite.com/b.html", std::u16string());
}

TEST_F(ShortcutsProviderTest, FragmentLengthMatches) {
  std::u16string text(u"just a");
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.testsite.com/d.html", false));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.testsite.com/e.html", false));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.testsite.com/f.html", false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://www.testsite.com/d.html", std::u16string());
}

TEST_F(ShortcutsProviderTest, DaysAgoMatches) {
  std::u16string text(u"ago");
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.daysagotest.com/a.html", false));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.daysagotest.com/b.html", false));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.daysagotest.com/c.html", false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://www.daysagotest.com/a.html",
                           std::u16string());
}

TEST_F(ShortcutsProviderTest, CalculateScore) {
  auto shortcut = MakeShortcut(u"test");

  // Maximal score.
  const int kMaxScore = CalculateScore("test", shortcut);

  // Score decreases as percent of the match is decreased.
  int score_three_quarters = CalculateScore("tes", shortcut);
  EXPECT_LT(score_three_quarters, kMaxScore);
  int score_one_half = CalculateScore("te", shortcut);
  EXPECT_LT(score_one_half, score_three_quarters);
  int score_one_quarter = CalculateScore("t", shortcut);
  EXPECT_LT(score_one_quarter, score_one_half);

  // Should decay with time - one week.
  shortcut.last_access_time = base::Time::Now() - base::Days(7);
  int score_week_old = CalculateScore("test", shortcut);
  EXPECT_LT(score_week_old, kMaxScore);

  // Should decay more in two weeks.
  shortcut.last_access_time = base::Time::Now() - base::Days(14);
  int score_two_weeks_old = CalculateScore("test", shortcut);
  EXPECT_LT(score_two_weeks_old, score_week_old);

  // But not if it was actively clicked on. 2 hits slow decaying power.
  shortcut.number_of_hits = 2;
  shortcut.last_access_time = base::Time::Now() - base::Days(14);
  int score_popular_two_weeks_old = CalculateScore("test", shortcut);
  EXPECT_LT(score_two_weeks_old, score_popular_two_weeks_old);
  // But still decayed.
  EXPECT_LT(score_popular_two_weeks_old, kMaxScore);

  // 3 hits slow decaying power even more.
  shortcut.number_of_hits = 3;
  shortcut.last_access_time = base::Time::Now() - base::Days(14);
  int score_more_popular_two_weeks_old = CalculateScore("test", shortcut);
  EXPECT_LT(score_two_weeks_old, score_more_popular_two_weeks_old);
  EXPECT_LT(score_popular_two_weeks_old, score_more_popular_two_weeks_old);
  // But still decayed.
  EXPECT_LT(score_more_popular_two_weeks_old, kMaxScore);
}

class ShortcutsProviderPreserveLongerShortcutsTest
    : public ShortcutsProviderTest {
 public:
  ShortcutsProviderPreserveLongerShortcutsTest() {
    // `scoped_feature_list_` needs to be initialized as early as possible, to
    // avoid data races caused by tasks on other threads accessing it.
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(
        omnibox::kPreserveLongerShortcutsText);
  }
};

TEST_F(ShortcutsProviderPreserveLongerShortcutsTest,
       CalculateScore_LongTextFeature) {
  auto long_shortcut = MakeShortcut(u"test Yerevan");
  // Maximal score.
  const int kMaxScore = CalculateScore("test Yerevan", long_shortcut);
  // Score does not decrease when up to 3 chars are missing.
  EXPECT_EQ(CalculateScore("test Yere", long_shortcut), kMaxScore);
  // Score decreases if more than 3 chars are missing.
  EXPECT_LT(CalculateScore("test Yer", long_shortcut), kMaxScore);

  auto short_shortcut = MakeShortcut(u"ab");
  // Make sure there's no negative or weird scores when the shortcut text is
  // shorter than the 3 char adjustment.
  EXPECT_EQ(CalculateScore("ab", short_shortcut), kMaxScore);
  EXPECT_EQ(CalculateScore("a", short_shortcut), kMaxScore);
}

TEST_F(ShortcutsProviderTest, DeleteMatch) {
  TestShortcutData shortcuts_to_test_delete[] = {
      {"BD85DBA2-8C29-49F9-84AE-48E1E90881F1", "delete", "www.deletetest.com/1",
       "http://www.deletetest.com/1", AutocompleteMatch::DocumentType::NONE,
       "http://www.deletetest.com/1", "0,2", "Erase this shortcut!", "0,0",
       ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 1},
      {"BD85DBA2-8C29-49F9-84AE-48E1E90881F2", "erase", "www.deletetest.com/1",
       "http://www.deletetest.com/1", AutocompleteMatch::DocumentType::NONE,
       "http://www.deletetest.com/1", "0,2", "Erase this shortcut!", "0,0",
       ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_TITLE, "", 1,
       1},
      {"BD85DBA2-8C29-49F9-84AE-48E1E90881F3", "keep", "www.deletetest.com/1/2",
       "http://www.deletetest.com/1/2", AutocompleteMatch::DocumentType::NONE,
       "http://www.deletetest.com/1/2", "0,2", "Keep this shortcut!", "0,0",
       ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_TITLE, "", 1,
       1},
      {"BD85DBA2-8C29-49F9-84AE-48E1E90881F4", "delete", "www.deletetest.com/2",
       "http://www.deletetest.com/2", AutocompleteMatch::DocumentType::NONE,
       "http://www.deletetest.com/2", "0,2", "Erase this shortcut!", "0,0",
       ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 1},
  };

  scoped_refptr<ShortcutsBackend> backend = client_->GetShortcutsBackend();
  size_t original_shortcuts_count = backend->shortcuts_map().size();

  PopulateShortcutsBackendWithTestData(backend, shortcuts_to_test_delete,
                                       base::size(shortcuts_to_test_delete));

  EXPECT_EQ(original_shortcuts_count + 4, backend->shortcuts_map().size());
  EXPECT_FALSE(backend->shortcuts_map().end() ==
               backend->shortcuts_map().find(u"delete"));
  EXPECT_FALSE(backend->shortcuts_map().end() ==
               backend->shortcuts_map().find(u"erase"));

  AutocompleteMatch match(provider_.get(), 1200, true,
                          AutocompleteMatchType::HISTORY_TITLE);

  match.destination_url = GURL(shortcuts_to_test_delete[0].destination_url);
  match.contents = ASCIIToUTF16(shortcuts_to_test_delete[0].contents);
  match.description = ASCIIToUTF16(shortcuts_to_test_delete[0].description);

  provider_->DeleteMatch(match);

  // shortcuts_to_test_delete[0] and shortcuts_to_test_delete[1] should be
  // deleted, but not shortcuts_to_test_delete[2] or
  // shortcuts_to_test_delete[3], which have different URLs.
  EXPECT_EQ(original_shortcuts_count + 2, backend->shortcuts_map().size());
  EXPECT_FALSE(backend->shortcuts_map().end() ==
               backend->shortcuts_map().find(u"delete"));
  EXPECT_TRUE(backend->shortcuts_map().end() ==
              backend->shortcuts_map().find(u"erase"));

  match.destination_url = GURL(shortcuts_to_test_delete[3].destination_url);
  match.contents = ASCIIToUTF16(shortcuts_to_test_delete[3].contents);
  match.description = ASCIIToUTF16(shortcuts_to_test_delete[3].description);

  provider_->DeleteMatch(match);
  EXPECT_EQ(original_shortcuts_count + 1, backend->shortcuts_map().size());
  EXPECT_TRUE(backend->shortcuts_map().end() ==
              backend->shortcuts_map().find(u"delete"));
}

TEST_F(ShortcutsProviderTest, DoesNotProvideOnFocus) {
  AutocompleteInput input(u"about:o", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

class ShortcutsProviderAggregateShortcutsTest : public ShortcutsProviderTest {
 public:
  ShortcutsProviderAggregateShortcutsTest() {
    // `scoped_feature_list_` needs to be initialized as early as possible, to
    // avoid data races caused by tasks on other threads accessing it.
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(omnibox::kAggregateShortcuts);
  }
};

TEST_F(ShortcutsProviderAggregateShortcutsTest, GetMatches) {
  {
    // When multiple shortcuts with the same destination URL match the input,
    // they should be scored together (i.e. their visit counts summed, the most
    // recent visit date and shortest text considered).
    AutocompleteInput input(u"wi", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const auto& matches = provider_->matches();
    EXPECT_EQ(matches.size(), 3u);
    // There are 2 shortcuts with the wilson7 url which have the same aggregate
    // text length, visit count, and last visit as the 1 winston shortcut.
    EXPECT_EQ(matches[0].destination_url.spec(),
              "https://wikipedia.org/winston");
    EXPECT_EQ(matches[1].destination_url.spec(),
              "https://wikipedia.org/wilson7");
    // Matches with the same score otherwise, are demoted by 1, hence the `+ 1`.
    EXPECT_EQ(matches[0].relevance, matches[1].relevance + 1);
    EXPECT_EQ(matches[2].destination_url.spec(),
              "https://wikipedia.org/wilson7-other");
    EXPECT_GT(matches[0].relevance, matches[2].relevance + 1);
    EXPECT_GT(matches[2].relevance, 0);
  }

  {
    // When multiple shortcuts have the same destination URL but only 1 matches
    // the input, they should not be scored together.
    AutocompleteInput input(u"wilson", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const auto& matches = provider_->matches();
    EXPECT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0].destination_url.spec(),
              "https://wikipedia.org/wilson7");
    EXPECT_EQ(matches[1].destination_url.spec(),
              "https://wikipedia.org/wilson7-other");
    EXPECT_EQ(matches[0].relevance, matches[1].relevance + 1);
    EXPECT_GT(matches[1].relevance, 0);
  }

  {
    // The provider limit should not affect number of shortcuts aggregated, only
    // the matches returned, i.e. the number of aggregate shortcuts. There are
    // 7 shortcuts matching the input with 3 unique URLs. The 3 aggregate
    // shortcuts have the same aggregate score factors and should be scored the
    // same (other than the `+ 1` limitation).
    AutocompleteInput input(u"zebra", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const auto& matches = provider_->matches();
    EXPECT_EQ(matches.size(), 3u);
    EXPECT_EQ(matches[0].destination_url.spec(),
              "https://wikipedia.org/zebra-c");
    EXPECT_EQ(matches[1].destination_url.spec(),
              "https://wikipedia.org/zebra-a");
    EXPECT_EQ(matches[2].destination_url.spec(),
              "https://wikipedia.org/zebra-b");
    EXPECT_EQ(matches[0].relevance, matches[1].relevance + 1);
    EXPECT_EQ(matches[1].relevance, matches[2].relevance + 1);
    EXPECT_GT(matches[2].relevance, 0);
  }
}

TEST_F(ShortcutsProviderTest, CalculateAggregateScore) {
  const auto days_ago = [](int n) { return base::Time::Now() - base::Days(n); };

  // Aggregate score should consider the shortest text length, most recent visit
  // time, and sum of visit counts.
  auto shortcut_a_short = MakeShortcut(u"size5", days_ago(3), 1);
  auto shortcut_a_frequent = MakeShortcut(u"size____10", days_ago(3), 10);
  auto shortcut_a_recent = MakeShortcut(u"size____10", days_ago(1), 1);
  auto score_a = CalculateAggregateScore(
      "a", {&shortcut_a_short, &shortcut_a_frequent, &shortcut_a_recent});
  auto shortcut_b = MakeShortcut(u"size5", days_ago(1), 12);
  auto score_b = CalculateAggregateScore("a", {&shortcut_b});
  EXPECT_EQ(score_a, score_b);
  EXPECT_GT(score_a, 0);

  // `CalculateAggregateScore` should give the same scores as `CalculateScore`
  // when there is only 1 shortcut (i.e. no aggregation).
  auto score_b_non_aggregate = CalculateScore("a", shortcut_b);
  EXPECT_EQ(score_b_non_aggregate, score_b);

  // Typing more of the text increases score.
  auto score_b_long_query = CalculateAggregateScore("ab", {&shortcut_b});
  EXPECT_GT(score_b_long_query, score_b);

  // More recent shortcuts should be scored higher.
  auto shortcut_b_old = MakeShortcut(u"size5", days_ago(2), 12);
  auto score_b_old = CalculateAggregateScore("a", {&shortcut_b_old});
  EXPECT_LT(score_b_old, score_b);

  // Shortcuts with higher visit counts should be scored higher.
  auto shortcut_b_frequent = MakeShortcut(u"size5", days_ago(1), 13);
  auto score_b_frequent = CalculateAggregateScore("a", {&shortcut_b_frequent});
  EXPECT_GT(score_b_frequent, score_b);
}
