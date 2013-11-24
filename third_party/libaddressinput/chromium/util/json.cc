// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/json.h"

#include "base/json/json_reader.h"
#include "base/logging.h"

namespace i18n {
namespace addressinput {

Json::Json() {}
Json::~Json() {}

bool Json::ParseObject(const std::string& json) {
  dict_.reset();

  // |json| is converted to a |c_str()| here because rapidjson and other parts
  // of the standalone library use char* rather than std::string.
  scoped_ptr<base::Value> parsed(base::JSONReader::Read(json.c_str()));
  if (parsed && parsed->IsType(base::Value::TYPE_DICTIONARY))
    dict_.reset(static_cast<base::DictionaryValue*>(parsed.release()));

  return !!dict_;
}

bool Json::HasStringValueForKey(const std::string& key) const {
  base::Value* val = NULL;
  dict_->GetWithoutPathExpansion(key, &val);
  return val && val->IsType(base::Value::TYPE_STRING);
}

std::string Json::GetStringValueForKey(const std::string& key) const {
  std::string result;
  dict_->GetStringWithoutPathExpansion(key, &result);
  return result;
}

}  // namespace addressinput
}  // namespace i18n
