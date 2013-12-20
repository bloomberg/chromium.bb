// Copyright (C) 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rule_retriever.h"

#include <libaddressinput/callback.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <string>

#include <gtest/gtest.h>

#include "fake_downloader.h"
#include "fake_storage.h"
#include "retriever.h"
#include "rule.h"

namespace i18n {
namespace addressinput {

namespace {

// Tests for RuleRetriever object.
class RuleRetrieverTest : public testing::Test {
 protected:
  RuleRetrieverTest()
      : rule_retriever_(scoped_ptr<const Retriever>(new Retriever(
            FakeDownloader::kFakeDataUrl,
            scoped_ptr<const Downloader>(new FakeDownloader),
            scoped_ptr<Storage>(new FakeStorage)))),
        success_(false),
        key_(),
        rule_() {}

  virtual ~RuleRetrieverTest() {}

  scoped_ptr<RuleRetriever::Callback> BuildCallback() {
    return ::i18n::addressinput::BuildCallback(
        this, &RuleRetrieverTest::OnRuleReady);
  }

  RuleRetriever rule_retriever_;
  bool success_;
  std::string key_;
  Rule rule_;

 private:
  void OnRuleReady(bool success,
                   const std::string& key,
                   const Rule& rule) {
    success_ = success;
    key_ = key;
    rule_.CopyFrom(rule);
  }
};

TEST_F(RuleRetrieverTest, ExistingRule) {
  static const char kExistingKey[] = "data/CA";

  rule_retriever_.RetrieveRule(kExistingKey, BuildCallback());

  EXPECT_TRUE(success_);
  EXPECT_EQ(kExistingKey, key_);
  EXPECT_FALSE(rule_.GetFormat().empty());
}

TEST_F(RuleRetrieverTest, MissingRule) {
  static const char kMissingKey[] = "junk";

  rule_retriever_.RetrieveRule(kMissingKey, BuildCallback());

  EXPECT_TRUE(success_);  // The server returns "{}" for bad keys.
  EXPECT_EQ(kMissingKey, key_);
  EXPECT_TRUE(rule_.GetFormat().empty());
}

}  // namespace

}  // namespace addressinput
}  // namespace i18n
