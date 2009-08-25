// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "o3d/gpu_plugin/np_utils/dispatched_np_object.h"
#include "o3d/gpu_plugin/np_utils/np_dispatcher.h"
#include "o3d/gpu_plugin/np_utils/npn_test_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::StrictMock;

namespace o3d {
namespace gpu_plugin {

// This mock class has a dispatcher chain with an entry for each mocked
// function. The tests that follow that invoking an NPAPI method calls the
// corresponding mocked member function.
class MockDispatchedNPObject : public DispatchedNPObject {
 public:
  MockDispatchedNPObject()
      : DispatchedNPObject(NULL) {
  }

  MOCK_METHOD0(VoidReturnNoParams, void());
  MOCK_METHOD1(VoidReturnBoolParam, void(bool));
  MOCK_METHOD1(VoidReturnIntParam, void(int));
  MOCK_METHOD1(VoidReturnFloatParam, void(float));
  MOCK_METHOD1(VoidReturnDoubleParam, void(double));
  MOCK_METHOD1(VoidReturnStringParam, void(std::string));
  MOCK_METHOD1(VoidReturnObjectParam, void(NPObject*));
  MOCK_METHOD2(VoidReturnTwoParams, void(bool, int));
  MOCK_METHOD0(Overloaded, void());
  MOCK_METHOD1(Overloaded, void(bool));
  MOCK_METHOD1(Overloaded, void(std::string));
  MOCK_METHOD0(BoolReturn, bool());
  MOCK_METHOD0(IntReturn, int());
  MOCK_METHOD0(FloatReturn, float());
  MOCK_METHOD0(DoubleReturn, double());
  MOCK_METHOD0(StringReturn, std::string());
  MOCK_METHOD0(ObjectReturn, NPObject*());

 protected:
  NP_UTILS_BEGIN_DISPATCHER_CHAIN(MockDispatchedNPObject, DispatchedNPObject)
    NP_UTILS_DISPATCHER(VoidReturnNoParams, void())
    NP_UTILS_DISPATCHER(VoidReturnBoolParam, void(bool))
    NP_UTILS_DISPATCHER(VoidReturnIntParam, void(int))
    NP_UTILS_DISPATCHER(VoidReturnFloatParam, void(float))
    NP_UTILS_DISPATCHER(VoidReturnDoubleParam, void(double))
    NP_UTILS_DISPATCHER(VoidReturnStringParam, void(std::string))
    NP_UTILS_DISPATCHER(VoidReturnObjectParam, void(NPObject*))
    NP_UTILS_DISPATCHER(VoidReturnTwoParams, void(bool, int))
    NP_UTILS_DISPATCHER(Overloaded, void())
    NP_UTILS_DISPATCHER(Overloaded, void(bool))
    NP_UTILS_DISPATCHER(Overloaded, void(std::string))
    NP_UTILS_DISPATCHER(BoolReturn, bool())
    NP_UTILS_DISPATCHER(IntReturn, int())
    NP_UTILS_DISPATCHER(FloatReturn, float())
    NP_UTILS_DISPATCHER(DoubleReturn, double())
    NP_UTILS_DISPATCHER(StringReturn, std::string())
    NP_UTILS_DISPATCHER(ObjectReturn, NPObject*());
  NP_UTILS_END_DISPATCHER_CHAIN
};

class NPObjectDispatcherTest : public testing::Test {
 protected:
  virtual void SetUp() {
    InitializeNPNTestStub();
    
    object_ = new StrictMock<MockDispatchedNPObject>;

    for (int i = 0; i != arraysize(args_); ++i) {
      NULL_TO_NPVARIANT(args_[i]);
    }
    NULL_TO_NPVARIANT(result_);
  }

  virtual void TearDown() {
    delete object_;
    ShutdownNPNTestStub();
  }

