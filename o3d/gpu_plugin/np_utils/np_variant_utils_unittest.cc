// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/np_utils/base_np_object_mock.h"
#include "o3d/gpu_plugin/np_utils/np_variant_utils.h"
#include "o3d/gpu_plugin/np_utils/npn_test_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::MakeMatcher;
using testing::Matcher;
using testing::Pointee;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace o3d {
namespace gpu_plugin {

class NPVariantUtilsTest : public testing::Test {
 protected:
  virtual void SetUp() {
    InitializeNPNTestStub();

    np_class_ = const_cast<NPClass*>(
        BaseNPObject::GetNPClass<StrictMock<MockBaseNPObject> >());

    // Make sure no MockBaseNPObject objects exist before test.
    ASSERT_EQ(0, MockBaseNPObject::count());
  }

  virtual void TearDown() {
    // Make sure no MockBaseNPObject leaked an object.
    ASSERT_EQ(0, MockBaseNPObject::count());

    ShutdownNPNTestStub();
  }

  NPP_t npp_;
  NPClass* np_class_;
  NPVariant variant_;
};

TEST_F(NPVariantUtilsTest, TestBoolNPVariantToValue) {
  bool v;

  BOOLEAN_TO_NPVARIANT(false, variant_);
  EXPECT_TRUE(NPVariantToValue(&v, variant_));
  EXPECT_FALSE(v);

  BOOLEAN_TO_NPVARIANT(true, variant_);
  EXPECT_TRUE(NPVariantToValue(&v, variant_));
  EXPECT_TRUE(v);

  INT32_TO_NPVARIANT(7, variant_);
  EXPECT_FALSE(NPVariantToValue(&v, variant_));
}

TEST_F(NPVariantUtilsTest, TestIntNPVariantToValue) {
  INT32_TO_NPVARIANT(7, variant_);

  int v1;
  EXPECT_TRUE(NPVariantToValue(&v1, variant_));
  EXPECT_EQ(7, v1);

  float v2;
  EXPECT_TRUE(NPVariantToValue(&v2, variant_));
  EXPECT_EQ(7.0f, v2);

  double v3;
  EXPECT_TRUE(NPVariantToValue(&v3, variant_));
  EXPECT_EQ(7.0, v3);

  BOOLEAN_TO_NPVARIANT(false, variant_);
  EXPECT_FALSE(NPVariantToValue(&v1, variant_));
}

TEST_F(NPVariantUtilsTest, TestFloatNPVariantToValue) {
  float v;

  DOUBLE_TO_NPVARIANT(7.0, variant_);
  EXPECT_TRUE(NPVariantToValue(&v, variant_));
  EXPECT_EQ(7.0f, v);

  BOOLEAN_TO_NPVARIANT(false, variant_);
  EXPECT_FALSE(NPVariantToValue(&v, variant_));
}

TEST_F(NPVariantUtilsTest, TestDoubleNPVariantToValue) {
  double v;

  DOUBLE_TO_NPVARIANT(7.0, variant_);
  EXPECT_TRUE(NPVariantToValue(&v, variant_));
  EXPECT_EQ(7.0, v);

  BOOLEAN_TO_NPVARIANT(false, variant_);
  EXPECT_FALSE(NPVariantToValue(&v, variant_));
}

TEST_F(NPVariantUtilsTest, TestStringNPVariantToValue) {
  std::string v;

  STRINGZ_TO_NPVARIANT("hello", variant_);
  EXPECT_TRUE(NPVariantToValue(&v, variant_));
  EXPECT_EQ(std::string("hello"), v);

  BOOLEAN_TO_NPVARIANT(false, variant_);
  EXPECT_FALSE(NPVariantToValue(&v, variant_));
}

TEST_F(NPVariantUtilsTest, TestObjectNPVariantToValue) {
  NPObject* object = gpu_plugin::NPN_CreateObject(NULL, np_class_);
  NPObject* v;

  OBJECT_TO_NPVARIANT(object, variant_);
  EXPECT_TRUE(NPVariantToValue(&v, variant_));
  EXPECT_EQ(object, v);

  BOOLEAN_TO_NPVARIANT(false, variant_);
  EXPECT_FALSE(NPVariantToValue(&v, variant_));

  gpu_plugin::NPN_ReleaseObject(object);
}

TEST_F(NPVariantUtilsTest, TestNullNPVariantToValue) {
  NPObject* v;

  NULL_TO_NPVARIANT(variant_);
  EXPECT_TRUE(NPVariantToValue(&v, variant_));
  EXPECT_TRUE(NULL == v);

  BOOLEAN_TO_NPVARIANT(false, variant_);
  EXPECT_FALSE(NPVariantToValue(&v, variant_));
}

TEST_F(NPVariantUtilsTest, TestBoolValueToNPVariant) {
  ValueToNPVariant(true, &variant_);
  EXPECT_TRUE(NPVARIANT_IS_BOOLEAN(variant_));
  EXPECT_TRUE(NPVARIANT_TO_BOOLEAN(variant_));

  ValueToNPVariant(false, &variant_);
  EXPECT_TRUE(NPVARIANT_IS_BOOLEAN(variant_));
  EXPECT_FALSE(NPVARIANT_TO_BOOLEAN(variant_));
}

TEST_F(NPVariantUtilsTest, TestIntValueToNPVariant) {
  ValueToNPVariant(7, &variant_);
  EXPECT_TRUE(NPVARIANT_IS_INT32(variant_));
  EXPECT_EQ(7, NPVARIANT_TO_INT32(variant_));
}

TEST_F(NPVariantUtilsTest, TestFloatValueToNPVariant) {
  ValueToNPVariant(7.0f, &variant_);
  EXPECT_TRUE(NPVARIANT_IS_DOUBLE(variant_));
  EXPECT_EQ(7.0, NPVARIANT_TO_DOUBLE(variant_));
}

TEST_F(NPVariantUtilsTest, TestDoubleValueToNPVariant) {
  ValueToNPVariant(7.0, &variant_);
  EXPECT_TRUE(NPVARIANT_IS_DOUBLE(variant_));
  EXPECT_EQ(7.0, NPVARIANT_TO_DOUBLE(variant_));
}

TEST_F(NPVariantUtilsTest, TestStringValueToNPVariant) {
  ValueToNPVariant(std::string("hello"), &variant_);
  EXPECT_TRUE(NPVARIANT_IS_STRING(variant_));
  EXPECT_EQ(std::string("hello"),
            std::string(variant_.value.stringValue.UTF8Characters,
                        variant_.value.stringValue.UTF8Length));
}

TEST_F(NPVariantUtilsTest, TestObjectValueToNPVariant) {
  NPObject* object = gpu_plugin::NPN_CreateObject(NULL, np_class_);

  ValueToNPVariant(object, &variant_);
  EXPECT_TRUE(NPVARIANT_IS_OBJECT(variant_));
  EXPECT_EQ(object, NPVARIANT_TO_OBJECT(variant_));

  gpu_plugin::NPN_ReleaseVariantValue(&variant_);
  gpu_plugin::NPN_ReleaseObject(object);
}

TEST_F(NPVariantUtilsTest, TestNullValueToNPVariant) {
  ValueToNPVariant(static_cast<NPObject*>(NULL), &variant_);
  EXPECT_TRUE(NPVARIANT_IS_NULL(variant_));
}

template <typename T>
class VariantMatcher : public testing::MatcherInterface<const NPVariant&> {
 public:
  explicit VariantMatcher(const T& value) : value_(value) {
  }

  virtual bool Matches(const NPVariant& variant) const {
    T other_value;
    return NPVariantToValue(&other_value, variant) && other_value == value_;
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "equals " << value_;
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not equal " << value_;
  }

 private:
  T value_;
};

template <typename T>
Matcher<const NPVariant&> VariantMatches(const T& value) {
  return MakeMatcher(new VariantMatcher<T>(value));
}

TEST_F(NPVariantUtilsTest, CanInvokeVoidMethodWithNativeTypes) {
  NPIdentifier name = NPN_GetStringIdentifier("foo");
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      gpu_plugin::NPN_CreateObject(NULL, np_class_));

  VOID_TO_NPVARIANT(variant_);

  EXPECT_CALL(*object, Invoke(name, Pointee(VariantMatches<int>(7)), 1, _))
    .WillOnce(DoAll(SetArgumentPointee<3>(variant_),
                    Return(true)));

  EXPECT_TRUE(NPInvokeVoid(NULL, object, name, 7));

  gpu_plugin::NPN_ReleaseObject(object);
}

TEST_F(NPVariantUtilsTest, InvokeVoidMethodCanFail) {
  NPIdentifier name = NPN_GetStringIdentifier("foo");
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      gpu_plugin::NPN_CreateObject(NULL, np_class_));

  VOID_TO_NPVARIANT(variant_);

  EXPECT_CALL(*object, Invoke(name, Pointee(VariantMatches<int>(7)), 1, _))
    .WillOnce(DoAll(SetArgumentPointee<3>(variant_),
                    Return(false)));

  EXPECT_FALSE(NPInvokeVoid(NULL, object, name, 7));

  gpu_plugin::NPN_ReleaseObject(object);
}

