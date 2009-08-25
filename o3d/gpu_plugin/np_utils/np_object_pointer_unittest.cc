// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/np_utils/base_np_object_mock.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"
#include "o3d/gpu_plugin/np_utils/npn_test_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::StrictMock;

namespace o3d {
namespace gpu_plugin {

class NPObjectPointerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    InitializeNPNTestStub();

    np_class_ = const_cast<NPClass*>(
        BaseNPObject::GetNPClass<StrictMock<MockBaseNPObject> >());

    // Make sure no MockBaseNPObject objects exist before test.
    ASSERT_EQ(0, MockBaseNPObject::count());

    raw_pointer_ = static_cast<MockBaseNPObject*>(
      gpu_plugin::NPN_CreateObject(NULL, np_class_));
  }

  virtual void TearDown() {
    gpu_plugin::NPN_ReleaseObject(raw_pointer_);

    // Make sure no MockBaseNPObject leaked an object.
    ASSERT_EQ(0, MockBaseNPObject::count());

    ShutdownNPNTestStub();
  }

  NPClass* np_class_;
  MockBaseNPObject* raw_pointer_;
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

TEST_F(NPObjectPointerTest, ArrowOperatorCanBeUsedToAccessNPObjectMembers) {
  NPIdentifier name = NPN_GetStringIdentifier("hello");

  EXPECT_CALL(*raw_pointer_, HasProperty(name)).WillOnce(Return(true));

  NPObjectPointer<MockBaseNPObject> p(raw_pointer_);
  EXPECT_TRUE(p->HasProperty(name));
}

TEST_F(NPObjectPointerTest, PointerCanBeConstructedFromReturnedNPObject) {
  gpu_plugin::NPN_RetainObject(raw_pointer_);
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

}  // namespace gpu_plugin
}  // namespace o3d