  NPVariant args_[3];
  NPVariant result_;
  MockDispatchedNPObject* object_;
};

TEST_F(NPObjectDispatcherTest, CannotInvokeMissingFunction) {
  EXPECT_FALSE(object_->Invoke(NPN_GetStringIdentifier("missing"),
               NULL,
               0,
               &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeVoidReturnNoParams) {
  EXPECT_CALL(*object_, VoidReturnNoParams());

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("voidReturnNoParams"),
                              NULL,
                              0,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest,
       CannotInvokeVoidReturnNoParamsWithTooManyParams) {
  EXPECT_FALSE(object_->Invoke(NPN_GetStringIdentifier("voidReturnNoParams"),
                               args_,
                               1,
                               &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeVoidReturnIntParam) {
  EXPECT_CALL(*object_, VoidReturnIntParam(7));

  INT32_TO_NPVARIANT(7, args_[0]);

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("voidReturnIntParam"),
                              args_,
                              1,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeVoidReturnBoolParam) {
  EXPECT_CALL(*object_, VoidReturnBoolParam(true));

  BOOLEAN_TO_NPVARIANT(true, args_[0]);

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("voidReturnBoolParam"),
                              args_,
                              1,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeVoidReturnFloatParamWithDoubleParam) {
  EXPECT_CALL(*object_, VoidReturnFloatParam(7.0f));

  DOUBLE_TO_NPVARIANT(7.0, args_[0]);

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("voidReturnFloatParam"),
                              args_,
                              1,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeVoidReturnFloatParamWithIntParam) {
  EXPECT_CALL(*object_, VoidReturnFloatParam(7.0f));

  INT32_TO_NPVARIANT(7, args_[0]);

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("voidReturnFloatParam"),
                              args_,
                              1,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeVoidReturnDoubleParamWithDoubleParam) {
  EXPECT_CALL(*object_, VoidReturnDoubleParam(7.0));

  DOUBLE_TO_NPVARIANT(7.0, args_[0]);

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("voidReturnDoubleParam"),
                              args_,
                              1,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeVoidReturnDoubleParamWithIntParam) {
  EXPECT_CALL(*object_, VoidReturnDoubleParam(7.0f));

  INT32_TO_NPVARIANT(7, args_[0]);

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("voidReturnDoubleParam"),
                              args_,
                              1,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeVoidReturnStringParam) {
  EXPECT_CALL(*object_, VoidReturnStringParam(std::string("hello")));

  STRINGZ_TO_NPVARIANT("hello", args_[0]);

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("voidReturnStringParam"),
                              args_,
                              1,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeVoidReturnObjectParamWithObject) {
  EXPECT_CALL(*object_, VoidReturnObjectParam(object_));

  OBJECT_TO_NPVARIANT(object_, args_[0]);

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("voidReturnObjectParam"),
                              args_,
                              1,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeVoidReturnObjectParamWithNull) {
  EXPECT_CALL(*object_, VoidReturnObjectParam(NULL));

  NULL_TO_NPVARIANT(args_[0]);

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("voidReturnObjectParam"),
                              args_,
                              1,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeVoidReturnTwoParams) {
  EXPECT_CALL(*object_, VoidReturnTwoParams(false, 7));

  BOOLEAN_TO_NPVARIANT(false, args_[0]);
  INT32_TO_NPVARIANT(7, args_[1]);

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("voidReturnTwoParams"),
                              args_,
                              2,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeOverloadedWithNoParams) {
  EXPECT_CALL(*object_, Overloaded());

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("overloaded"),
                              NULL,
                              0,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeOverloadedWithOneStringParam) {
  EXPECT_CALL(*object_, Overloaded(std::string("hello")));

  STRINGZ_TO_NPVARIANT("hello", args_[0]);

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("overloaded"),
                              args_,
                              1,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeOverloadedWithOneBoolParam) {
  EXPECT_CALL(*object_, Overloaded(true));

  BOOLEAN_TO_NPVARIANT(true, args_[0]);

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("overloaded"),
                              args_,
                              1,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_VOID(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeBoolReturn) {
  EXPECT_CALL(*object_, BoolReturn()).WillOnce(Return(true));

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("boolReturn"),
                              NULL,
                              0,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_BOOLEAN(result_));
  EXPECT_TRUE(NPVARIANT_TO_BOOLEAN(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeIntReturn) {
  EXPECT_CALL(*object_, IntReturn()).WillOnce(Return(7));

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("intReturn"),
                              NULL,
                              0,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_INT32(result_));
  EXPECT_EQ(7, NPVARIANT_TO_INT32(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeFloatReturn) {
  EXPECT_CALL(*object_, FloatReturn()).WillOnce(Return(7.0f));

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("floatReturn"),
                              NULL,
                              0,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_DOUBLE(result_));
  EXPECT_EQ(7.0, NPVARIANT_TO_DOUBLE(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeDoubleReturn) {
  EXPECT_CALL(*object_, DoubleReturn()).WillOnce(Return(7.0));

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("doubleReturn"),
                              NULL,
                              0,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_DOUBLE(result_));
  EXPECT_EQ(7.0, NPVARIANT_TO_DOUBLE(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeStringReturn) {
  EXPECT_CALL(*object_, StringReturn()).WillOnce(Return(std::string("hello")));

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("stringReturn"),
                              NULL,
                              0,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_STRING(result_));

  NPString& str = NPVARIANT_TO_STRING(result_);
  EXPECT_EQ(std::string("hello"),
            std::string(str.UTF8Characters, str.UTF8Length));

  // Callee is responsible for releasing string.
  gpu_plugin::NPN_ReleaseVariantValue(&result_);
}

TEST_F(NPObjectDispatcherTest, CanInvokeObjectReturnWithObject) {
  EXPECT_CALL(*object_, ObjectReturn()).WillOnce(Return(object_));

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("objectReturn"),
                              NULL,
                              0,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_OBJECT(result_));
  EXPECT_EQ(object_, NPVARIANT_TO_OBJECT(result_));
}

TEST_F(NPObjectDispatcherTest, CanInvokeObjectReturnWithNull) {
  EXPECT_CALL(*object_, ObjectReturn())
      .WillOnce(Return(static_cast<NPObject*>(NULL)));

  EXPECT_TRUE(object_->Invoke(NPN_GetStringIdentifier("objectReturn"),
                              NULL,
                              0,
                              &result_));
  EXPECT_TRUE(NPVARIANT_IS_NULL(result_));
}

TEST_F(NPObjectDispatcherTest, HasMethodReturnsTrueIfMatchingMemberVariable) {
  EXPECT_TRUE(object_->HasMethod(NPN_GetStringIdentifier("objectReturn")));
}

TEST_F(NPObjectDispatcherTest, HasMethodReturnsTrueIfNoMatchingMemberVariable) {
  EXPECT_FALSE(object_->HasMethod(NPN_GetStringIdentifier("missing")));
}

TEST_F(NPObjectDispatcherTest, EnumeratesAllAvailableMethods) {
  NPIdentifier* names;
  uint32_t num_names;
  ASSERT_TRUE(object_->Enumerate(&names, &num_names));

  // Don't compare all of them; this test would need to change every time new
  // dispatchers were added to the test NPObject class. Just compare the first
  // registered (last in the dispatcher chain) and that more than one is
  // returned.
  EXPECT_GT(num_names, 1u);
  EXPECT_EQ(NPN_GetStringIdentifier("voidReturnNoParams"),
                                    names[num_names - 1]);

  NPN_MemFree(names);
}

}  // namespace gpu_plugin
}  // namespace o3d
