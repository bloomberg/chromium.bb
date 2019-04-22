// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/builtin_provider.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

namespace {

const char kEmbedderAboutScheme[] = "chrome";
const char kDefaultURL1[] = "chrome://default1/";
const char kDefaultURL2[] = "chrome://default2/";
const char kDefaultURL3[] = "chrome://foo/";
const char kSubpageURL[] = "chrome://subpage/";

// Arbitrary host constants, chosen to start with the letters "b" and "me".
const char kHostBar[] = "bar";
const char kHostMedia[] = "media";
const char kHostMemory[] = "memory";
const char kHostMemoryInternals[] = "memory-internals";
const char kHostSubpage[] = "subpage";

const char kSubpageOne[] = "one";
const char kSubpageTwo[] = "two";
const char kSubpageThree[] = "three";

class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  FakeAutocompleteProviderClient() {}

  std::string GetEmbedderRepresentationOfAboutScheme() const override {
    return kEmbedderAboutScheme;
  }

  std::vector<base::string16> GetBuiltinURLs() override {
    std::vector<base::string16> urls;
    urls.push_back(ASCIIToUTF16(kHostBar));
    urls.push_back(ASCIIToUTF16(kHostMedia));
    // The URL that is a superstring of the other is intentionally placed first
    // here. The provider makes no guarantees that shorter URLs will appear
    // first.
    urls.push_back(ASCIIToUTF16(kHostMemoryInternals));
    urls.push_back(ASCIIToUTF16(kHostMemory));
    urls.push_back(ASCIIToUTF16(kHostSubpage));

    base::string16 prefix = ASCIIToUTF16(kHostSubpage) + ASCIIToUTF16("/");
    urls.push_back(prefix + ASCIIToUTF16(kSubpageOne));
    urls.push_back(prefix + ASCIIToUTF16(kSubpageTwo));
    urls.push_back(prefix + ASCIIToUTF16(kSubpageThree));
    return urls;
  }

  std::vector<base::string16> GetBuiltinsToProvideAsUserTypes() override {
    std::vector<base::string16> urls;
    urls.push_back(ASCIIToUTF16(kDefaultURL1));
    urls.push_back(ASCIIToUTF16(kDefaultURL2));
    urls.push_back(ASCIIToUTF16(kDefaultURL3));
    return urls;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAutocompleteProviderClient);
};

}  // namespace

class BuiltinProviderTest : public testing::Test {
 protected:
  struct TestData {
    const base::string16 input;
    const std::vector<GURL> output;
  };

  BuiltinProviderTest() : provider_(nullptr) {}
  ~BuiltinProviderTest() override {}

  void SetUp() override {
    client_.reset(new FakeAutocompleteProviderClient());
    provider_ = new BuiltinProvider(client_.get());
  }
  void TearDown() override { provider_ = nullptr; }

  void RunTest(const TestData cases[], size_t num_cases) {
    ACMatches matches;
    for (size_t i = 0; i < num_cases; ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "case %" PRIuS ": %s", i, base::UTF16ToUTF8(cases[i].input).c_str()));
      AutocompleteInput input(cases[i].input, metrics::OmniboxEventProto::OTHER,
                              TestSchemeClassifier());
      input.set_prevent_inline_autocomplete(true);
      provider_->Start(input, false);
      EXPECT_TRUE(provider_->done());
      matches = provider_->matches();
      ASSERT_EQ(cases[i].output.size(), matches.size());
      for (size_t j = 0; j < cases[i].output.size(); ++j) {
        EXPECT_EQ(cases[i].output[j], matches[j].destination_url);
        EXPECT_FALSE(matches[j].allowed_to_be_default_match);
      }
    }
  }

  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<BuiltinProvider> provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BuiltinProviderTest);
};

