// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/android/policy_converter.h"

#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "base/json/json_reader.h"
#include "base/notreached.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/policy/android/jni_headers/PolicyConverter_jni.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaRef;

namespace policy {
namespace android {

PolicyConverter::PolicyConverter(const Schema* policy_schema)
    : policy_schema_(policy_schema), policy_bundle_(new PolicyBundle) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env,
      Java_PolicyConverter_create(env, reinterpret_cast<intptr_t>(this)).obj());
  DCHECK(!java_obj_.is_null());
}

PolicyConverter::~PolicyConverter() {
  Java_PolicyConverter_onNativeDestroyed(base::android::AttachCurrentThread(),
                                         java_obj_);
}

std::unique_ptr<PolicyBundle> PolicyConverter::GetPolicyBundle() {
  std::unique_ptr<PolicyBundle> filled_bundle(std::move(policy_bundle_));
  policy_bundle_.reset(new PolicyBundle);
  return filled_bundle;
}

base::android::ScopedJavaLocalRef<jobject> PolicyConverter::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

void PolicyConverter::SetPolicyBoolean(JNIEnv* env,
                                       const JavaRef<jobject>& obj,
                                       const JavaRef<jstring>& policyKey,
                                       jboolean value) {
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey),
                 base::Value(static_cast<bool>(value)));
}

void PolicyConverter::SetPolicyInteger(JNIEnv* env,
                                       const JavaRef<jobject>& obj,
                                       const JavaRef<jstring>& policyKey,
                                       jint value) {
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey),
                 base::Value(static_cast<int>(value)));
}

void PolicyConverter::SetPolicyString(JNIEnv* env,
                                      const JavaRef<jobject>& obj,
                                      const JavaRef<jstring>& policyKey,
                                      const JavaRef<jstring>& value) {
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey),
                 base::Value(ConvertJavaStringToUTF8(env, value)));
}

void PolicyConverter::SetPolicyStringArray(JNIEnv* env,
                                           const JavaRef<jobject>& obj,
                                           const JavaRef<jstring>& policyKey,
                                           const JavaRef<jobjectArray>& array) {
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey),
                 ConvertJavaStringArrayToListValue(env, array));
}

// static
base::Value PolicyConverter::ConvertJavaStringArrayToListValue(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array) {
  DCHECK(!array.is_null());
  base::android::JavaObjectArrayReader<jstring> array_reader(array);
  DCHECK_GE(array_reader.size(), 0)
      << "Invalid array length: " << array_reader.size();

  base::Value list_value(base::Value::Type::LIST);
  for (auto j_str : array_reader)
    list_value.Append(ConvertJavaStringToUTF8(env, j_str));

  return list_value;
}

// static
base::Optional<base::Value> PolicyConverter::ConvertValueToSchema(
    base::Value value,
    const Schema& schema) {
  if (!schema.valid())
    return value;

  switch (schema.type()) {
    case base::Value::Type::NONE:
      return base::Value();

    case base::Value::Type::BOOLEAN: {
      std::string string_value;
      if (value.is_string()) {
        const std::string& string_value = value.GetString();
        if (string_value.compare("true") == 0)
          return base::Value(true);

        if (string_value.compare("false") == 0)
          return base::Value(false);

        return value;
      }
      if (value.is_int())
        return base::Value(value.GetInt() != 0);

      return value;
    }

    case base::Value::Type::INTEGER: {
      if (value.is_string()) {
        const std::string& string_value = value.GetString();
        int int_value = 0;
        if (base::StringToInt(string_value, &int_value))
          return base::Value(int_value);
      }
      return value;
    }

    case base::Value::Type::DOUBLE: {
      if (value.is_string()) {
        const std::string& string_value = value.GetString();
        double double_value = 0;
        if (base::StringToDouble(string_value, &double_value))
          return base::Value(double_value);
      }
      return value;
    }

    // String can't be converted from other types.
    case base::Value::Type::STRING: {
      return value;
    }

    // Binary is not a valid schema type.
    case base::Value::Type::BINARY: {
      NOTREACHED();
      return base::Value();
    }

    // Complex types have to be deserialized from JSON.
    case base::Value::Type::DICTIONARY:
    case base::Value::Type::LIST: {
      if (value.is_string()) {
        base::Optional<base::Value> decoded_value = base::JSONReader::Read(
            value.GetString(),
            base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
        if (decoded_value.has_value())
          return decoded_value;
      }
      return value;
    }

    // TODO(crbug.com/859477): Remove after root cause is found.
    case base::Value::Type::DEAD: {
      CHECK(false);
      return base::nullopt;
    }
  }

  // TODO(crbug.com/859477): Revert to NOTREACHED() after root cause is found.
  CHECK(false);
  return base::nullopt;
}

void PolicyConverter::SetPolicyValue(const std::string& key,
                                     base::Value value) {
  const Schema schema = policy_schema_->GetKnownProperty(key);
  const PolicyNamespace ns(POLICY_DOMAIN_CHROME, std::string());
  base::Optional<base::Value> converted_value =
      ConvertValueToSchema(std::move(value), schema);
  std::unique_ptr<base::Value> value_ptr =
      converted_value.has_value()
          ? base::Value::ToUniquePtrValue(std::move(converted_value.value()))
          : nullptr;
  policy_bundle_->Get(ns).Set(key, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                              POLICY_SOURCE_PLATFORM, std::move(value_ptr),
                              nullptr);
}

}  // namespace android
}  // namespace policy
