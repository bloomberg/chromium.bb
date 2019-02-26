// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/us.h"
#include "chromeos/services/ime/public/cpp/rulebased/engine.h"
#include "chromeos/services/ime/public/cpp/rulebased/rules_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace ime {

namespace {

struct KeyVerifyEntry {
  const char* key;
  uint8_t modifiers;
  const char* expected_commit_text;
  const char* expected_composition_text;
};

}  // namespace

class RulebasedImeTest : public testing::Test {
 protected:
  RulebasedImeTest() = default;
  ~RulebasedImeTest() override = default;

  // testing::Test:
  void SetUp() override { engine_.reset(new rulebased::Engine); }

  void VerifyKeys(std::vector<KeyVerifyEntry> entries) {
    for (auto entry : entries) {
      rulebased::ProcessKeyResult res =
          engine_->ProcessKey(entry.key, entry.modifiers);
      EXPECT_TRUE(res.key_handled);
      EXPECT_EQ(entry.expected_commit_text, res.commit_text);
      EXPECT_EQ(entry.expected_composition_text, res.composition_text);
    }
  }

  std::unique_ptr<rulebased::Engine> engine_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RulebasedImeTest);
};

TEST_F(RulebasedImeTest, Arabic) {
  engine_->Activate("ar");
  std::vector<KeyVerifyEntry> entries;
  entries.push_back({"KeyA", rulebased::MODIFIER_SHIFT, u8"\u0650", ""});
  entries.push_back({"KeyB", 0, u8"\u0644\u0627", ""});
  entries.push_back({"Space", 0, " ", ""});
  VerifyKeys(entries);
}

TEST_F(RulebasedImeTest, Persian) {
  engine_->Activate("fa");
  std::vector<KeyVerifyEntry> entries;
  entries.push_back({"KeyA", 0, u8"\u0634", ""});
  entries.push_back({"KeyV", rulebased::MODIFIER_SHIFT, "", ""});
  entries.push_back({"Space", rulebased::MODIFIER_SHIFT, u8"\u200c", ""});
  VerifyKeys(entries);
}

TEST_F(RulebasedImeTest, Thai) {
  engine_->Activate("th");
  std::vector<KeyVerifyEntry> entries;
  entries.push_back({"KeyA", 0, u8"\u0e1f", ""});
  entries.push_back({"KeyA", rulebased::MODIFIER_ALTGR, "", ""});
  VerifyKeys(entries);

  engine_->Activate("th_pattajoti");
  entries.clear();
  entries.push_back({"KeyA", 0, u8"\u0e49", ""});
  entries.push_back({"KeyB", rulebased::MODIFIER_SHIFT, u8"\u0e31\u0e49", ""});
  VerifyKeys(entries);

  engine_->Activate("th_tis");
  entries.clear();
  entries.push_back({"KeyA", 0, u8"\u0e1f", ""});
  entries.push_back({"KeyM", rulebased::MODIFIER_SHIFT, u8"?", ""});
  VerifyKeys(entries);
}

TEST_F(RulebasedImeTest, DevaPhone) {
  engine_->Activate("deva_phone");
  std::vector<KeyVerifyEntry> entries;
  // "njnchh" -> "\u091e\u094d\u091c\u091e\u094d\u091b".
  entries.push_back({"KeyN", 0, "", u8"\u0928"});
  entries.push_back({"KeyJ", 0, "", u8"\u091e\u094d\u091c"});
  entries.push_back({"KeyN", 0, "", u8"\u091e\u094d\u091c\u094d\u091e"});
  entries.push_back({"KeyC", 0, "", u8"\u091e\u094d\u091c\u091e\u094d\u091a"});
  entries.push_back({"KeyH", 0, "", u8"\u091e\u094d\u091c\u091e\u094d\u091a"});
  entries.push_back({"KeyH", 0, "", u8"\u091e\u094d\u091c\u091e\u094d\u091b"});
  entries.push_back({"Backspace", 0, "", u8"\u091e\u094d\u091c\u091e\u094d"});
  entries.push_back({"Space", 0, u8"\u091e\u094d\u091c\u091e\u094d ", ""});
  VerifyKeys(entries);
}

TEST_F(RulebasedImeTest, DevaPhone_Backspace) {
  engine_->Activate("deva_phone");
  std::vector<KeyVerifyEntry> entries;
  entries.push_back({"KeyN", 0, "", u8"\u0928"});
  entries.push_back({"Backspace", 0, "", u8""});

  entries.push_back({"KeyN", 0, "", u8"\u0928"});
  entries.push_back({"KeyC", 0, "", u8"\u091e\u094d\u091a"});
  entries.push_back({"Backspace", 0, "", u8"\u091e\u094d"});

  entries.push_back({"KeyC", 0, "", u8"\u091e\u094d\u091a"});
  entries.push_back({"KeyH", 0, "", u8"\u091e\u094d\u091a"});
  entries.push_back({"Backspace", 0, "", u8"\u091e\u094d"});

  entries.push_back({"KeyH", 0, "", u8"\u091e\u094d\u091a"});
  entries.push_back({"Backspace", 0, "", u8"\u091e\u094d"});
  entries.push_back({"Backspace", 0, "", u8"\u091e"});
  entries.push_back({"Backspace", 0, "", u8""});
  VerifyKeys(entries);
}

