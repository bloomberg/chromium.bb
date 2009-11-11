// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/np_utils/np_class.h"
#include "o3d/gpu_plugin/np_utils/np_object_mock.h"
#include "o3d/gpu_plugin/np_utils/np_browser_stub.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::StrictMock;

namespace gpu_plugin {

class DerivedNPObject : public MockNPObject {
 public:
  explicit DerivedNPObject(NPP npp) : MockNPObject(npp) {
  }
};

class NPObjectPointerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    np_class_ = NPGetClass<StrictMock<MockNPObject> >();

    raw_pointer_ = static_cast<MockNPObject*>(
      NPBrowser::get()->CreateObject(NULL, np_class_));

    raw_derived_pointer_ = static_cast<DerivedNPObject*>(
      NPBrowser::get()->CreateObject(NULL, np_class_));
  }

  virtual void TearDown() {
    NPBrowser::get()->ReleaseObject(raw_pointer_);
    NPBrowser::get()->ReleaseObject(raw_derived_pointer_);
  }

  StubNPBrowser stub_browser_;
  const NPClass* np_class_;
  MockNPObject* raw_pointer_;
  DerivedNPObject* raw_derived_pointer_;
};

TEST_F(NPObjectPointerTest, PointerIsNullByDefault) {
  NPObjectPointer<MockNPObject> p;
  ASSERT_TRUE(NULL == p.Get());
}

TEST_F(NPObjectPointerTest, PointerCanBeExplicitlyConstructedFromRawPointer) {
  EXPECT_EQ(1, raw_pointer_->referenceCount);
  {
    NPObjectPointer<MockNPObject> p(raw_pointer_);
    ASSERT_TRUE(raw_pointer_ == p.Get());
    EXPECT_EQ(2, raw_pointer_->referenceCount);
  }
  EXPECT_EQ(1, raw_pointer_->referenceCount);
}

TEST_F(NPObjectPointerTest,
    PointerCanBeExplicitlyConstructedFromNullRawPointer) {
  NPObjectPointer<MockNPObject> p(NULL);
  ASSERT_TRUE(NULL == p.Get());
}

TEST_F(NPObjectPointerTest, PointerCanBeCopyConstructed) {
  NPObjectPointer<MockNPObject> p1(raw_pointer_);
  EXPECT_EQ(2, raw_pointer_->referenceCount);
  {
    NPObjectPointer<MockNPObject> p2(p1);
    ASSERT_TRUE(raw_pointer_ == p2.Get());
    EXPECT_EQ(3, raw_pointer_->referenceCount);
  }
  EXPECT_EQ(2, raw_pointer_->referenceCount);
}

TEST_F(NPObjectPointerTest, PointerCanBeConstructedFromDerived) {
  NPObjectPointer<DerivedNPObject> p1(raw_derived_pointer_);
  EXPECT_EQ(2, raw_derived_pointer_->referenceCount);
  {
    NPObjectPointer<MockNPObject> p2(p1);
    ASSERT_TRUE(raw_derived_pointer_ == p2.Get());
    EXPECT_EQ(3, raw_derived_pointer_->referenceCount);
  }
  EXPECT_EQ(2, raw_derived_pointer_->referenceCount);
}

TEST_F(NPObjectPointerTest,
    PointerCanBeCopyConstructedFromNull) {
  NPObjectPointer<MockNPObject> p(NULL);
  ASSERT_TRUE(NULL == p.Get());
}

TEST_F(NPObjectPointerTest, PointerCanBeAssigned) {
  NPObjectPointer<MockNPObject> p1(raw_pointer_);
  EXPECT_EQ(2, raw_pointer_->referenceCount);
  {
    NPObjectPointer<MockNPObject> p2;
    p2 = p1;
    ASSERT_TRUE(raw_pointer_ == p2.Get());
    EXPECT_EQ(3, raw_pointer_->referenceCount);

    p2 = NPObjectPointer<MockNPObject>();
    ASSERT_TRUE(NULL == p2.Get());
    EXPECT_EQ(2, raw_pointer_->referenceCount);

    p2 = p1;
    ASSERT_TRUE(raw_pointer_ == p2.Get());
    EXPECT_EQ(3, raw_pointer_->referenceCount);
  }
  EXPECT_EQ(2, raw_pointer_->referenceCount);
}

TEST_F(NPObjectPointerTest, PointerCanBeAssignedToSelf) {
  NPObjectPointer<MockNPObject> p(raw_pointer_);
  NPBrowser::get()->ReleaseObject(raw_pointer_);
  EXPECT_EQ(1, raw_pointer_->referenceCount);
  p = p;
  EXPECT_EQ(1, raw_pointer_->referenceCount);
  NPBrowser::get()->RetainObject(raw_pointer_);
}

