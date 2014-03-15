// Copyright (C) 2014 Google Inc.
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

#include "util/trie.h"

#include <set>
#include <string>

#include <gtest/gtest.h>

namespace i18n {
namespace addressinput {

namespace {

TEST(TrieTest, EmptyTrieHasNoData) {
  Trie<std::string> trie;
  std::set<std::string> result;
  trie.FindDataForKeyPrefix("key", &result);
  EXPECT_TRUE(result.empty());
}

TEST(TrieTest, CanGetDataByExactKey) {
  Trie<std::string> trie;
  trie.AddDataForKey("hello", "world");
  std::set<std::string> result;
  trie.FindDataForKeyPrefix("hello", &result);
  std::set<std::string> expected;
  expected.insert("world");
  EXPECT_EQ(expected, result);
}

TEST(TrieTest, CanGetDataByPrefix) {
  Trie<std::string> trie;
  trie.AddDataForKey("hello", "world");
  std::set<std::string> result;
  trie.FindDataForKeyPrefix("he", &result);
  std::set<std::string> expected;
  expected.insert("world");
  EXPECT_EQ(expected, result);
}

TEST(TrieTest, KeyTooLongNoData) {
  Trie<std::string> trie;
  trie.AddDataForKey("hello", "world");
  std::set<std::string> result;
  trie.FindDataForKeyPrefix("helloo", &result);
  EXPECT_TRUE(result.empty());
}

TEST(TrieTest, CommonPrefixFindsMultipleData) {
  Trie<std::string> trie;
  trie.AddDataForKey("hello", "world");
  trie.AddDataForKey("howdy", "buddy");
  trie.AddDataForKey("foo", "bar");
  std::set<std::string> results;
  trie.FindDataForKeyPrefix("h", &results);
  std::set<std::string> expected;
  expected.insert("world");
  expected.insert("buddy");
  EXPECT_EQ(expected, results);
}

TEST(TrieTest, KeyCanBePrefixOfOtherKey) {
  Trie<std::string> trie;
  trie.AddDataForKey("hello", "world");
  trie.AddDataForKey("helloo", "woorld");
  trie.AddDataForKey("hella", "warld");
  std::set<std::string> results;
  trie.FindDataForKeyPrefix("hello", &results);
  std::set<std::string> expected;
  expected.insert("world");
  expected.insert("woorld");
  EXPECT_EQ(expected, results);
}

TEST(TrieTest, AllowMutlipleKeys) {
  Trie<std::string> trie;
  trie.AddDataForKey("hello", "world");
  trie.AddDataForKey("hello", "woorld");
  std::set<std::string> results;
  trie.FindDataForKeyPrefix("hello", &results);
  std::set<std::string> expected;
  expected.insert("world");
  expected.insert("woorld");
  EXPECT_EQ(expected, results);
}

TEST(TrieTest, CanFindVeryLongKey) {
  Trie<std::string> trie;
  static const char kVeryLongKey[] = "1234567890qwertyuioasdfghj";
  trie.AddDataForKey(kVeryLongKey, "world");
  std::set<std::string> result;
  trie.FindDataForKeyPrefix(kVeryLongKey, &result);
  std::set<std::string> expected;
  expected.insert("world");
  EXPECT_EQ(expected, result);
}

}  // namespace

}  // namespace addressinput
}  // namespace i18n