TEST_F(BuiltinProviderTest, TypingScheme) {
  const base::string16 kAbout = ASCIIToUTF16(url::kAboutScheme);
  const base::string16 kEmbedder = ASCIIToUTF16(kEmbedderAboutScheme);
  const base::string16 kSeparator1 = ASCIIToUTF16(":");
  const base::string16 kSeparator2 = ASCIIToUTF16(":/");
  const base::string16 kSeparator3 =
      ASCIIToUTF16(url::kStandardSchemeSeparator);

  // These default URLs should correspond with those in BuiltinProvider::Start.
  const GURL kURL1(kDefaultURL1);
  const GURL kURL2(kDefaultURL2);
  const GURL kURL3(kDefaultURL3);

  TestData typing_scheme_cases[] = {
    // Typing an unrelated scheme should give nothing.
    {ASCIIToUTF16("h"),        {}},
    {ASCIIToUTF16("http"),     {}},
    {ASCIIToUTF16("file"),     {}},
    {ASCIIToUTF16("abouz"),    {}},
    {ASCIIToUTF16("aboutt"),   {}},
    {ASCIIToUTF16("aboutt:"),  {}},
    {ASCIIToUTF16("chroma"),   {}},
    {ASCIIToUTF16("chromee"),  {}},
    {ASCIIToUTF16("chromee:"), {}},

    // Typing a portion of about:// should give the default urls.
    {kAbout.substr(0, 1),      {kURL1, kURL2, kURL3}},
    {ASCIIToUTF16("A"),        {kURL1, kURL2, kURL3}},
    {kAbout,                   {kURL1, kURL2, kURL3}},
    {kAbout + kSeparator1,     {kURL1, kURL2, kURL3}},
    {kAbout + kSeparator2,     {kURL1, kURL2, kURL3}},
    {kAbout + kSeparator3,     {kURL1, kURL2, kURL3}},
    {ASCIIToUTF16("aBoUT://"), {kURL1, kURL2, kURL3}},

    // Typing a portion of the embedder scheme should give the default urls.
    {kEmbedder.substr(0, 1),    {kURL1, kURL2, kURL3}},
    {ASCIIToUTF16("C"),         {kURL1, kURL2, kURL3}},
    {kEmbedder,                 {kURL1, kURL2, kURL3}},
    {kEmbedder + kSeparator1,   {kURL1, kURL2, kURL3}},
    {kEmbedder + kSeparator2,   {kURL1, kURL2, kURL3}},
    {kEmbedder + kSeparator3,   {kURL1, kURL2, kURL3}},
    {ASCIIToUTF16("ChRoMe://"), {kURL1, kURL2, kURL3}},
  };

  RunTest(typing_scheme_cases, base::size(typing_scheme_cases));
}

TEST_F(BuiltinProviderTest, NonEmbedderURLs) {
  TestData test_cases[] = {
    // Typing an unrelated scheme should give nothing.
    {ASCIIToUTF16("g@rb@g3"),                      {}},
    {ASCIIToUTF16("www.google.com"),               {}},
    {ASCIIToUTF16("http:www.google.com"),          {}},
    {ASCIIToUTF16("http://www.google.com"),        {}},
    {ASCIIToUTF16("file:filename"),                {}},
    {ASCIIToUTF16("scheme:"),                      {}},
    {ASCIIToUTF16("scheme://"),                    {}},
    {ASCIIToUTF16("scheme://host"),                {}},
    {ASCIIToUTF16("scheme:host/path?query#ref"),   {}},
    {ASCIIToUTF16("scheme://host/path?query#ref"), {}},
  };

  RunTest(test_cases, base::size(test_cases));
}