TEST_F(NPObjectPointerTest, PointerCanBeAssignedDerived) {
  NPObjectPointer<DerivedNPObject> p1(raw_derived_pointer_);
  EXPECT_EQ(2, raw_derived_pointer_->referenceCount);
  {
    NPObjectPointer<MockNPObject> p2;
    p2 = p1;
    ASSERT_TRUE(raw_derived_pointer_ == p2.Get());
    EXPECT_EQ(3, raw_derived_pointer_->referenceCount);

    p2 = NPObjectPointer<MockNPObject>();
    ASSERT_TRUE(NULL == p2.Get());
    EXPECT_EQ(2, raw_derived_pointer_->referenceCount);

    p2 = p1;
    ASSERT_TRUE(raw_derived_pointer_ == p2.Get());
    EXPECT_EQ(3, raw_derived_pointer_->referenceCount);
  }
  EXPECT_EQ(2, raw_derived_pointer_->referenceCount);
}

TEST_F(NPObjectPointerTest, DerivedPointerCanBeAssignedToSelf) {
  NPObjectPointer<MockNPObject> p1(raw_derived_pointer_);
  NPObjectPointer<DerivedNPObject> p2(raw_derived_pointer_);
  NPBrowser::get()->ReleaseObject(raw_derived_pointer_);
  NPBrowser::get()->ReleaseObject(raw_derived_pointer_);
  EXPECT_EQ(1, raw_derived_pointer_->referenceCount);
  p1 = p2;
  EXPECT_EQ(1, raw_derived_pointer_->referenceCount);
  NPBrowser::get()->RetainObject(raw_derived_pointer_);
  NPBrowser::get()->RetainObject(raw_derived_pointer_);
}

TEST_F(NPObjectPointerTest, CanComparePointersForEqual) {
  NPObjectPointer<MockNPObject> p1(raw_pointer_);
  NPObjectPointer<DerivedNPObject> p2(raw_derived_pointer_);
  EXPECT_TRUE(p1 == p1);
  EXPECT_FALSE(p1 == p2);
  EXPECT_FALSE(p2 == p1);
  EXPECT_FALSE(p1 == NPObjectPointer<MockNPObject>());
}

TEST_F(NPObjectPointerTest, CanComparePointersForNotEqual) {
  NPObjectPointer<MockNPObject> p1(raw_pointer_);
  NPObjectPointer<DerivedNPObject> p2(raw_derived_pointer_);
  EXPECT_FALSE(p1 != p1);
  EXPECT_TRUE(p1 != p2);
  EXPECT_TRUE(p2 != p1);
  EXPECT_TRUE(p1 != NPObjectPointer<MockNPObject>());
}

TEST_F(NPObjectPointerTest, ArrowOperatorCanBeUsedToAccessNPObjectMembers) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("hello");

  EXPECT_CALL(*raw_pointer_, HasProperty(name)).WillOnce(Return(true));

  NPObjectPointer<MockNPObject> p(raw_pointer_);
  EXPECT_TRUE(p->HasProperty(name));
}

TEST_F(NPObjectPointerTest, StarOperatorReturnsNPObjectReference) {
  NPObjectPointer<MockNPObject> p(raw_pointer_);
  EXPECT_EQ(raw_pointer_, &*p);
}

TEST_F(NPObjectPointerTest, PointerCanBeConstructedFromReturnedNPObject) {
  NPBrowser::get()->RetainObject(raw_pointer_);
  EXPECT_EQ(2, raw_pointer_->referenceCount);
  {
    NPObjectPointer<MockNPObject> p(
      NPObjectPointer<MockNPObject>::FromReturned(raw_pointer_));
    EXPECT_EQ(2, raw_pointer_->referenceCount);
  }
  EXPECT_EQ(1, raw_pointer_->referenceCount);
}

TEST_F(NPObjectPointerTest, PointerCanBeConstructedFromReturnedNullNPObject) {
  NPObjectPointer<MockNPObject> p(
    NPObjectPointer<MockNPObject>::FromReturned(NULL));
  EXPECT_TRUE(NULL == p.Get());
}

TEST_F(NPObjectPointerTest, PointerCanBeReturnedAsARawNPObject) {
  NPObjectPointer<MockNPObject> p(raw_pointer_);
  EXPECT_EQ(raw_pointer_, p.ToReturned());

  // Check reference count is incremented before return for caller.
  EXPECT_EQ(3, raw_pointer_->referenceCount);

  NPBrowser::get()->ReleaseObject(raw_pointer_);
}

TEST_F(NPObjectPointerTest, NULLPointerCanBeReturnedAsARawNPObject) {
  NPObjectPointer<MockNPObject> p;
  EXPECT_TRUE(NULL == p.ToReturned());
}

}  // namespace gpu_plugin