TEST_F(RulebasedImeTest, DevaPhone_Enter) {
  engine_->Activate("deva_phone");
  std::vector<KeyVerifyEntry> entries;
  entries.push_back({"KeyN", 0, "", u8"\u0928"});
  VerifyKeys(entries);
  rulebased::ProcessKeyResult res = engine_->ProcessKey("Enter", 0);
  EXPECT_FALSE(res.key_handled);
  EXPECT_EQ(u8"\u0928", res.commit_text);
  EXPECT_EQ("", res.composition_text);
}

TEST_F(RulebasedImeTest, DevaPhone_Reset) {
  engine_->Activate("deva_phone");
  std::vector<KeyVerifyEntry> entries;
  entries.push_back({"KeyN", 0, "", u8"\u0928"});
  VerifyKeys(entries);

  engine_->Reset();
  entries.clear();
  entries.push_back({"KeyC", 0, "", u8"\u091a"});
  VerifyKeys(entries);
}

TEST_F(RulebasedImeTest, Transforms) {
  const char* transforms[] = {
      u8"a", u8"x", u8"x\u001dAA", u8"X", u8"(\\w)(\\w)[<>]", u8"\\2\\1"};
  auto data = rulebased::RulesData::Create(us::kKeyMap, false, transforms,
                                           base::size(transforms), nullptr);
  std::string transformed;
  bool res = data->Transform("..", -1, "b", &transformed);
  EXPECT_FALSE(res);
  res = data->Transform("..", -1, "a", &transformed);
  EXPECT_TRUE(res);
  EXPECT_EQ("..x", transformed);
  res = data->Transform("..xA", 3, "A", &transformed);
  EXPECT_TRUE(res);
  EXPECT_EQ("..X", transformed);
  res = data->Transform("..ab", -1, "<", &transformed);
  EXPECT_TRUE(res);
  EXPECT_EQ("..ba", transformed);
}

TEST_F(RulebasedImeTest, PredictTransform) {
  const char* transforms[] = {
      u8"10",        u8"A",           u8"([aeou])\u001d?`",
      u8"\\1\u0300", u8"[\\[\\]]{2}", u8"ʘ"};
  auto data = rulebased::RulesData::Create(us::kKeyMap, false, transforms,
                                           base::size(transforms), nullptr);
  bool res = data->PredictTransform("..x", -1);
  EXPECT_FALSE(res);
  res = data->PredictTransform(u8"..0", -1);
  EXPECT_FALSE(res);
  res = data->PredictTransform(u8"..1", -1);
  EXPECT_TRUE(res);
  res = data->PredictTransform(u8"..100", -1);
  EXPECT_FALSE(res);

  res = data->PredictTransform(u8"..a", -1);
  EXPECT_TRUE(res);
  res = data->PredictTransform(u8"..a\u001d", -1);
  EXPECT_TRUE(res);
  res = data->PredictTransform(u8"..a\u001d`", -1);
  EXPECT_TRUE(res);
  res = data->PredictTransform(u8"..a`", -1);
  EXPECT_TRUE(res);
  res = data->PredictTransform(u8"..a``", -1);

  EXPECT_FALSE(res);
  res = data->PredictTransform(u8"..[", -1);
  EXPECT_TRUE(res);
  res = data->PredictTransform(u8"..]", -1);
  EXPECT_TRUE(res);
  res = data->PredictTransform(u8"..[]", -1);
  EXPECT_TRUE(res);
  res = data->PredictTransform(u8"..][", -1);
  EXPECT_TRUE(res);
  res = data->PredictTransform(u8"..[][", -1);
  EXPECT_TRUE(res);
  res = data->PredictTransform(u8"..[][][", -1);
  EXPECT_TRUE(res);
  res = data->PredictTransform(u8"..[][][)", -1);
  EXPECT_FALSE(res);
}

TEST_F(RulebasedImeTest, Transforms_deva_phone) {
  auto data = rulebased::RulesData::GetById("deva_phone");
  std::string transformed;
  // No match.
  bool res = data->Transform("..", -1, "?", &transformed);
  EXPECT_FALSE(res);
  // 1st rule.
  res = data->Transform("..", -1, "0", &transformed);
  EXPECT_TRUE(res);
  EXPECT_EQ(u8"..\u0966", transformed);
  // Last rule.
  res = data->Transform(u8"\u0964", -1, ".", &transformed);
  EXPECT_TRUE(res);
  EXPECT_EQ(u8"\u2026", transformed);
  // Rule with "\\1".
  std::string str(u8"..\u0915");
  int transat = str.length();
  res = data->Transform(str + "a", transat, "u", &transformed);
  EXPECT_TRUE(res);
  EXPECT_EQ(u8"..\u0915\u094c", transformed);
}

}  // namespace ime
}  // namespace chromeos
