// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class provides the same interface as ../src/cpp/src/util/json.h but with
// a Chromium-specific JSON parser. This is because it didn't make much sense to
// have 2 JSON parsers in Chrome's code base, and the standalone library opted
// to use rapidjson instead of jsoncpp. See the other file for an explanation of
// what this class does.

#ifndef THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_UTIL_JSON_H_
#define THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_UTIL_JSON_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace i18n {
namespace addressinput {

class Json {
 public:
  Json();
  ~Json();

  bool ParseObject(const std::string& json);
  bool HasStringValueForKey(const std::string& key) const;
  std::string GetStringValueForKey(const std::string& key) const;

 private:
  scoped_ptr<base::DictionaryValue> dict_;

  DISALLOW_COPY_AND_ASSIGN(Json);
};

}  // namespace addressinput
}  // namespace i18n

#endif  // THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_UTIL_JSON_H_
