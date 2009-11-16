// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/np_utils/np_object_mock.h"
#include "gpu/np_utils/np_browser_stub.h"
#include "gpu/np_utils/np_utils.h"
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

namespace np_utils {

class NPUtilsTest : public testing::Test {
 protected:
  StubNPBrowser stub_browser_;
  NPP_t npp_;
  NPVariant variant_;
};

TEST_F(NPUtilsTest, TestBoolNPVariantToValue) {
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

TEST_F(NPUtilsTest, TestIntNPVariantToValue) {
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

TEST_F(NPUtilsTest, TestFloatNPVariantToValue) {
  float v;

  DOUBLE_TO_NPVARIANT(7.0, variant_);
  EXPECT_TRUE(NPVariantToValue(&v, variant_));
  EXPECT_EQ(7.0f, v);

  BOOLEAN_TO_NPVARIANT(false, variant_);
  EXPECT_FALSE(NPVariantToValue(&v, variant_));
}

TEST_F(NPUtilsTest, TestDoubleNPVariantToValue) {
  double v;

  DOUBLE_TO_NPVARIANT(7.0, variant_);
  EXPECT_TRUE(NPVariantToValue(&v, variant_));
  EXPECT_EQ(7.0, v);

  BOOLEAN_TO_NPVARIANT(false, variant_);
  EXPECT_FALSE(NPVariantToValue(&v, variant_));
}

TEST_F(NPUtilsTest, TestStringNPVariantToValue) {
  std::string v;

  STRINGZ_TO_NPVARIANT("hello", variant_);
  EXPECT_TRUE(NPVariantToValue(&v, variant_));
  EXPECT_EQ(std::string("hello"), v);

  BOOLEAN_TO_NPVARIANT(false, variant_);
  EXPECT_FALSE(NPVariantToValue(&v, variant_));
}

TEST_F(NPUtilsTest, TestObjectNPVariantToValue) {
  NPObjectPointer<NPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);
  NPObjectPointer<NPObject> v;

  OBJECT_TO_NPVARIANT(object.Get(), variant_);
  EXPECT_TRUE(NPVariantToValue(&v, variant_));
  EXPECT_EQ(object, v);

  BOOLEAN_TO_NPVARIANT(false, variant_);
  EXPECT_FALSE(NPVariantToValue(&v, variant_));
}

TEST_F(NPUtilsTest, TestNullNPVariantToValue) {
  NPObjectPointer<NPObject> v;

  NULL_TO_NPVARIANT(variant_);
  EXPECT_TRUE(NPVariantToValue(&v, variant_));
  EXPECT_TRUE(NPObjectPointer<NPObject>() == v);

  BOOLEAN_TO_NPVARIANT(false, variant_);
  EXPECT_FALSE(NPVariantToValue(&v, variant_));
}

TEST_F(NPUtilsTest, TestDerivedObjectNPVariantToValue) {
  NPObjectPointer<NPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);
  NPObjectPointer<StrictMock<MockNPObject> > v;

  OBJECT_TO_NPVARIANT(object.Get(), variant_);
  EXPECT_TRUE(NPVariantToValue(&v, variant_));
  EXPECT_EQ(object, v);
}

TEST_F(NPUtilsTest,
    TestDerivedObjectNPVariantToValueFailsIfValueHasDifferentType) {
  NPObjectPointer<NPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);
  NPObjectPointer<MockNPObject> v;

  OBJECT_TO_NPVARIANT(object.Get(), variant_);
  EXPECT_FALSE(NPVariantToValue(&v, variant_));
}

TEST_F(NPUtilsTest, TestBoolValueToNPVariant) {
  ValueToNPVariant(true, &variant_);
  EXPECT_TRUE(NPVARIANT_IS_BOOLEAN(variant_));
  EXPECT_TRUE(NPVARIANT_TO_BOOLEAN(variant_));

  ValueToNPVariant(false, &variant_);
  EXPECT_TRUE(NPVARIANT_IS_BOOLEAN(variant_));
  EXPECT_FALSE(NPVARIANT_TO_BOOLEAN(variant_));
}

TEST_F(NPUtilsTest, TestIntValueToNPVariant) {
  ValueToNPVariant(7, &variant_);
  EXPECT_TRUE(NPVARIANT_IS_INT32(variant_));
  EXPECT_EQ(7, NPVARIANT_TO_INT32(variant_));
}

