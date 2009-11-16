// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/np_utils/np_class.h"
#include "gpu/np_utils/np_object_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace np_utils {

class NPClassTest : public testing::Test {
 protected:
  virtual void SetUp() {
    np_class = NPGetClass<StrictMock<MockNPObject> >();

    // Dummy identifier is never used with real NPAPI so it can point to
    // anything.
    identifier = this;
  }

  virtual void TearDown() {
  }

  NPP_t npp_;
  const NPClass* np_class;
  NPIdentifier identifier;
  NPVariant args[3];
  NPVariant result;
};

TEST_F(NPClassTest, AllocateAndDeallocateObject) {
  MockNPObject* object = static_cast<MockNPObject*>(
      np_class->allocate(&npp_, const_cast<NPClass*>(np_class)));
  EXPECT_TRUE(NULL != object);

  np_class->deallocate(object);
}

TEST_F(NPClassTest, InvalidateForwards) {
  MockNPObject* object = static_cast<MockNPObject*>(
      np_class->allocate(&npp_, const_cast<NPClass*>(np_class)));

  EXPECT_CALL(*object, Invalidate());
  np_class->invalidate(object);

  np_class->deallocate(object);
}

TEST_F(NPClassTest, HasMethodForwards) {
  MockNPObject* object = static_cast<MockNPObject*>(
      np_class->allocate(&npp_, const_cast<NPClass*>(np_class)));

  EXPECT_CALL(*object, HasMethod(identifier));
  np_class->hasMethod(object, identifier);

  np_class->deallocate(object);
}

TEST_F(NPClassTest, InvokeForwards) {
  MockNPObject* object = static_cast<MockNPObject*>(
      np_class->allocate(&npp_, const_cast<NPClass*>(np_class)));

  EXPECT_CALL(*object, Invoke(identifier, args, 3, &result));
  np_class->invoke(object, identifier, args, 3, &result);

  np_class->deallocate(object);
}

TEST_F(NPClassTest, InvokeDefaultForwards) {
  MockNPObject* object = static_cast<MockNPObject*>(
      np_class->allocate(&npp_, const_cast<NPClass*>(np_class)));

  EXPECT_CALL(*object, InvokeDefault(args, 3, &result));
  np_class->invokeDefault(object, args, 3, &result);

  np_class->deallocate(object);
}

TEST_F(NPClassTest, HasPropertyForwards) {
  MockNPObject* object = static_cast<MockNPObject*>(
      np_class->allocate(&npp_, const_cast<NPClass*>(np_class)));

  EXPECT_CALL(*object, HasProperty(identifier));
  np_class->hasProperty(object, identifier);

  np_class->deallocate(object);
}

TEST_F(NPClassTest, GetPropertyForwards) {
  MockNPObject* object = static_cast<MockNPObject*>(
      np_class->allocate(&npp_, const_cast<NPClass*>(np_class)));

  EXPECT_CALL(*object, GetProperty(identifier, &result));
  np_class->getProperty(object, identifier, &result);

  np_class->deallocate(object);
}

TEST_F(NPClassTest, SetPropertyForwards) {
  MockNPObject* object = static_cast<MockNPObject*>(
      np_class->allocate(&npp_, const_cast<NPClass*>(np_class)));

  EXPECT_CALL(*object, SetProperty(identifier, &result));
  np_class->setProperty(object, identifier, &result);

  np_class->deallocate(object);
}

TEST_F(NPClassTest, RemovePropertyForwards) {
  MockNPObject* object = static_cast<MockNPObject*>(
      np_class->allocate(&npp_, const_cast<NPClass*>(np_class)));

  EXPECT_CALL(*object, RemoveProperty(identifier));
  np_class->removeProperty(object, identifier);

  np_class->deallocate(object);
}

TEST_F(NPClassTest, EnumerateForwards) {
  MockNPObject* object = static_cast<MockNPObject*>(
      np_class->allocate(&npp_, const_cast<NPClass*>(np_class)));

  NPIdentifier* identifier = NULL;
  uint32_t count;
  EXPECT_CALL(*object, Enumerate(&identifier, &count));
  np_class->enumerate(object, &identifier, &count);

  np_class->deallocate(object);
}

TEST_F(NPClassTest, ConstructForwards) {
  MockNPObject* object = static_cast<MockNPObject*>(
      np_class->allocate(&npp_, const_cast<NPClass*>(np_class)));

  EXPECT_CALL(*object, Construct(args, 3, &result));
  np_class->construct(object, args, 3, &result);

  np_class->deallocate(object);
}
}  // namespace np_utils
