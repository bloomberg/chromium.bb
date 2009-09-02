// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/np_utils/base_np_object_mock.h"
#include "o3d/gpu_plugin/np_utils/np_browser_stub.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::StrictMock;

namespace o3d {
namespace gpu_plugin {

class DerivedNPObject : public MockBaseNPObject {
 public:
  explicit DerivedNPObject(NPP npp) : MockBaseNPObject(npp) {
  }
};

class NPObjectPointerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    np_class_ = BaseNPObject::GetNPClass<StrictMock<MockBaseNPObject> >();

    // Make sure no MockBaseNPObject objects exist before test.
    ASSERT_EQ(0, MockBaseNPObject::count());

    raw_pointer_ = static_cast<MockBaseNPObject*>(
      NPBrowser::get()->CreateObject(NULL, np_class_));

    raw_derived_pointer_ = static_cast<DerivedNPObject*>(
      NPBrowser::get()->CreateObject(NULL, np_class_));
  }

  virtual void TearDown() {
    NPBrowser::get()->ReleaseObject(raw_pointer_);
    NPBrowser::get()->ReleaseObject(raw_derived_pointer_);

    // Make sure no MockBaseNPObject leaked an object.
    ASSERT_EQ(0, MockBaseNPObject::count());
  }

  StubNPBrowser stub_browser_;
  const NPClass* np_class_;
  MockBaseNPObject* raw_pointer_;
  DerivedNPObject* raw_derived_pointer_;
};

TEST_F(NPObjectPointerTest, PointerIsNullByDefault) {
  NPObjectPointer<MockBaseNPObject> p;
  ASSERT_TRUE(NULL == p.Get());
}

TEST_F(NPObjectPointerTest, PointerCanBeExplicitlyConstructedFromRawPointer) {
  EXPECT_EQ(1, raw_pointer_->referenceCount);
  {
    NPObjectPointer<MockBaseNPObject> p(raw_pointer_);
    ASSERT_TRUE(raw_pointer_ == p.Get());
    EXPECT_EQ(2, raw_pointer_->referenceCount);
  }
  EXPECT_EQ(1, raw_pointer_->referenceCount);
}

TEST_F(NPObjectPointerTest,
    PointerCanBeExplicitlyConstructedFromNullRawPointer) {
  NPObjectPointer<MockBaseNPObject> p(NULL);
  ASSERT_TRUE(NULL == p.Get());
}

TEST_F(NPObjectPointerTest, PointerCanBeCopyConstructed) {
  NPObjectPointer<MockBaseNPObject> p1(raw_pointer_);
  EXPECT_EQ(2, raw_pointer_->referenceCount);
  {
    NPObjectPointer<MockBaseNPObject> p2(p1);
    ASSERT_TRUE(raw_pointer_ == p2.Get());
    EXPECT_EQ(3, raw_pointer_->referenceCount);
  }
  EXPECT_EQ(2, raw_pointer_->referenceCount);
}

TEST_F(NPObjectPointerTest, PointerCanBeConstructedFromDerived) {
  NPObjectPointer<DerivedNPObject> p1(raw_derived_pointer_);
  EXPECT_EQ(2, raw_derived_pointer_->referenceCount);
  {
    NPObjectPointer<MockBaseNPObject> p2(p1);
    ASSERT_TRUE(raw_derived_pointer_ == p2.Get());
    EXPECT_EQ(3, raw_derived_pointer_->referenceCount);
  }
  EXPECT_EQ(2, raw_derived_pointer_->referenceCount);
}

TEST_F(NPObjectPointerTest,
    PointerCanBeCopyConstructedFromNull) {
  NPObjectPointer<MockBaseNPObject> p(NULL);
  ASSERT_TRUE(NULL == p.Get());
}

TEST_F(NPObjectPointerTest, PointerCanBeAssigned) {
  NPObjectPointer<MockBaseNPObject> p1(raw_pointer_);
  EXPECT_EQ(2, raw_pointer_->referenceCount);
  {
    NPObjectPointer<MockBaseNPObject> p2;
    p2 = p1;
    ASSERT_TRUE(raw_pointer_ == p2.Get());
    EXPECT_EQ(3, raw_pointer_->referenceCount);

    p2 = NPObjectPointer<MockBaseNPObject>();
    ASSERT_TRUE(NULL == p2.Get());
    EXPECT_EQ(2, raw_pointer_->referenceCount);

    p2 = p1;
    ASSERT_TRUE(raw_pointer_ == p2.Get());
    EXPECT_EQ(3, raw_pointer_->referenceCount);
  }
  EXPECT_EQ(2, raw_pointer_->referenceCount);
}