TEST_F(BuiltinProviderTest, EmbedderProvidedURLs) {
  const base::string16 kAbout = ASCIIToUTF16(url::kAboutScheme);
  const base::string16 kEmbedder = ASCIIToUTF16(kEmbedderAboutScheme);
  const base::string16 kSep1 = ASCIIToUTF16(":");
  const base::string16 kSep2 = ASCIIToUTF16(":/");
  const base::string16 kSep3 =
      ASCIIToUTF16(url::kStandardSchemeSeparator);

  // The following hosts are arbitrary, chosen so that they all start with the
  // letters "me".
  const base::string16 kHostM1 = ASCIIToUTF16(kHostMedia);
  const base::string16 kHostM2 = ASCIIToUTF16(kHostMemoryInternals);
  const base::string16 kHostM3 = ASCIIToUTF16(kHostMemory);
  const GURL kURLM1(kEmbedder + kSep3 + kHostM1);
  const GURL kURLM2(kEmbedder + kSep3 + kHostM2);
  const GURL kURLM3(kEmbedder + kSep3 + kHostM3);

  TestData test_cases[] = {
    // Typing an about URL with an unknown host should give nothing.
    {kAbout + kSep1 + ASCIIToUTF16("host"), {}},
    {kAbout + kSep2 + ASCIIToUTF16("host"), {}},
    {kAbout + kSep3 + ASCIIToUTF16("host"), {}},

    // Typing an embedder URL with an unknown host should give nothing.
    {kEmbedder + kSep1 + ASCIIToUTF16("host"), {}},
    {kEmbedder + kSep2 + ASCIIToUTF16("host"), {}},
    {kEmbedder + kSep3 + ASCIIToUTF16("host"), {}},

    // Typing an about URL should provide matching URLs.
    {kAbout + kSep1 + kHostM1.substr(0, 1),    {kURLM1, kURLM2, kURLM3}},
    {kAbout + kSep2 + kHostM1.substr(0, 2),    {kURLM1, kURLM2, kURLM3}},
    {kAbout + kSep3 + kHostM1.substr(0, 3),    {kURLM1}},
    {kAbout + kSep3 + kHostM2.substr(0, 3),    {kURLM2, kURLM3}},
    {kAbout + kSep3 + kHostM1,                 {kURLM1}},
    {kAbout + kSep2 + kHostM2,                 {kURLM2}},
    {kAbout + kSep2 + kHostM3,                 {kURLM2, kURLM3}},

    // Typing an embedder URL should provide matching URLs.
    {kEmbedder + kSep1 + kHostM1.substr(0, 1), {kURLM1, kURLM2, kURLM3}},
    {kEmbedder + kSep2 + kHostM1.substr(0, 2), {kURLM1, kURLM2, kURLM3}},
    {kEmbedder + kSep3 + kHostM1.substr(0, 3), {kURLM1}},
    {kEmbedder + kSep3 + kHostM2.substr(0, 3), {kURLM2, kURLM3}},
    {kEmbedder + kSep3 + kHostM1,              {kURLM1}},
    {kEmbedder + kSep2 + kHostM2,              {kURLM2}},
    {kEmbedder + kSep2 + kHostM3,              {kURLM2, kURLM3}},
  };

  RunTest(test_cases, base::size(test_cases));
}

