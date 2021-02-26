// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/maybe_owned.h"

#include <memory>
#include <utility>

#include "core/fxcrt/unowned_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fxcrt {
namespace {

class PseudoDeletable {
 public:
  explicit PseudoDeletable(int id, int* count_location)
      : id_(id), count_location_(count_location) {}
  ~PseudoDeletable() { ++(*count_location_); }
  int GetID() const { return id_; }

 private:
  int id_;
  int* count_location_;
};

}  // namespace

TEST(MaybeOwned, Null) {
  MaybeOwned<PseudoDeletable> ptr1;
  EXPECT_FALSE(ptr1.IsOwned());
  EXPECT_FALSE(ptr1);
  EXPECT_EQ(nullptr, ptr1.Get());

  MaybeOwned<PseudoDeletable> ptr2;
  EXPECT_TRUE(ptr1 == ptr2);
  EXPECT_FALSE(ptr1 != ptr2);
}

TEST(MaybeOwned, NotOwned) {
  int delete_count = 0;
  PseudoDeletable thing1(100, &delete_count);
  {
    MaybeOwned<PseudoDeletable> ptr(&thing1);
    EXPECT_FALSE(ptr.IsOwned());
    EXPECT_EQ(ptr.Get(), &thing1);
    EXPECT_EQ(100, ptr->GetID());
    EXPECT_TRUE(ptr == &thing1);
    EXPECT_FALSE(ptr != &thing1);

    MaybeOwned<PseudoDeletable> empty;
    EXPECT_FALSE(ptr == empty);
    EXPECT_TRUE(ptr != empty);
  }
  EXPECT_EQ(0, delete_count);

  delete_count = 0;
  PseudoDeletable thing2(200, &delete_count);
  {
    MaybeOwned<PseudoDeletable> ptr(&thing1);
    ptr = &thing2;
    EXPECT_FALSE(ptr.IsOwned());
    EXPECT_EQ(ptr.Get(), &thing2);
    EXPECT_EQ(200, ptr->GetID());
  }
  EXPECT_EQ(0, delete_count);

  delete_count = 0;
  int owned_delete_count = 0;
  {
    MaybeOwned<PseudoDeletable> ptr(&thing1);
    EXPECT_EQ(100, ptr->GetID());
    ptr = std::make_unique<PseudoDeletable>(300, &owned_delete_count);
    EXPECT_TRUE(ptr.IsOwned());
    EXPECT_EQ(300, ptr->GetID());
  }
  EXPECT_EQ(0, delete_count);
  EXPECT_EQ(1, owned_delete_count);
}

TEST(MaybeOwned, UnownedPtr) {
  int delete_count = 0;
  PseudoDeletable thing1(100, &delete_count);
  PseudoDeletable thing2(200, &delete_count);
  UnownedPtr<PseudoDeletable> unowned1(&thing1);
  UnownedPtr<PseudoDeletable> unowned2(&thing2);
  {
    MaybeOwned<PseudoDeletable> ptr1(unowned1);
    MaybeOwned<PseudoDeletable> ptr2(unowned2);
    ptr2 = unowned1;
    ptr1 = unowned2;
  }
  EXPECT_EQ(0, delete_count);
}

TEST(MaybeOwned, Owned) {
  int delete_count = 0;
  {
    MaybeOwned<PseudoDeletable> ptr(
        std::make_unique<PseudoDeletable>(100, &delete_count));
    EXPECT_TRUE(ptr.IsOwned());
    EXPECT_EQ(100, ptr->GetID());

    MaybeOwned<PseudoDeletable> empty;
    EXPECT_FALSE(ptr == empty);
    EXPECT_TRUE(ptr != empty);
  }
  EXPECT_EQ(1, delete_count);

  delete_count = 0;
  {
    MaybeOwned<PseudoDeletable> ptr(
        std::make_unique<PseudoDeletable>(200, &delete_count));
    ptr = std::make_unique<PseudoDeletable>(300, &delete_count);
    EXPECT_TRUE(ptr.IsOwned());
    EXPECT_EQ(300, ptr->GetID());
    EXPECT_EQ(1, delete_count);
  }
  EXPECT_EQ(2, delete_count);

  delete_count = 0;
  int unowned_delete_count = 0;
  PseudoDeletable thing2(400, &unowned_delete_count);
  {
    MaybeOwned<PseudoDeletable> ptr(
        std::make_unique<PseudoDeletable>(500, &delete_count));
    ptr = &thing2;
    EXPECT_FALSE(ptr.IsOwned());
    EXPECT_EQ(400, ptr->GetID());
    EXPECT_EQ(1, delete_count);
    EXPECT_EQ(0, unowned_delete_count);
  }
  EXPECT_EQ(1, delete_count);
  EXPECT_EQ(0, unowned_delete_count);
}

TEST(MaybeOwned, Release) {
  int delete_count = 0;
  {
    std::unique_ptr<PseudoDeletable> stolen;
    {
      MaybeOwned<PseudoDeletable> ptr(
          std::make_unique<PseudoDeletable>(100, &delete_count));
      EXPECT_TRUE(ptr.IsOwned());
      stolen = ptr.Release();
      EXPECT_FALSE(ptr.IsOwned());
      EXPECT_EQ(ptr, stolen);
      EXPECT_EQ(0, delete_count);
    }
    EXPECT_EQ(0, delete_count);
  }
  EXPECT_EQ(1, delete_count);
}

TEST(MaybeOwned, Move) {
  int delete_count = 0;
  PseudoDeletable thing1(100, &delete_count);
  {
    MaybeOwned<PseudoDeletable> ptr1(&thing1);
    MaybeOwned<PseudoDeletable> ptr2(
        std::make_unique<PseudoDeletable>(200, &delete_count));
    EXPECT_FALSE(ptr1.IsOwned());
    EXPECT_TRUE(ptr2.IsOwned());

    MaybeOwned<PseudoDeletable> ptr3(std::move(ptr1));
    MaybeOwned<PseudoDeletable> ptr4(std::move(ptr2));
    EXPECT_FALSE(ptr1.IsOwned());
    EXPECT_FALSE(ptr2.IsOwned());
    EXPECT_FALSE(ptr3.IsOwned());
    EXPECT_TRUE(ptr4.IsOwned());
    EXPECT_EQ(0, delete_count);
    EXPECT_EQ(nullptr, ptr1.Get());
    EXPECT_EQ(nullptr, ptr2.Get());
    EXPECT_EQ(100, ptr3->GetID());
    EXPECT_EQ(200, ptr4->GetID());

    MaybeOwned<PseudoDeletable> ptr5;
    MaybeOwned<PseudoDeletable> ptr6;
    ptr5 = std::move(ptr3);
    ptr6 = std::move(ptr4);
    EXPECT_FALSE(ptr3.IsOwned());
    EXPECT_FALSE(ptr4.IsOwned());
    EXPECT_FALSE(ptr5.IsOwned());
    EXPECT_TRUE(ptr6.IsOwned());
    EXPECT_EQ(0, delete_count);
    EXPECT_EQ(nullptr, ptr3.Get());
    EXPECT_EQ(nullptr, ptr4.Get());
    EXPECT_EQ(100, ptr5->GetID());
    EXPECT_EQ(200, ptr6->GetID());
  }
  EXPECT_EQ(1, delete_count);
}

}  // namespace fxcrt