TEST_F(NPUtilsTest, TestFloatValueToNPVariant) {
  ValueToNPVariant(7.0f, &variant_);
  EXPECT_TRUE(NPVARIANT_IS_DOUBLE(variant_));
  EXPECT_EQ(7.0, NPVARIANT_TO_DOUBLE(variant_));
}

TEST_F(NPUtilsTest, TestDoubleValueToNPVariant) {
  ValueToNPVariant(7.0, &variant_);
  EXPECT_TRUE(NPVARIANT_IS_DOUBLE(variant_));
  EXPECT_EQ(7.0, NPVARIANT_TO_DOUBLE(variant_));
}

TEST_F(NPUtilsTest, TestStringValueToNPVariant) {
  ValueToNPVariant(std::string("hello"), &variant_);
  EXPECT_TRUE(NPVARIANT_IS_STRING(variant_));
  EXPECT_EQ(std::string("hello"),
            std::string(variant_.value.stringValue.UTF8Characters,
                        variant_.value.stringValue.UTF8Length));
}

TEST_F(NPUtilsTest, TestObjectValueToNPVariant) {
  NPObjectPointer<NPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  ValueToNPVariant(object, &variant_);
  EXPECT_TRUE(NPVARIANT_IS_OBJECT(variant_));
  EXPECT_EQ(object.Get(), NPVARIANT_TO_OBJECT(variant_));

  NPBrowser::get()->ReleaseVariantValue(&variant_);
}

TEST_F(NPUtilsTest, TestNullValueToNPVariant) {
  ValueToNPVariant(NPObjectPointer<NPObject>(), &variant_);
  EXPECT_TRUE(NPVARIANT_IS_NULL(variant_));
}

TEST_F(NPUtilsTest, CanCopyObjectSmartVariant) {
  NPObjectPointer<NPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);
  EXPECT_EQ(1, object->referenceCount);
  {
    SmartNPVariant v1(object);
    EXPECT_EQ(2, object->referenceCount);
    {
      SmartNPVariant v2(v1);
      EXPECT_EQ(3, object->referenceCount);
    }
    EXPECT_EQ(2, object->referenceCount);
  }
  EXPECT_EQ(1, object->referenceCount);
}

TEST_F(NPUtilsTest, CanCopyStringSmartVariant) {
  SmartNPVariant v1(std::string("hello"));
  SmartNPVariant v2(v1);
  std::string r;
  EXPECT_TRUE(NPVariantToValue(&r, v2));
  EXPECT_EQ(std::string("hello"), r);
  EXPECT_NE(v1.value.stringValue.UTF8Characters,
            v2.value.stringValue.UTF8Characters);
}

TEST_F(NPUtilsTest, CanReleaseSmartVariant) {
  SmartNPVariant variant(std::string("hello"));
  EXPECT_FALSE(variant.IsVoid());
  variant.Release();
  EXPECT_TRUE(variant.IsVoid());
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

TEST_F(NPUtilsTest, CanDetermineIfObjectHasMethod) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("foo");
  NPObjectPointer<MockNPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  EXPECT_CALL(*object, HasMethod(name))
    .WillOnce(Return(true));

  EXPECT_TRUE(NPHasMethod(NULL, object, "foo"));
}

TEST_F(NPUtilsTest, CanInvokeVoidMethodWithNativeTypes) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("foo");
  NPObjectPointer<MockNPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  VOID_TO_NPVARIANT(variant_);

  EXPECT_CALL(*object, Invoke(name, Pointee(VariantMatches<int32>(7)), 1, _))
    .WillOnce(DoAll(SetArgumentPointee<3>(variant_),
                    Return(true)));

  EXPECT_TRUE(NPInvokeVoid(NULL, object, "foo", 7));
}

TEST_F(NPUtilsTest, InvokeVoidMethodCanFail) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("foo");
  NPObjectPointer<MockNPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  VOID_TO_NPVARIANT(variant_);

  EXPECT_CALL(*object, Invoke(name, Pointee(VariantMatches<int32>(7)), 1, _))
    .WillOnce(DoAll(SetArgumentPointee<3>(variant_),
                    Return(false)));

  EXPECT_FALSE(NPInvokeVoid(NULL, object, "foo", 7));
}