TEST_F(BuiltinProviderTest, AboutBlank) {
  const base::string16 kAbout = ASCIIToUTF16(url::kAboutScheme);
  const base::string16 kEmbedder = ASCIIToUTF16(kEmbedderAboutScheme);
  const base::string16 kAboutBlank = ASCIIToUTF16(url::kAboutBlankURL);
  const base::string16 kBlank = ASCIIToUTF16("blank");
  const base::string16 kSeparator1 =
      ASCIIToUTF16(url::kStandardSchemeSeparator);
  const base::string16 kSeparator2 = ASCIIToUTF16(":///");
  const base::string16 kSeparator3 = ASCIIToUTF16(";///");

  const GURL kURLBar =
      GURL(kEmbedder + kSeparator1 + ASCIIToUTF16(kHostBar));
  const GURL kURLBlank(kAboutBlank);

  TestData about_blank_cases[] = {
    // Typing an about:blank prefix should yield about:blank, among other URLs.
    {kAboutBlank.substr(0, 7), {kURLBlank, kURLBar}},
    {kAboutBlank.substr(0, 8), {kURLBlank}},

    // Using any separator that is supported by fixup should yield about:blank.
    // For now, BuiltinProvider does not suggest url-what-you-typed matches for
    // for about:blank; check "about:blan" and "about;blan" substrings instead.
    {kAbout + kSeparator2.substr(0, 1) + kBlank.substr(0, 4), {kURLBlank}},
    {kAbout + kSeparator2.substr(0, 2) + kBlank,              {kURLBlank}},
    {kAbout + kSeparator2.substr(0, 3) + kBlank,              {kURLBlank}},
    {kAbout + kSeparator2 + kBlank,                           {kURLBlank}},
    {kAbout + kSeparator3.substr(0, 1) + kBlank.substr(0, 4), {kURLBlank}},
    {kAbout + kSeparator3.substr(0, 2) + kBlank,              {kURLBlank}},
    {kAbout + kSeparator3.substr(0, 3) + kBlank,              {kURLBlank}},
    {kAbout + kSeparator3 + kBlank,                           {kURLBlank}},

    // Using the embedder scheme should not yield about:blank.
    {kEmbedder + kSeparator1.substr(0, 1) + kBlank, {}},
    {kEmbedder + kSeparator1.substr(0, 2) + kBlank, {}},
    {kEmbedder + kSeparator1.substr(0, 3) + kBlank, {}},
    {kEmbedder + kSeparator1 + kBlank,              {}},

    // Adding trailing text should not yield about:blank.
    {kAboutBlank + ASCIIToUTF16("/"),  {}},
    {kAboutBlank + ASCIIToUTF16("/p"), {}},
    {kAboutBlank + ASCIIToUTF16("x"),  {}},
    {kAboutBlank + ASCIIToUTF16("?q"), {}},
    {kAboutBlank + ASCIIToUTF16("#r"), {}},

    // Interrupting "blank" with conflicting text should not yield about:blank.
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("/"),  {}},
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("/p"), {}},
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("x"),  {}},
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("?q"), {}},
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("#r"), {}},
  };

  RunTest(about_blank_cases, base::size(about_blank_cases));
}

