// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/base_np_object_mock.h"
#include "o3d/gpu_plugin/base_np_object.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace o3d {
namespace gpu_plugin {

class BaseNPObjectTest : public testing::Test {
 protected:
  virtual void SetUp() {
    np_class = const_cast<NPClass*>(
        BaseNPObject::GetNPClass<StrictMock<MockBaseNPObject> >());

    // Dummy identifier is never used with real NPAPI so it can point to
    // anything.
    identifier = this;

    // Make sure no MockBaseNPObject objects exist before test.
    ASSERT_EQ(0, MockBaseNPObject::count());
  }

  virtual void TearDown() {
    // Make sure no MockBaseNPObject leaked an object.
    ASSERT_EQ(0, MockBaseNPObject::count());
  }

  NPP_t npp_;
  NPClass* np_class;
  NPIdentifier identifier;
  NPVariant args[3];
  NPVariant result;
};


TEST_F(BaseNPObjectTest, AllocateAndDeallocateObject) {
  EXPECT_EQ(0, MockBaseNPObject::count());

  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      np_class->allocate(&npp_, np_class));
  EXPECT_TRUE(NULL != object);

  EXPECT_EQ(1, MockBaseNPObject::count());

  np_class->deallocate(object);
  EXPECT_EQ(0, MockBaseNPObject::count());
}

TEST_F(BaseNPObjectTest, InvalidateForwards) {
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      np_class->allocate(&npp_, np_class));

  EXPECT_CALL(*object, Invalidate());
  np_class->invalidate(object);

  np_class->deallocate(object);
}

TEST_F(BaseNPObjectTest, HasMethodForwards) {
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      np_class->allocate(&npp_, np_class));

  EXPECT_CALL(*object, HasMethod(identifier));
  np_class->hasMethod(object, identifier);

  np_class->deallocate(object);
}

TEST_F(BaseNPObjectTest, InvokeForwards) {
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      np_class->allocate(&npp_, np_class));

  EXPECT_CALL(*object, Invoke(identifier, args, 3, &result));
  np_class->invoke(object, identifier, args, 3, &result);

  np_class->deallocate(object);
}

TEST_F(BaseNPObjectTest, InvokeDefaultForwards) {
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      np_class->allocate(&npp_, np_class));

  EXPECT_CALL(*object, InvokeDefault(args, 3, &result));
  np_class->invokeDefault(object, args, 3, &result);

  np_class->deallocate(object);
}

TEST_F(BaseNPObjectTest, HasPropertyForwards) {
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      np_class->allocate(&npp_, np_class));

  EXPECT_CALL(*object, HasProperty(identifier));
  np_class->hasProperty(object, identifier);

  np_class->deallocate(object);
}

TEST_F(BaseNPObjectTest, GetPropertyForwards) {
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      np_class->allocate(&npp_, np_class));

  EXPECT_CALL(*object, GetProperty(identifier, &result));
  np_class->getProperty(object, identifier, &result);

  np_class->deallocate(object);
}

TEST_F(BaseNPObjectTest, SetPropertyForwards) {
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      np_class->allocate(&npp_, np_class));

  EXPECT_CALL(*object, SetProperty(identifier, &result));
  np_class->setProperty(object, identifier, &result);

  np_class->deallocate(object);
}

TEST_F(BaseNPObjectTest, RemovePropertyForwards) {
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      np_class->allocate(&npp_, np_class));

  EXPECT_CALL(*object, RemoveProperty(identifier));
  np_class->removeProperty(object, identifier);

  np_class->deallocate(object);
}

TEST_F(BaseNPObjectTest, EnumerateForwards) {
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      np_class->allocate(&npp_, np_class));

  NPIdentifier* identifier = NULL;
  uint32_t count;
  EXPECT_CALL(*object, Enumerate(&identifier, &count));
  np_class->enumerate(object, &identifier, &count);

  np_class->deallocate(object);
}

TEST_F(BaseNPObjectTest, ConstructForwards) {
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      np_class->allocate(&npp_, np_class));

  EXPECT_CALL(*object, Construct(args, 3, &result));
  np_class->construct(object, args, 3, &result);

  np_class->deallocate(object);
}
}  // namespace gpu_plugin
}  // namespace o3d
