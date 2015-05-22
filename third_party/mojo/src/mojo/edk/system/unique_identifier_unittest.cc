// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/unique_identifier.h"

#include <set>
#include <sstream>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "mojo/edk/embedder/simple_platform_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

class UniqueIdentifierTest : public testing::Test {
 public:
  UniqueIdentifierTest() {}
  ~UniqueIdentifierTest() override {}

  embedder::PlatformSupport* platform_support() { return &platform_support_; }

 private:
  embedder::SimplePlatformSupport platform_support_;

  DISALLOW_COPY_AND_ASSIGN(UniqueIdentifierTest);
};

TEST_F(UniqueIdentifierTest, Basic) {
  // (This also checks copy constructibility.)
  UniqueIdentifier id1 = UniqueIdentifier::Generate(platform_support());

  EXPECT_EQ(id1, id1);
  EXPECT_FALSE(id1 != id1);
  EXPECT_FALSE(id1 < id1);

  UniqueIdentifier id2 = UniqueIdentifier::Generate(platform_support());

  EXPECT_FALSE(id2 == id1);
  EXPECT_NE(id2, id1);
  EXPECT_TRUE((id1 < id2) ^ (id2 < id1));

  // Test copyability.
  id2 = id1;
}

TEST_F(UniqueIdentifierTest, Logging) {
  std::ostringstream oss1;
  UniqueIdentifier id1 = UniqueIdentifier::Generate(platform_support());
  oss1 << id1;
  EXPECT_FALSE(oss1.str().empty());

  std::ostringstream oss2;
  UniqueIdentifier id2 = UniqueIdentifier::Generate(platform_support());
  oss2 << id2;
  EXPECT_FALSE(oss2.str().empty());

  EXPECT_NE(id1, id2);
  EXPECT_NE(oss1.str(), oss2.str());
}

TEST_F(UniqueIdentifierTest, StdSet) {
  std::set<UniqueIdentifier> s;
  EXPECT_TRUE(s.empty());

  UniqueIdentifier id1 = UniqueIdentifier::Generate(platform_support());
  EXPECT_TRUE(s.find(id1) == s.end());
  s.insert(id1);
  EXPECT_TRUE(s.find(id1) != s.end());
  EXPECT_FALSE(s.empty());

  UniqueIdentifier id2 = UniqueIdentifier::Generate(platform_support());
  EXPECT_TRUE(s.find(id2) == s.end());
  s.insert(id2);
  EXPECT_TRUE(s.find(id2) != s.end());
  // Make sure |id1| is still in |s|.
  EXPECT_TRUE(s.find(id1) != s.end());

  s.erase(id1);
  EXPECT_TRUE(s.find(id1) == s.end());
  // Make sure |id2| is still in |s|.
  EXPECT_TRUE(s.find(id2) != s.end());

  s.erase(id2);
  EXPECT_TRUE(s.find(id2) == s.end());
  EXPECT_TRUE(s.empty());
}

TEST_F(UniqueIdentifierTest, HashSet) {
  base::hash_set<UniqueIdentifier> s;
  EXPECT_TRUE(s.empty());

  UniqueIdentifier id1 = UniqueIdentifier::Generate(platform_support());
  EXPECT_TRUE(s.find(id1) == s.end());
  s.insert(id1);
  EXPECT_TRUE(s.find(id1) != s.end());
  EXPECT_FALSE(s.empty());

  UniqueIdentifier id2 = UniqueIdentifier::Generate(platform_support());
  EXPECT_TRUE(s.find(id2) == s.end());
  s.insert(id2);
  EXPECT_TRUE(s.find(id2) != s.end());
  // Make sure |id1| is still in |s|.
  EXPECT_TRUE(s.find(id1) != s.end());

  s.erase(id1);
  EXPECT_TRUE(s.find(id1) == s.end());
  // Make sure |id2| is still in |s|.
  EXPECT_TRUE(s.find(id2) != s.end());

  s.erase(id2);
  EXPECT_TRUE(s.find(id2) == s.end());
  EXPECT_TRUE(s.empty());
}

}  // namespace
}  // namespace system
}  // namespace mojo