TEST_F(BuiltinProviderTest, DoesNotSupportMatchesOnFocus) {
  AutocompleteInput input(ASCIIToUTF16("chrome://m"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_from_omnibox_focus(true);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(BuiltinProviderTest, Subpages) {
  const base::string16 kSubpage = ASCIIToUTF16(kSubpageURL);
  const base::string16 kPageOne = ASCIIToUTF16(kSubpageOne);
  const base::string16 kPageTwo = ASCIIToUTF16(kSubpageTwo);
  const base::string16 kPageThree = ASCIIToUTF16(kSubpageThree);
  const GURL kURLOne(kSubpage + kPageOne);
  const GURL kURLTwo(kSubpage + kPageTwo);
  const GURL kURLThree(kSubpage + kPageThree);

  TestData settings_subpage_cases[] = {
    // Typing the settings path should show settings and the first two subpages.
    {kSubpage, {GURL(kSubpage), kURLOne, kURLTwo}},

    // Typing a subpage path should return the appropriate results.
    {kSubpage + kPageTwo.substr(0, 1),                 {kURLTwo, kURLThree}},
    {kSubpage + kPageTwo.substr(0, 2),                 {kURLTwo}},
    {kSubpage + kPageThree.substr(0, kPageThree.length() - 1),
                                                       {kURLThree}},
    {kSubpage + kPageOne,                              {kURLOne}},
    {kSubpage + kPageTwo,                              {kURLTwo}},
  };

  RunTest(settings_subpage_cases, base::size(settings_subpage_cases));
}

TEST_F(BuiltinProviderTest, Inlining) {
  const base::string16 kAbout = ASCIIToUTF16(url::kAboutScheme);
  const base::string16 kEmbedder = ASCIIToUTF16(kEmbedderAboutScheme);
  const base::string16 kSep = ASCIIToUTF16(url::kStandardSchemeSeparator);
  const base::string16 kHostM = ASCIIToUTF16(kHostMedia);
  const base::string16 kHostB = ASCIIToUTF16(kHostBar);
  const base::string16 kHostMem = ASCIIToUTF16(kHostMemory);
  const base::string16 kHostMemInt = ASCIIToUTF16(kHostMemoryInternals);
  const base::string16 kHostSub = ASCIIToUTF16(kHostSubpage);
  const base::string16 kHostSubTwo = ASCIIToUTF16(kHostSubpage) +
                                     ASCIIToUTF16("/") +
                                     ASCIIToUTF16(kSubpageTwo);

  struct InliningTestData {
    const base::string16 input;
    const base::string16 expected_inline_autocompletion;
  } cases[] = {
    // Typing along "about://media" should not yield an inline autocompletion
    // until the completion is unique.  We don't bother checking every single
    // character before the first "m" is typed.
    {kAbout.substr(0, 2),                 base::string16()},
    {kAbout,                              base::string16()},
    {kAbout + kSep,                       base::string16()},
    {kAbout + kSep + kHostM.substr(0, 1), base::string16()},
    {kAbout + kSep + kHostM.substr(0, 2), base::string16()},
    {kAbout + kSep + kHostM.substr(0, 3), kHostM.substr(3)},
    {kAbout + kSep + kHostM.substr(0, 4), kHostM.substr(4)},

    // Ditto with "chrome://media".
    {kEmbedder.substr(0, 2),                 base::string16()},
    {kEmbedder,                              base::string16()},
    {kEmbedder + kSep,                       base::string16()},
    {kEmbedder + kSep + kHostM.substr(0, 1), base::string16()},
    {kEmbedder + kSep + kHostM.substr(0, 2), base::string16()},
    {kEmbedder + kSep + kHostM.substr(0, 3), kHostM.substr(3)},
    {kEmbedder + kSep + kHostM.substr(0, 4), kHostM.substr(4)},

    // The same rules should apply to "about://bar" and "chrome://bar".
    // At the "a" from "bar" in "about://bar", Chrome should be willing to
    // start inlining.  (Before that it conflicts with about:blank.)  At
    // the "b" from "bar" in "chrome://bar", Chrome should be willing to
    // start inlining.  (There is no chrome://blank page.)
    {kAbout + kSep + kHostB.substr(0, 1),    base::string16()},
    {kAbout + kSep + kHostB.substr(0, 2),    kHostB.substr(2)},
    {kAbout + kSep + kHostB.substr(0, 3),    kHostB.substr(3)},
    {kEmbedder + kSep + kHostB.substr(0, 1), kHostB.substr(1)},
    {kEmbedder + kSep + kHostB.substr(0, 2), kHostB.substr(2)},
    {kEmbedder + kSep + kHostB.substr(0, 3), kHostB.substr(3)},

    // The same rules should apply to "about://memory" and "chrome://memory".
    // At the second "m", an inline autocompletion should be offered. Although
    // this could also be completed with "memory-internals", "memory" is shorter
    // and prefix of the other candidate, so it is preferred.
    {kAbout + kSep + kHostMem.substr(0, 1), base::string16()},
    {kAbout + kSep + kHostMem.substr(0, 2), base::string16()},
    {kAbout + kSep + kHostMem.substr(0, 3), kHostMem.substr(3)},
    {kAbout + kSep + kHostMem.substr(0, 4), kHostMem.substr(4)},
    {kEmbedder + kSep + kHostMem.substr(0, 1), base::string16()},
    {kEmbedder + kSep + kHostMem.substr(0, 2), base::string16()},
    {kEmbedder + kSep + kHostMem.substr(0, 3), kHostMem.substr(3)},
    {kEmbedder + kSep + kHostMem.substr(0, 4), kHostMem.substr(4)},

    // After "memory-", then "memory-internals" should be inlined.
    {kAbout + kSep + kHostMemInt.substr(0, 7), kHostMemInt.substr(7)},
    {kEmbedder + kSep + kHostMemInt.substr(0, 7), kHostMemInt.substr(7)},

    // Similarly, inline "about://subpage" and "chrome://subpage" even though
    // other, longer completions (e.g. "chrome://subpage/one") are available.
    {kAbout + kSep + kHostSub.substr(0, 1), kHostSub.substr(1)},
    {kAbout + kSep + kHostSub.substr(0, 2), kHostSub.substr(2)},
    {kAbout + kSep + kHostSub.substr(0, 3), kHostSub.substr(3)},
    {kEmbedder + kSep + kHostSub.substr(0, 1), kHostSub.substr(1)},
    {kEmbedder + kSep + kHostSub.substr(0, 2), kHostSub.substr(2)},
    {kEmbedder + kSep + kHostSub.substr(0, 3), kHostSub.substr(3)},

    // Once the user input distinctly matches a longer subpage
    // ("chrome://subpage/two"), inline that. This doesn't happen until the user
    // enters "w" so that it it can be distinguished from
    // "chrome://subpage/three".
    {kAbout + kSep + kHostSubTwo.substr(0, 8), base::string16()},
    {kAbout + kSep + kHostSubTwo.substr(0, 9), base::string16()},
    {kAbout + kSep + kHostSubTwo.substr(0, 10), kHostSubTwo.substr(10)},
    {kEmbedder + kSep + kHostSubTwo.substr(0, 8), base::string16()},
    {kEmbedder + kSep + kHostSubTwo.substr(0, 9), base::string16()},
    {kEmbedder + kSep + kHostSubTwo.substr(0, 10), kHostSubTwo.substr(10)},

    // Typing something non-match after an inline autocompletion should stop
    // the inline autocompletion from appearing.
    {kAbout + kSep + kHostB.substr(0, 2) + ASCIIToUTF16("/"), base::string16()},
    {kAbout + kSep + kHostB.substr(0, 2) + ASCIIToUTF16("a"), base::string16()},
    {kAbout + kSep + kHostB.substr(0, 2) + ASCIIToUTF16("+"), base::string16()},
  };

  ACMatches matches;
  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "case %" PRIuS ": %s", i, base::UTF16ToUTF8(cases[i].input).c_str()));
    AutocompleteInput input(cases[i].input, metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    matches = provider_->matches();
    if (cases[i].expected_inline_autocompletion.empty()) {
      // If we're not expecting an inline autocompletion, make sure that no
      // matches are allowed_to_be_default.
      for (size_t j = 0; j < matches.size(); ++j) {
        EXPECT_LT(matches[j].relevance,
                  HistoryURLProvider::kScoreForWhatYouTypedResult);
        EXPECT_FALSE(matches[j].allowed_to_be_default_match);
      }
    } else {
      // If we are expecting an inline autocompletion, confirm that one and only
      // one of the matches is marked as allowed_to_be_default and that its
      // inline autocompletion is equal to the expected inline autocompletion.
      ASSERT_FALSE(matches.empty());
      size_t default_match_index = matches.size();
      for (size_t j = 0; j < matches.size(); ++j) {
        // If we already found a match that is allowed_to_be_default, ensure
        // that subsequent matches are NOT marked as allowed_to_be_default.
        if (default_match_index < matches.size()) {
          ASSERT_FALSE(matches[j].allowed_to_be_default_match)
              << "Only one match should be allowed to be the default match.";
        } else if (matches[j].allowed_to_be_default_match) {
          default_match_index = j;
        }
      }
      ASSERT_LT(default_match_index, matches.size())
          << "One match should be marked as allowed to be default but none is.";
      EXPECT_GT(matches[default_match_index].relevance,
                HistoryURLProvider::kScoreForWhatYouTypedResult);
      EXPECT_EQ(cases[i].expected_inline_autocompletion,
                matches[default_match_index].inline_autocompletion);
    }
  }
}