TEST_F(NPVariantUtilsTest, CanInvokeNonVoidMethodWithNativeTypes) {
  NPIdentifier name = NPN_GetStringIdentifier("foo");
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      gpu_plugin::NPN_CreateObject(NULL, np_class_));

  DOUBLE_TO_NPVARIANT(1.5, variant_);

  EXPECT_CALL(*object, Invoke(name, Pointee(VariantMatches<int>(7)), 1, _))
    .WillOnce(DoAll(SetArgumentPointee<3>(variant_),
                    Return(true)));

  double r;
  EXPECT_TRUE(NPInvoke(NULL, object, name, 7, &r));
  EXPECT_EQ(1.5, r);

  gpu_plugin::NPN_ReleaseObject(object);
}

TEST_F(NPVariantUtilsTest, InvokeNonVoidMethodCanFail) {
  NPIdentifier name = NPN_GetStringIdentifier("foo");
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      gpu_plugin::NPN_CreateObject(NULL, np_class_));

  DOUBLE_TO_NPVARIANT(1.5, variant_);

  EXPECT_CALL(*object, Invoke(name, Pointee(VariantMatches<int>(7)), 1, _))
    .WillOnce(DoAll(SetArgumentPointee<3>(variant_),
                    Return(false)));

  double r;
  EXPECT_FALSE(NPInvoke(NULL, object, name, 7, &r));

  gpu_plugin::NPN_ReleaseObject(object);
}

TEST_F(NPVariantUtilsTest, InvokeNonVoidMethodFailsIfResultIsIncompatible) {
  NPIdentifier name = NPN_GetStringIdentifier("foo");
  MockBaseNPObject* object = static_cast<MockBaseNPObject*>(
      gpu_plugin::NPN_CreateObject(NULL, np_class_));

  DOUBLE_TO_NPVARIANT(1.5, variant_);

  EXPECT_CALL(*object, Invoke(name, Pointee(VariantMatches<int>(7)), 1, _))
    .WillOnce(DoAll(SetArgumentPointee<3>(variant_),
                    Return(true)));

  int r;
  EXPECT_FALSE(NPInvoke(NULL, object, name, 7, &r));

  gpu_plugin::NPN_ReleaseObject(object);
}

}  // namespace gpu_plugin
}  // namespace o3d
