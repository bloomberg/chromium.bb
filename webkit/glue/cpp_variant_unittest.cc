// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "webkit/glue/cpp_variant.h"

using WebKit::WebBindings;
using webkit_glue::CppVariant;

// Creates a std::string from an NPVariant of string type.  If the NPVariant
// is not a string, empties the std::string.
void MakeStdString(const NPVariant& np, std::string* std_string) {
  if (np.type == NPVariantType_String) {
    const char* chars =
        reinterpret_cast<const char*>(np.value.stringValue.UTF8Characters);
    (*std_string).assign(chars, np.value.stringValue.UTF8Length);
  } else {
    (*std_string).clear();
  }
}

// Verifies that the actual NPVariant is a string and that its value matches
// the expected_str.
void CheckString(const std::string& expected_str, const NPVariant& actual) {
  EXPECT_EQ(NPVariantType_String, actual.type);
  std::string actual_str;
  MakeStdString(actual, &actual_str);
  EXPECT_EQ(expected_str, actual_str);
}

// Verifies that both the actual and the expected NPVariants are strings and
// that their values match.
void CheckString(const NPVariant& expected, const NPVariant& actual) {
  EXPECT_EQ(NPVariantType_String, expected.type);
  std::string expected_str;
  MakeStdString(expected, &expected_str);
  CheckString(expected_str, actual);
}

int g_allocate_call_count = 0;
int g_deallocate_call_count = 0;

void CheckObject(const NPVariant& actual) {
  EXPECT_EQ(NPVariantType_Object, actual.type);
  EXPECT_TRUE(actual.value.objectValue);
  EXPECT_EQ(1U, actual.value.objectValue->referenceCount);
  EXPECT_EQ(1, g_allocate_call_count);
  EXPECT_EQ(0, g_deallocate_call_count);
}

NPObject* MockNPAllocate(NPP npp, NPClass* aClass) {
  // This is a mock allocate method that mimics the behavior
  // of WebBindings::createObject when allocate() is NULL

  ++g_allocate_call_count;
  // Ignore npp and NPClass
  return reinterpret_cast<NPObject*>(malloc(sizeof(NPObject)));
}

void MockNPDeallocate(NPObject* npobj) {
  // This is a mock deallocate method that mimics the behavior
  // of NPN_DeallocateObject when deallocate() is NULL

  ++g_deallocate_call_count;
  free(npobj);
}

static NPClass void_class = { NP_CLASS_STRUCT_VERSION,
                              MockNPAllocate,
                              MockNPDeallocate,
                              0, 0, 0, 0, 0, 0, 0, 0, 0 };

NPObject* MakeVoidObject() {
  g_allocate_call_count = 0;
  g_deallocate_call_count = 0;
  return WebBindings::createObject(NULL, &void_class);
}

TEST(CppVariantTest, NewVariantHasNullType) {
  CppVariant value;
  EXPECT_EQ(NPVariantType_Null, value.type);
}

TEST(CppVariantTest, SetNullSetsType) {
  CppVariant value;
  value.Set(17);
  value.SetNull();
  EXPECT_EQ(NPVariantType_Null, value.type);
}

TEST(CppVariantTest, CopyConstructorDoesDeepCopy) {
  CppVariant source;
  source.Set("test string");
  CppVariant dest = source;
  EXPECT_EQ(NPVariantType_String, dest.type);
  EXPECT_EQ(NPVariantType_String, source.type);

  // Ensure that the string was copied, not just the pointer.
  EXPECT_NE(source.value.stringValue.UTF8Characters,
    dest.value.stringValue.UTF8Characters);

  CheckString(source, dest);
}

TEST(CppVariantTest, CopyConstructorIncrementsRefCount) {
  CppVariant source;
  NPObject *object = MakeVoidObject();
  source.Set(object);
  // 2 references so far.
  EXPECT_EQ(2U, source.value.objectValue->referenceCount);

  CppVariant dest = source;
  EXPECT_EQ(3U, dest.value.objectValue->referenceCount);
  EXPECT_EQ(1, g_allocate_call_count);
  WebBindings::releaseObject(object);
  source.SetNull();
  CheckObject(dest);
}

TEST(CppVariantTest, AssignmentDoesDeepCopy) {
  CppVariant source;
  source.Set("test string");
  CppVariant dest;
  dest = source;
  EXPECT_EQ(NPVariantType_String, dest.type);
  EXPECT_EQ(NPVariantType_String, source.type);

  // Ensure that the string was copied, not just the pointer.
  EXPECT_NE(source.value.stringValue.UTF8Characters,
    dest.value.stringValue.UTF8Characters);

  CheckString(source, dest);
}

