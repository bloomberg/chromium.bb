// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/SerializedScriptValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8File.h"
#include "core/fileapi/File.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(SerializedScriptValueTest, UserSelectedFile) {
  V8TestingScope scope;
  String file_path = testing::BlinkRootDir();
  file_path.Append("/Source/bindings/core/v8/SerializedScriptValueTest.cpp");
  File* original_file = File::Create(file_path);
  ASSERT_TRUE(original_file->HasBackingFile());
  ASSERT_EQ(File::kIsUserVisible, original_file->GetUserVisibility());
  ASSERT_EQ(file_path, original_file->GetPath());

  v8::Local<v8::Value> v8_original_file =
      ToV8(original_file, scope.GetContext()->Global(), scope.GetIsolate());
  RefPtr<SerializedScriptValue> serialized_script_value =
      SerializedScriptValue::Serialize(
          scope.GetIsolate(), v8_original_file,
          SerializedScriptValue::SerializeOptions(), ASSERT_NO_EXCEPTION);
  v8::Local<v8::Value> v8_file =
      serialized_script_value->Deserialize(scope.GetIsolate());

  ASSERT_TRUE(V8File::hasInstance(v8_file, scope.GetIsolate()));
  File* file = V8File::toImpl(v8::Local<v8::Object>::Cast(v8_file));
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ(File::kIsUserVisible, file->GetUserVisibility());
  EXPECT_EQ(file_path, file->GetPath());
}

TEST(SerializedScriptValueTest, FileConstructorFile) {
  V8TestingScope scope;
  RefPtr<BlobDataHandle> blob_data_handle = BlobDataHandle::Create();
  File* original_file = File::Create("hello.txt", 12345678.0, blob_data_handle);
  ASSERT_FALSE(original_file->HasBackingFile());
  ASSERT_EQ(File::kIsNotUserVisible, original_file->GetUserVisibility());
  ASSERT_EQ("hello.txt", original_file->name());

  v8::Local<v8::Value> v8_original_file =
      ToV8(original_file, scope.GetContext()->Global(), scope.GetIsolate());
  RefPtr<SerializedScriptValue> serialized_script_value =
      SerializedScriptValue::Serialize(
          scope.GetIsolate(), v8_original_file,
          SerializedScriptValue::SerializeOptions(), ASSERT_NO_EXCEPTION);
  v8::Local<v8::Value> v8_file =
      serialized_script_value->Deserialize(scope.GetIsolate());

  ASSERT_TRUE(V8File::hasInstance(v8_file, scope.GetIsolate()));
  File* file = V8File::toImpl(v8::Local<v8::Object>::Cast(v8_file));
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_EQ(File::kIsNotUserVisible, file->GetUserVisibility());
  EXPECT_EQ("hello.txt", file->name());
}

}  // namespace blink
