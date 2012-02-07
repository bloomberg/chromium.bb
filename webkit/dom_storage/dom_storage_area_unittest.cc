// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/dom_storage/dom_storage_area.h"

namespace dom_storage {

TEST(DomStorageAreaTest, DomStorageAreaBasics) {
  const GURL kOrigin("http://dom_storage/");
  const string16 kKey(ASCIIToUTF16("key"));
  const string16 kValue(ASCIIToUTF16("value"));
  const string16 kKey2(ASCIIToUTF16("key2"));
  const string16 kValue2(ASCIIToUTF16("value2"));

  scoped_refptr<DomStorageArea> area(
      new DomStorageArea(1, kOrigin, FilePath(), NULL));
  string16 old_value;
  NullableString16 old_nullable_value;
  scoped_refptr<DomStorageArea> copy;

  // We don't focus on the underlying DomStorageMap functionality
  // since that's covered by seperate unit tests.
  EXPECT_EQ(kOrigin, area->origin());
  EXPECT_EQ(1, area->namespace_id());
  EXPECT_EQ(0u, area->Length());
  area->SetItem(kKey, kValue, &old_nullable_value);
  area->SetItem(kKey2, kValue2, &old_nullable_value);

  // Verify that a copy shares the same map.
  copy = area->ShallowCopy(2);
  EXPECT_EQ(kOrigin, copy->origin());
  EXPECT_EQ(2, copy->namespace_id());
  EXPECT_EQ(area->Length(), copy->Length());
  EXPECT_EQ(area->GetItem(kKey).string(), copy->GetItem(kKey).string());
  EXPECT_EQ(area->Key(0).string(), copy->Key(0).string());
  EXPECT_EQ(copy->map_.get(), area->map_.get());

  // But will deep copy-on-write as needed.
  EXPECT_TRUE(area->RemoveItem(kKey, &old_value));
  EXPECT_NE(copy->map_.get(), area->map_.get());
  copy = area->ShallowCopy(2);
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  EXPECT_TRUE(area->SetItem(kKey, kValue, &old_nullable_value));
  EXPECT_NE(copy->map_.get(), area->map_.get());
  copy = area->ShallowCopy(2);
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  EXPECT_NE(0u, area->Length());
  EXPECT_TRUE(area->Clear());
  EXPECT_EQ(0u, area->Length());
  EXPECT_NE(copy->map_.get(), area->map_.get());
}

}  // namespace dom_storage
