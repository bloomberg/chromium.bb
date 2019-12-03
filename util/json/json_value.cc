// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/json/json_value.h"

namespace openscreen {

absl::optional<int> MaybeGetInt(const Json::Value& message,
                                const char* first,
                                const char* last) {
  const Json::Value* value = message.find(first, last);
  absl::optional<int> result;
  if (value && value->isInt()) {
    result = value->asInt();
  }
  return result;
}

absl::optional<absl::string_view> MaybeGetString(const Json::Value& message,
                                                 const char* first,
                                                 const char* last) {
  const Json::Value* value = message.find(first, last);
  absl::optional<absl::string_view> result;
  if (value && value->isString()) {
    const char* begin;
    const char* end;
    begin = end = nullptr;
    value->getString(&begin, &end);
    if (end >= begin) {
      result = absl::string_view(begin, end - begin);
    }
  }
  return result;
}

}  // namespace openscreen
