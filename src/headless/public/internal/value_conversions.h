// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_INTERNAL_VALUE_CONVERSIONS_H_
#define HEADLESS_PUBLIC_INTERNAL_VALUE_CONVERSIONS_H_

#include <memory>

#include "headless/lib/browser/protocol/base_string_adapter.h"
#include "headless/public/util/error_reporter.h"

namespace headless {
namespace internal {

// Generic conversion from a type to a base::Value. Implemented below
// (for composite and low level types) and and in types_DOMAIN.cc.
template <typename T>
std::unique_ptr<base::Value> ToValue(const T& value);

// Generic conversion from a base::Value to a type. Note that this generic
// variant is never defined. Instead, we declare a specific template
// specialization for all the used types.
template <typename T>
struct FromValue {
  static std::unique_ptr<T> Parse(const base::Value& value,
                                  ErrorReporter* errors);
};

template <>
inline std::unique_ptr<base::Value> ToValue(const int& value) {
  return std::make_unique<base::Value>(value);
}

template <>
inline std::unique_ptr<base::Value> ToValue(const double& value) {
  return std::make_unique<base::Value>(value);
}

template <>
inline std::unique_ptr<base::Value> ToValue(const bool& value) {
  return std::make_unique<base::Value>(value);
}

template <>
inline std::unique_ptr<base::Value> ToValue(const std::string& value) {
  return std::make_unique<base::Value>(value);
}

template <>
inline std::unique_ptr<base::Value> ToValue(const base::Value& value) {
  return value.CreateDeepCopy();
}

template <>
inline std::unique_ptr<base::Value> ToValue(
    const base::DictionaryValue& value) {
  return ToValue(static_cast<const base::Value&>(value));
}

// Note: Order of the two templates below is important to handle
// vectors of unique_ptr.
template <typename T>
std::unique_ptr<base::Value> ToValue(const std::unique_ptr<T>& value) {
  return ToValue(*value);
}

template <typename T>
std::unique_ptr<base::Value> ToValue(const std::vector<T>& vector_of_values) {
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  for (const T& value : vector_of_values)
    result->Append(ToValue(value));
  return std::move(result);
}

template <>
inline std::unique_ptr<base::Value> ToValue(const protocol::Binary& value) {
  return ToValue(value.toBase64());
}

// FromValue specializations for basic types.
template <>
struct FromValue<bool> {
  static bool Parse(const base::Value& value, ErrorReporter* errors) {
    if (!value.is_bool()) {
      errors->AddError("boolean value expected");
      return false;
    }
    return value.GetBool();
  }
};

template <>
struct FromValue<int> {
  static int Parse(const base::Value& value, ErrorReporter* errors) {
    if (!value.is_int()) {
      errors->AddError("integer value expected");
      return 0;
    }
    return value.GetInt();
  }
};

template <>
struct FromValue<double> {
  static double Parse(const base::Value& value, ErrorReporter* errors) {
    if (!value.is_double() && !value.is_int()) {
      errors->AddError("double value expected");
      return 0;
    }
    return value.GetDouble();
  }
};

template <>
struct FromValue<std::string> {
  static std::string Parse(const base::Value& value, ErrorReporter* errors) {
    if (!value.is_string()) {
      errors->AddError("string value expected");
      return "";
    }
    return value.GetString();
  }
};

template <>
struct FromValue<base::DictionaryValue> {
  static std::unique_ptr<base::DictionaryValue> Parse(const base::Value& value,
                                                      ErrorReporter* errors) {
    const base::DictionaryValue* result;
    if (!value.GetAsDictionary(&result)) {
      errors->AddError("dictionary value expected");
      return nullptr;
    }
    return result->CreateDeepCopy();
  }
};

template <>
struct FromValue<base::Value> {
  static std::unique_ptr<base::Value> Parse(const base::Value& value,
                                            ErrorReporter* errors) {
    return value.CreateDeepCopy();
  }
};

template <typename T>
struct FromValue<std::unique_ptr<T>> {
  static std::unique_ptr<T> Parse(const base::Value& value,
                                  ErrorReporter* errors) {
    return FromValue<T>::Parse(value, errors);
  }
};

template <>
struct FromValue<protocol::Binary> {
  static protocol::Binary Parse(const base::Value& value,
                                ErrorReporter* errors) {
    if (!value.is_string()) {
      errors->AddError("string value expected");
      return protocol::Binary();
    }
    bool success = false;
    protocol::Binary out =
        protocol::Binary::fromBase64(value.GetString(), &success);
    if (!success)
      errors->AddError("base64 decoding error");
    return out;
  }
};

template <typename T>
struct FromValue<std::vector<T>> {
  static std::vector<T> Parse(const base::Value& value, ErrorReporter* errors) {
    std::vector<T> result;
    if (!value.is_list()) {
      errors->AddError("list value expected");
      return result;
    }
    errors->Push();
    for (const auto& item : value.GetList())
      result.push_back(FromValue<T>::Parse(item, errors));
    errors->Pop();
    return result;
  }
};

}  // namespace internal
}  // namespace headless

#endif  // HEADLESS_PUBLIC_INTERNAL_VALUE_CONVERSIONS_H_