TEST(CppVariantTest, AssignmentIncrementsRefCount) {
  CppVariant source;
  NPObject *object = MakeVoidObject();
  source.Set(object);
  // 2 references so far.
  EXPECT_EQ(2U, source.value.objectValue->referenceCount);

  CppVariant dest;
  dest = source;
  EXPECT_EQ(3U, dest.value.objectValue->referenceCount);
  EXPECT_EQ(1, g_allocate_call_count);

  WebBindings::releaseObject(object);
  source.SetNull();
  CheckObject(dest);
}

TEST(CppVariantTest, DestroyingCopyDoesNotCorruptSource) {
  CppVariant source;
  source.Set("test string");
  std::string before;
  MakeStdString(source, &before);
  {
    CppVariant dest = source;
  }
  CheckString(before, source);

  NPObject *object = MakeVoidObject();
  source.Set(object);
  {
    CppVariant dest2 = source;
  }
  WebBindings::releaseObject(object);
  CheckObject(source);
}

TEST(CppVariantTest, CopiesTypeAndValueToNPVariant) {
  NPVariant np;
  CppVariant cpp;

  cpp.Set(true);
  cpp.CopyToNPVariant(&np);
  EXPECT_EQ(cpp.type, np.type);
  EXPECT_EQ(cpp.value.boolValue, np.value.boolValue);
  WebBindings::releaseVariantValue(&np);

  cpp.Set(17);
  cpp.CopyToNPVariant(&np);
  EXPECT_EQ(cpp.type, np.type);
  EXPECT_EQ(cpp.value.intValue, np.value.intValue);
  WebBindings::releaseVariantValue(&np);

  cpp.Set(3.1415);
  cpp.CopyToNPVariant(&np);
  EXPECT_EQ(cpp.type, np.type);
  EXPECT_EQ(cpp.value.doubleValue, np.value.doubleValue);
  WebBindings::releaseVariantValue(&np);

  cpp.Set("test value");
  cpp.CopyToNPVariant(&np);
  CheckString("test value", np);
  WebBindings::releaseVariantValue(&np);

  cpp.SetNull();
  cpp.CopyToNPVariant(&np);
  EXPECT_EQ(cpp.type, np.type);
  WebBindings::releaseVariantValue(&np);

  NPObject *object = MakeVoidObject();
  cpp.Set(object);
  cpp.CopyToNPVariant(&np);
  WebBindings::releaseObject(object);
  cpp.SetNull();
  CheckObject(np);
  WebBindings::releaseVariantValue(&np);
}

TEST(CppVariantTest, SetsTypeAndValueFromNPVariant) {
  NPVariant np;
  CppVariant cpp;

  VOID_TO_NPVARIANT(np);
  cpp.Set(np);
  EXPECT_EQ(np.type, cpp.type);
  WebBindings::releaseVariantValue(&np);

  NULL_TO_NPVARIANT(np);
  cpp.Set(np);
  EXPECT_EQ(np.type, cpp.type);
  WebBindings::releaseVariantValue(&np);

  BOOLEAN_TO_NPVARIANT(true, np);
  cpp.Set(np);
  EXPECT_EQ(np.type, cpp.type);
  EXPECT_EQ(np.value.boolValue, cpp.value.boolValue);
  WebBindings::releaseVariantValue(&np);

  INT32_TO_NPVARIANT(15, np);
  cpp.Set(np);
  EXPECT_EQ(np.type, cpp.type);
  EXPECT_EQ(np.value.intValue, cpp.value.intValue);
  WebBindings::releaseVariantValue(&np);

  DOUBLE_TO_NPVARIANT(2.71828, np);
  cpp.Set(np);
  EXPECT_EQ(np.type, cpp.type);
  EXPECT_EQ(np.value.doubleValue, cpp.value.doubleValue);
  WebBindings::releaseVariantValue(&np);

  NPString np_ascii_str = { "1st test value",
                            static_cast<uint32_t>(strlen("1st test value")) };
  WebBindings::initializeVariantWithStringCopy(&np, &np_ascii_str);
  cpp.Set(np);
  CheckString("1st test value", cpp);
  WebBindings::releaseVariantValue(&np);

  // Test characters represented in 2/3/4 bytes in UTF-8
  // Greek alpha, Chinese number 1 (horizontal bar),
  // Deseret letter (similar to 'O')
  NPString np_intl_str = { "\xce\xb1\xe4\xb8\x80\xf0\x90\x90\x84",
                           static_cast<uint32_t>(strlen(
                               "\xce\xb1\xe4\xb8\x80\xf0\x90\x90\x84")) };
  WebBindings::initializeVariantWithStringCopy(&np, &np_intl_str);
  cpp.Set(np);
  CheckString("\xce\xb1\xe4\xb8\x80\xf0\x90\x90\x84", cpp);
  WebBindings::releaseVariantValue(&np);

  NPObject *obj = MakeVoidObject();
  OBJECT_TO_NPVARIANT(obj, np);  // Doesn't make a copy.
  cpp.Set(np);
  // Use this or WebBindings::releaseObject but NOT both.
  WebBindings::releaseVariantValue(&np);
  CheckObject(cpp);
}