TEST_F(NPUtilsTest, CanInvokeNonVoidMethodWithNativeTypes) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("foo");
  NPObjectPointer<MockNPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  DOUBLE_TO_NPVARIANT(1.5, variant_);

  EXPECT_CALL(*object, Invoke(name, Pointee(VariantMatches<int32>(7)), 1, _))
    .WillOnce(DoAll(SetArgumentPointee<3>(variant_),
                    Return(true)));

  double r;
  EXPECT_TRUE(NPInvoke(NULL, object, "foo", 7, &r));
  EXPECT_EQ(1.5, r);
}

TEST_F(NPUtilsTest, InvokeNonVoidMethodCanFail) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("foo");
  NPObjectPointer<MockNPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  DOUBLE_TO_NPVARIANT(1.5, variant_);

  EXPECT_CALL(*object, Invoke(name, Pointee(VariantMatches<int32>(7)), 1, _))
    .WillOnce(DoAll(SetArgumentPointee<3>(variant_),
                    Return(false)));

  double r;
  EXPECT_FALSE(NPInvoke(NULL, object, "foo", 7, &r));
}

TEST_F(NPUtilsTest, InvokeNonVoidMethodFailsIfResultIsIncompatible) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("foo");
  NPObjectPointer<MockNPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  DOUBLE_TO_NPVARIANT(1.5, variant_);

  EXPECT_CALL(*object, Invoke(name, Pointee(VariantMatches<int32>(7)), 1, _))
    .WillOnce(DoAll(SetArgumentPointee<3>(variant_),
                    Return(true)));

  int r;
  EXPECT_FALSE(NPInvoke(NULL, object, "foo", 7, &r));
}

TEST_F(NPUtilsTest, CanDetermineIfObjectHasProperty) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("foo");
  NPObjectPointer<MockNPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  EXPECT_CALL(*object, HasProperty(name))
    .WillOnce(Return(true));

  EXPECT_TRUE(NPHasProperty(NULL, object, "foo"));
}

TEST_F(NPUtilsTest, CanGetPropertyValue) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("foo");
  NPObjectPointer<MockNPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  DOUBLE_TO_NPVARIANT(1.5, variant_);

  EXPECT_CALL(*object, GetProperty(name, _))
    .WillOnce(DoAll(SetArgumentPointee<1>(variant_),
                    Return(true)));

  double r;
  EXPECT_TRUE(NPGetProperty(NULL, object, "foo", &r));
}

TEST_F(NPUtilsTest, NPGetPropertyReportsFailureIfResultTypeIsDifferent) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("foo");
  NPObjectPointer<MockNPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  DOUBLE_TO_NPVARIANT(1.5, variant_);

  EXPECT_CALL(*object, GetProperty(name, _))
    .WillOnce(DoAll(SetArgumentPointee<1>(variant_),
                    Return(true)));

  bool r;
  EXPECT_FALSE(NPGetProperty(NULL, object, "foo", &r));
}

TEST_F(NPUtilsTest, NPGetPropertyReportsFailureFromGetProperty) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("foo");
  NPObjectPointer<MockNPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  EXPECT_CALL(*object, GetProperty(name, _))
    .WillOnce(Return(false));

  double r;
  EXPECT_FALSE(NPGetProperty(NULL, object, "foo", &r));
}

TEST_F(NPUtilsTest, CanSetPropertyValue) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("foo");
  NPObjectPointer<MockNPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  EXPECT_CALL(*object, SetProperty(name, Pointee(VariantMatches(1.5))))
    .WillOnce(Return(true));

  EXPECT_TRUE(NPSetProperty(NULL, object, "foo", 1.5));
}

TEST_F(NPUtilsTest, NPSetPropertyReportsFailureFromSetProperty) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("foo");
  NPObjectPointer<MockNPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  EXPECT_CALL(*object, SetProperty(name, Pointee(VariantMatches(1.5))))
    .WillOnce(Return(false));

  EXPECT_FALSE(NPSetProperty(NULL, object, "foo", 1.5));
}

TEST_F(NPUtilsTest, CanRemovePropertyValue) {
  NPIdentifier name = NPBrowser::get()->GetStringIdentifier("foo");
  NPObjectPointer<MockNPObject> object =
      NPCreateObject<StrictMock<MockNPObject> >(NULL);

  EXPECT_CALL(*object, RemoveProperty(name))
    .WillOnce(Return(true));

  EXPECT_TRUE(NPRemoveProperty(NULL, object, "foo"));
}

}  // namespace np_utils