TEST_F(NPObjectPointerTest, PointerCanBeAssignedDerived) {
  NPObjectPointer<DerivedNPObject> p1(raw_derived_pointer_);
  EXPECT_EQ(2, raw_derived_pointer_->referenceCount);
  {
    NPObjectPointer<MockBaseNPObject> p2;
    p2 = p1;
    ASSERT_TRUE(raw_derived_pointer_ == p2.Get());
    EXPECT_EQ(3, raw_derived_pointer_->referenceCount);

    p2 = NPObjectPointer<MockBaseNPObject>();
    ASSERT_TRUE(NULL == p2.Get());
    EXPECT_EQ(2, raw_derived_pointer_->referenceCount);

    p2 = p1;
    ASSERT_TRUE(raw_derived_pointer_ == p2.Get());
    EXPECT_EQ(3, raw_derived_pointer_->referenceCount);
  }
  EXPECT_EQ(2, raw_derived_pointer_->referenceCount);
}

TEST_F(NPObjectPointerTest, CanComparePointersForEqual) {
  NPObjectPointer<MockBaseNPObject> p1(raw_pointer_);
  NPObjectPointer<DerivedNPObject> p2(raw_derived_pointer_);
  EXPECT_TRUE(p1 == p1);
  EXPECT_FALSE(p1 == p2);
  EXPECT_FALSE(p2 == p1);
  EXPECT_FALSE(p1 == NPObjectPointer<MockBaseNPObject>());
}

TEST_F(NPObjectPointerTest, CanComparePointersForNotEqual) {
  NPObjectPointer<MockBaseNPObject> p1(raw_pointer_);
  NPObjectPointer<DerivedNPObject> p2(raw_derived_pointer_);
  EXPECT_FALSE(p1 != p1);
  EXPECT_TRUE(p1 != p2);
  EXPECT_TRUE(p2 != p1);
  EXPECT_TRUE(p1 != NPObjectPointer<MockBaseNPObject>());
}

TEST_F(NPObjectPointerTest, ArrowOperatorCanBeUsedToAccessNPObjectMembers) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("hello");

  EXPECT_CALL(*raw_pointer_, HasProperty(name)).WillOnce(Return(true));

  NPObjectPointer<MockBaseNPObject> p(raw_pointer_);
  EXPECT_TRUE(p->HasProperty(name));
}

TEST_F(NPObjectPointerTest, StarOperatorReturnsNPObjectReference) {
  NPObjectPointer<MockBaseNPObject> p(raw_pointer_);
  EXPECT_EQ(raw_pointer_, &*p);
}

TEST_F(NPObjectPointerTest, PointerCanBeConstructedFromReturnedNPObject) {
  NPBrowser::get()->RetainObject(raw_pointer_);
  EXPECT_EQ(2, raw_pointer_->referenceCount);
  {
    NPObjectPointer<MockBaseNPObject> p(
      NPObjectPointer<MockBaseNPObject>::FromReturned(raw_pointer_));
    EXPECT_EQ(2, raw_pointer_->referenceCount);
  }
  EXPECT_EQ(1, raw_pointer_->referenceCount);
}

TEST_F(NPObjectPointerTest, PointerCanBeConstructedFromReturnedNullNPObject) {
  NPObjectPointer<MockBaseNPObject> p(
    NPObjectPointer<MockBaseNPObject>::FromReturned(NULL));
  EXPECT_TRUE(NULL == p.Get());
}

TEST_F(NPObjectPointerTest, PointerCanBeReturnedAsARawNPObject) {
  NPObjectPointer<MockBaseNPObject> p(raw_pointer_);
  EXPECT_EQ(raw_pointer_, p.ToReturned());

  // Check reference count is incremented before return for caller.
  EXPECT_EQ(3, raw_pointer_->referenceCount);

  NPBrowser::get()->ReleaseObject(raw_pointer_);
}

TEST_F(NPObjectPointerTest, NULLPointerCanBeReturnedAsARawNPObject) {
  NPObjectPointer<MockBaseNPObject> p;
  EXPECT_TRUE(NULL == p.ToReturned());
}

}  // namespace gpu_plugin
}  // namespace o3d