TEST(CppVariantTest, SetsSimpleTypesAndValues) {
  CppVariant cpp;
  cpp.Set(true);
  EXPECT_EQ(NPVariantType_Bool, cpp.type);
  EXPECT_TRUE(cpp.value.boolValue);

  cpp.Set(5);
  EXPECT_EQ(NPVariantType_Int32, cpp.type);
  EXPECT_EQ(5, cpp.value.intValue);

  cpp.Set(1.234);
  EXPECT_EQ(NPVariantType_Double, cpp.type);
  EXPECT_EQ(1.234, cpp.value.doubleValue);

  // C string
  cpp.Set("1st test string");
  CheckString("1st test string", cpp);

  // std::string
  std::string source("std test string");
  cpp.Set(source);
  CheckString("std test string", cpp);

  // NPString
  NPString np_ascii_str = { "test NPString",
                            static_cast<uint32_t>(strlen("test NPString")) };
  cpp.Set(np_ascii_str);
  std::string expected("test NPString");
  CheckString(expected, cpp);

  // Test characters represented in 2/3/4 bytes in UTF-8
  // Greek alpha, Chinese number 1 (horizontal bar),
  // Deseret letter (similar to 'O')
  NPString np_intl_str = { "\xce\xb1\xe4\xb8\x80\xf0\x90\x90\x84",
                           static_cast<uint32_t>(strlen(
                               "\xce\xb1\xe4\xb8\x80\xf0\x90\x90\x84")) };
  cpp.Set(np_intl_str);
  expected = std::string("\xce\xb1\xe4\xb8\x80\xf0\x90\x90\x84");
  CheckString(expected, cpp);

  NPObject* obj = MakeVoidObject();
  cpp.Set(obj);
  WebBindings::releaseObject(obj);
  CheckObject(cpp);
}

TEST(CppVariantTest, FreeDataSetsToVoid) {
  CppVariant cpp;
  EXPECT_EQ(NPVariantType_Null, cpp.type);
  cpp.Set(12);
  EXPECT_EQ(NPVariantType_Int32, cpp.type);
  cpp.FreeData();
  EXPECT_EQ(NPVariantType_Void, cpp.type);
}

TEST(CppVariantTest, FreeDataReleasesObject) {
  CppVariant cpp;
  NPObject* object = MakeVoidObject();
  cpp.Set(object);
  EXPECT_EQ(2U, object->referenceCount);
  cpp.FreeData();
  EXPECT_EQ(1U, object->referenceCount);
  EXPECT_EQ(0, g_deallocate_call_count);

  cpp.Set(object);
  WebBindings::releaseObject(object);
  EXPECT_EQ(0, g_deallocate_call_count);
  cpp.FreeData();
  EXPECT_EQ(1, g_deallocate_call_count);
}

