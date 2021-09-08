// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/grammar_service_client.h"

#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/grammar_checker.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class GrammarServiceClientTest : public testing::Test {
 public:
  GrammarServiceClientTest() = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(GrammarServiceClientTest, ReturnsEmptyResultWhenSpellCheckIsDiabled) {
  machine_learning::FakeServiceConnectionImpl fake_service_connection;
  machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  machine_learning::ServiceConnection::GetInstance()->Initialize();

  auto profile = std::make_unique<TestingProfile>();
  profile->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, false);
  profile->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, false);

  GrammarServiceClient client;
  base::RunLoop().RunUntilIdle();

  client.RequestTextCheck(
      profile.get(), u"cat",
      base::BindOnce(
          [](bool success, const std::vector<SpellCheckResult>& results) {
            EXPECT_FALSE(success);
            EXPECT_TRUE(results.empty());
          }));

  base::RunLoop().RunUntilIdle();
}

TEST_F(GrammarServiceClientTest, ParsesResults) {
  machine_learning::FakeServiceConnectionImpl fake_service_connection;
  machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  machine_learning::ServiceConnection::GetInstance()->Initialize();

  auto profile = std::make_unique<TestingProfile>();
  profile->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);
  profile->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, true);

  // Construct fake output
  machine_learning::mojom::GrammarCheckerResultPtr result =
      machine_learning::mojom::GrammarCheckerResult::New();
  result->status = machine_learning::mojom::GrammarCheckerResult::Status::OK;
  machine_learning::mojom::GrammarCheckerCandidatePtr candidate =
      machine_learning::mojom::GrammarCheckerCandidate::New();
  candidate->text = "fake output";
  candidate->score = 0.5f;
  machine_learning::mojom::GrammarCorrectionFragmentPtr fragment =
      machine_learning::mojom::GrammarCorrectionFragment::New();
  fragment->offset = 3;
  fragment->length = 5;
  fragment->replacement = "fake replacement";
  candidate->fragments.emplace_back(std::move(fragment));
  result->candidates.emplace_back(std::move(candidate));
  fake_service_connection.SetOutputGrammarCheckerResult(result);

  GrammarServiceClient client;
  base::RunLoop().RunUntilIdle();

  client.RequestTextCheck(
      profile.get(), u"fake input",
      base::BindOnce(
          [](bool success, const std::vector<SpellCheckResult>& results) {
            EXPECT_TRUE(success);
            ASSERT_EQ(results.size(), 1U);
            EXPECT_EQ(results[0].decoration, SpellCheckResult::GRAMMAR);
            EXPECT_EQ(results[0].location, 3);
            EXPECT_EQ(results[0].length, 5);
            ASSERT_EQ(results[0].replacements.size(), 1U);
            EXPECT_EQ(results[0].replacements[0], u"fake replacement");
          }));

  base::RunLoop().RunUntilIdle();
}

TEST_F(GrammarServiceClientTest, ParsesResultsForLongQuery) {
  machine_learning::FakeServiceConnectionImpl fake_service_connection;
  machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  machine_learning::ServiceConnection::GetInstance()->Initialize();

  auto profile = std::make_unique<TestingProfile>();
  profile->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);
  profile->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, true);

  // Construct fake output
  machine_learning::mojom::GrammarCheckerResultPtr result =
      machine_learning::mojom::GrammarCheckerResult::New();
  result->status = machine_learning::mojom::GrammarCheckerResult::Status::OK;
  machine_learning::mojom::GrammarCheckerCandidatePtr candidate =
      machine_learning::mojom::GrammarCheckerCandidate::New();
  candidate->text = "fake output";
  candidate->score = 0.5f;
  machine_learning::mojom::GrammarCorrectionFragmentPtr fragment =
      machine_learning::mojom::GrammarCorrectionFragment::New();
  fragment->offset = 3;
  fragment->length = 5;
  fragment->replacement = "fake replacement";
  candidate->fragments.emplace_back(std::move(fragment));
  result->candidates.emplace_back(std::move(candidate));
  fake_service_connection.SetOutputGrammarCheckerResult(result);

  GrammarServiceClient client;
  base::RunLoop().RunUntilIdle();

  const std::u16string long_text =
      u"This is a very very very very very very very very very very very very "
      "very very loooooooooooong sentence, indeed very very very very very "
      "very very very very very very very very very loooooooooooong. Followed "
      "by a fake input sentence.";

  client.RequestTextCheck(
      profile.get(), long_text,
      base::BindOnce(
          [](bool success, const std::vector<SpellCheckResult>& results) {
            EXPECT_TRUE(success);
            ASSERT_EQ(results.size(), 1U);
            EXPECT_EQ(results[0].decoration, SpellCheckResult::GRAMMAR);
            // The fake grammar check result is set to return offset=3, so here
            // getting the location as 203 verifies the trimming.
            EXPECT_EQ(results[0].location, 203);
            EXPECT_EQ(results[0].length, 5);
            ASSERT_EQ(results[0].replacements.size(), 1U);
            EXPECT_EQ(results[0].replacements[0], u"fake replacement");
          }));

  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace chromeos
