// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "storage/common/database/database_connections.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace storage {

TEST(DatabaseConnectionsTest, DatabaseConnectionsTest) {
  const std::string kOriginId("origin_id");
  const base::string16 kName(ASCIIToUTF16("database_name"));
  const base::string16 kName2(ASCIIToUTF16("database_name2"));
  const int64_t kSize = 1000;

  DatabaseConnections connections;

  EXPECT_TRUE(connections.IsEmpty());
  EXPECT_FALSE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_FALSE(connections.IsOriginUsed(kOriginId));

  connections.AddConnection(kOriginId, kName);
  EXPECT_FALSE(connections.IsEmpty());
  EXPECT_TRUE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_TRUE(connections.IsOriginUsed(kOriginId));
  EXPECT_EQ(0, connections.GetOpenDatabaseSize(kOriginId, kName));
  connections.SetOpenDatabaseSize(kOriginId, kName, kSize);
  EXPECT_EQ(kSize, connections.GetOpenDatabaseSize(kOriginId, kName));

  connections.RemoveConnection(kOriginId, kName);
  EXPECT_TRUE(connections.IsEmpty());
  EXPECT_FALSE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_FALSE(connections.IsOriginUsed(kOriginId));

  connections.AddConnection(kOriginId, kName);
  connections.SetOpenDatabaseSize(kOriginId, kName, kSize);
  EXPECT_EQ(kSize, connections.GetOpenDatabaseSize(kOriginId, kName));
  connections.AddConnection(kOriginId, kName);
  EXPECT_EQ(kSize, connections.GetOpenDatabaseSize(kOriginId, kName));
  EXPECT_FALSE(connections.IsEmpty());
  EXPECT_TRUE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_TRUE(connections.IsOriginUsed(kOriginId));
  connections.AddConnection(kOriginId, kName2);
  EXPECT_TRUE(connections.IsDatabaseOpened(kOriginId, kName2));

  DatabaseConnections another;
  another.AddConnection(kOriginId, kName);
  another.AddConnection(kOriginId, kName2);

  std::vector<std::pair<std::string, base::string16>> closed_dbs =
      connections.RemoveConnections(another);
  EXPECT_EQ(1u, closed_dbs.size());
  EXPECT_EQ(kOriginId, closed_dbs[0].first);
  EXPECT_EQ(kName2, closed_dbs[0].second);
  EXPECT_FALSE(connections.IsDatabaseOpened(kOriginId, kName2));
  EXPECT_TRUE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_EQ(kSize, connections.GetOpenDatabaseSize(kOriginId, kName));
  another.RemoveAllConnections();
  connections.RemoveAllConnections();
  EXPECT_TRUE(connections.IsEmpty());

  // Ensure the return value properly indicates the initial
  // addition and final removal.
  EXPECT_TRUE(connections.AddConnection(kOriginId, kName));
  EXPECT_FALSE(connections.AddConnection(kOriginId, kName));
  EXPECT_FALSE(connections.AddConnection(kOriginId, kName));
  EXPECT_FALSE(connections.RemoveConnection(kOriginId, kName));
  EXPECT_FALSE(connections.RemoveConnection(kOriginId, kName));
  EXPECT_TRUE(connections.RemoveConnection(kOriginId, kName));
}

}  // namespace storage