TEST(CppVariantTest, IsTypeFunctionsWork) {
  CppVariant cpp;
  // These should not happen in practice, since voids are not supported
  // This test must be first since it just clobbers internal data without
  // releasing.
  VOID_TO_NPVARIANT(cpp);
  EXPECT_FALSE(cpp.isBool());
  EXPECT_FALSE(cpp.isInt32());
  EXPECT_FALSE(cpp.isDouble());
  EXPECT_FALSE(cpp.isNumber());
  EXPECT_FALSE(cpp.isString());
  EXPECT_TRUE(cpp.isVoid());
  EXPECT_FALSE(cpp.isNull());
  EXPECT_TRUE(cpp.isEmpty());

  cpp.Set(true);
  EXPECT_TRUE(cpp.isBool());
  EXPECT_FALSE(cpp.isInt32());
  EXPECT_FALSE(cpp.isDouble());
  EXPECT_FALSE(cpp.isNumber());
  EXPECT_FALSE(cpp.isString());
  EXPECT_FALSE(cpp.isVoid());
  EXPECT_FALSE(cpp.isNull());
  EXPECT_FALSE(cpp.isEmpty());
  EXPECT_FALSE(cpp.isObject());

  cpp.Set(12);
  EXPECT_FALSE(cpp.isBool());
  EXPECT_TRUE(cpp.isInt32());
  EXPECT_FALSE(cpp.isDouble());
  EXPECT_TRUE(cpp.isNumber());
  EXPECT_FALSE(cpp.isString());
  EXPECT_FALSE(cpp.isVoid());
  EXPECT_FALSE(cpp.isNull());
  EXPECT_FALSE(cpp.isEmpty());
  EXPECT_FALSE(cpp.isObject());

  cpp.Set(3.1415);
  EXPECT_FALSE(cpp.isBool());
  EXPECT_FALSE(cpp.isInt32());
  EXPECT_TRUE(cpp.isDouble());
  EXPECT_TRUE(cpp.isNumber());
  EXPECT_FALSE(cpp.isString());
  EXPECT_FALSE(cpp.isVoid());
  EXPECT_FALSE(cpp.isNull());
  EXPECT_FALSE(cpp.isEmpty());
  EXPECT_FALSE(cpp.isObject());

  cpp.Set("a string");
  EXPECT_FALSE(cpp.isBool());
  EXPECT_FALSE(cpp.isInt32());
  EXPECT_FALSE(cpp.isDouble());
  EXPECT_FALSE(cpp.isNumber());
  EXPECT_TRUE(cpp.isString());
  EXPECT_FALSE(cpp.isVoid());
  EXPECT_FALSE(cpp.isNull());
  EXPECT_FALSE(cpp.isEmpty());
  EXPECT_FALSE(cpp.isObject());

  cpp.SetNull();
  EXPECT_FALSE(cpp.isBool());
  EXPECT_FALSE(cpp.isInt32());
  EXPECT_FALSE(cpp.isDouble());
  EXPECT_FALSE(cpp.isNumber());
  EXPECT_FALSE(cpp.isString());
  EXPECT_FALSE(cpp.isVoid());
  EXPECT_TRUE(cpp.isNull());
  EXPECT_TRUE(cpp.isEmpty());
  EXPECT_FALSE(cpp.isObject());

  NPObject *obj = MakeVoidObject();
  cpp.Set(obj);
  EXPECT_FALSE(cpp.isBool());
  EXPECT_FALSE(cpp.isInt32());
  EXPECT_FALSE(cpp.isDouble());
  EXPECT_FALSE(cpp.isNumber());
  EXPECT_FALSE(cpp.isString());
  EXPECT_FALSE(cpp.isVoid());
  EXPECT_FALSE(cpp.isNull());
  EXPECT_FALSE(cpp.isEmpty());
  EXPECT_TRUE(cpp.isObject());
  WebBindings::releaseObject(obj);
  CheckObject(cpp);
}

bool MockNPHasPropertyFunction(NPObject *npobj, NPIdentifier name) {
  return true;
}

bool MockNPGetPropertyFunction(NPObject *npobj, NPIdentifier name,
                               NPVariant *result) {
  if (WebBindings::getStringIdentifier("length") == name) {
    DOUBLE_TO_NPVARIANT(4, *result);
  } else if (WebBindings::getIntIdentifier(0) == name) {
    DOUBLE_TO_NPVARIANT(0, *result);
  } else if (WebBindings::getIntIdentifier(1) == name) {
    BOOLEAN_TO_NPVARIANT(true, *result);
  } else if (WebBindings::getIntIdentifier(2) == name) {
    NULL_TO_NPVARIANT(*result);
  } else if (WebBindings::getIntIdentifier(3) == name) {
    const char* s = "string";
    size_t length = strlen(s);
    char* mem = static_cast<char*>(malloc(length + 1));
    base::strlcpy(mem, s, length + 1);
    STRINGZ_TO_NPVARIANT(mem, *result);
  }

  return true;
}

TEST(CppVariantTest, ToVector) {
  NPClass array_like_class = {
      NP_CLASS_STRUCT_VERSION,
      0, // NPAllocateFunctionPtr allocate;
      0, // NPDeallocateFunctionPtr deallocate;
      0, // NPInvalidateFunctionPtr invalidate;
      0, // NPHasMethodFunctionPtr hasMethod;
      0, // NPInvokeFunctionPtr invoke;
      0, // NPInvokeDefaultFunctionPtr invokeDefault;
      MockNPHasPropertyFunction, // NPHasPropertyFunctionPtr hasProperty;
      MockNPGetPropertyFunction, // NPGetPropertyFunctionPtr getProperty;
      0, // NPSetPropertyFunctionPtr setProperty;
      0, // NPRemovePropertyFunctionPtr removeProperty;
      0, // NPEnumerationFunctionPtr enumerate;
      0 // NPConstructFunctionPtr construct;
      };

  NPObject* obj = WebBindings::createObject(NULL, &array_like_class);

  CppVariant cpp;
  cpp.Set(obj);

  std::vector<CppVariant> cpp_vector = cpp.ToVector();
  EXPECT_EQ(4u, cpp_vector.size());

  EXPECT_TRUE(cpp_vector[0].isDouble());
  EXPECT_EQ(0, cpp_vector[0].ToDouble());

  EXPECT_TRUE(cpp_vector[1].isBool());
  EXPECT_EQ(true, cpp_vector[1].ToBoolean());

  EXPECT_TRUE(cpp_vector[2].isNull());

  EXPECT_TRUE(cpp_vector[3].isString());
  CheckString("string", cpp_vector[3]);

  WebBindings::releaseObject(obj);
}
