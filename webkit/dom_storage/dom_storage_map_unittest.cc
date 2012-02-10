// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/dom_storage/dom_storage_map.h"

namespace dom_storage {

TEST(DomStorageMapTest, DomStorageMapBasics) {
  const string16 kKey(ASCIIToUTF16("key"));
  const string16 kValue(ASCIIToUTF16("value"));
  const string16 kKey2(ASCIIToUTF16("key2"));
  const string16 kValue2(ASCIIToUTF16("value2"));

  scoped_refptr<DomStorageMap> map(new DomStorageMap);
  string16 old_value;
  NullableString16 old_nullable_value;
  ValuesMap swap;
  scoped_refptr<DomStorageMap> copy;

  // Check the behavior of an empty map.
  EXPECT_EQ(0u, map->Length());
  EXPECT_TRUE(map->Key(0).is_null());
  EXPECT_TRUE(map->Key(100).is_null());
  EXPECT_TRUE(map->GetItem(kKey).is_null());
  EXPECT_FALSE(map->RemoveItem(kKey, &old_value));
  copy = map->DeepCopy();
  EXPECT_EQ(0u, copy->Length());
  map->SwapValues(&swap);
  EXPECT_TRUE(swap.empty());

  // Check the behavior of a map containing some values.
  EXPECT_TRUE(map->SetItem(kKey, kValue, &old_nullable_value));
  EXPECT_TRUE(old_nullable_value.is_null());
  EXPECT_EQ(1u, map->Length());
  EXPECT_EQ(kKey, map->Key(0).string());
  EXPECT_TRUE(map->Key(1).is_null());
  EXPECT_EQ(kValue, map->GetItem(kKey).string());
  EXPECT_TRUE(map->GetItem(kKey2).is_null());
  EXPECT_TRUE(map->RemoveItem(kKey, &old_value));
  EXPECT_EQ(kValue, old_value);
  old_value.clear();

  EXPECT_TRUE(map->SetItem(kKey, kValue, &old_nullable_value));
  EXPECT_TRUE(map->SetItem(kKey2, kValue, &old_nullable_value));
  EXPECT_TRUE(old_nullable_value.is_null());
  EXPECT_TRUE(map->SetItem(kKey2, kValue2, &old_nullable_value));
  EXPECT_EQ(kValue, old_nullable_value.string());
  EXPECT_EQ(2u, map->Length());
  EXPECT_EQ(kKey, map->Key(0).string());
  EXPECT_EQ(kKey2, map->Key(1).string());
  EXPECT_EQ(kKey, map->Key(0).string());

  copy = map->DeepCopy();
  EXPECT_EQ(2u, copy->Length());
  EXPECT_EQ(kValue, copy->GetItem(kKey).string());
  EXPECT_EQ(kValue2, copy->GetItem(kKey2).string());
  EXPECT_EQ(kKey, copy->Key(0).string());
  EXPECT_EQ(kKey2, copy->Key(1).string());
  EXPECT_TRUE(copy->Key(2).is_null());

  map->SwapValues(&swap);
  EXPECT_EQ(2ul, swap.size());
  EXPECT_EQ(0u, map->Length());
}

}  // namespace dom_storage
