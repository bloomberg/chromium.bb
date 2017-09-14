// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintChunk.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "platform/wtf/Optional.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(PaintChunkTest, matchesSame) {
  PaintChunkProperties properties;
  FakeDisplayItemClient client;
  client.UpdateCacheGeneration();
  DisplayItem::Id id(client, DisplayItem::kDrawingFirst);
  EXPECT_TRUE(PaintChunk(0, 1, id, properties)
                  .Matches(PaintChunk(0, 1, id, properties)));
}

TEST(PaintChunkTest, matchesEqual) {
  PaintChunkProperties properties;
  FakeDisplayItemClient client;
  client.UpdateCacheGeneration();
  DisplayItem::Id id(client, DisplayItem::kDrawingFirst);
  DisplayItem::Id id_equal = id;
  EXPECT_TRUE(PaintChunk(0, 1, id, properties)
                  .Matches(PaintChunk(0, 1, id_equal, properties)));
  EXPECT_TRUE(PaintChunk(0, 1, id_equal, properties)
                  .Matches(PaintChunk(0, 1, id, properties)));
}

TEST(PaintChunkTest, IdNotMatches) {
  PaintChunkProperties properties;
  FakeDisplayItemClient client1;
  client1.UpdateCacheGeneration();
  DisplayItem::Id id1(client1, DisplayItem::kDrawingFirst);

  FakeDisplayItemClient client2;
  client2.UpdateCacheGeneration();
  DisplayItem::Id id2(client2, DisplayItem::kDrawingFirst);
  EXPECT_FALSE(PaintChunk(0, 1, id2, properties)
                   .Matches(PaintChunk(0, 1, id1, properties)));
}

TEST(PaintChunkTest, IdNotMatchesUncacheable) {
  PaintChunkProperties properties;
  FakeDisplayItemClient client;
  client.UpdateCacheGeneration();
  DisplayItem::Id id(client, DisplayItem::kDrawingFirst);
  EXPECT_FALSE(PaintChunk(0, 1, id, properties, PaintChunk::kUncacheable)
                   .Matches(PaintChunk(0, 1, id, properties)));
  EXPECT_FALSE(
      PaintChunk(0, 1, id, properties)
          .Matches(PaintChunk(0, 1, id, properties, PaintChunk::kUncacheable)));
  EXPECT_FALSE(
      PaintChunk(0, 1, id, properties, PaintChunk::kUncacheable)
          .Matches(PaintChunk(0, 1, id, properties, PaintChunk::kUncacheable)));
}

TEST(PaintChunkTest, IdNotMatchesJustCreated) {
  PaintChunkProperties properties;
  Optional<FakeDisplayItemClient> client;
  client.emplace();
  EXPECT_TRUE(client->IsJustCreated());
  // Invalidation won't change the "just created" status.
  client->SetDisplayItemsUncached();
  EXPECT_TRUE(client->IsJustCreated());

  DisplayItem::Id id(*client, DisplayItem::kDrawingFirst);
  // A chunk of a newly created client doesn't match any chunk because it's
  // never cached.
  EXPECT_FALSE(PaintChunk(0, 1, id, properties)
                   .Matches(PaintChunk(0, 1, id, properties)));

  client->UpdateCacheGeneration();
  EXPECT_TRUE(PaintChunk(0, 1, id, properties)
                  .Matches(PaintChunk(0, 1, id, properties)));

  // Delete the current object and create a new object at the same address.
  client = WTF::nullopt;
  client.emplace();
  EXPECT_TRUE(client->IsJustCreated());
  EXPECT_FALSE(PaintChunk(0, 1, id, properties)
                   .Matches(PaintChunk(0, 1, id, properties)));
}

}  // namespace blink
