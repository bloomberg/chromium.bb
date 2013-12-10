// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cpp/src/util/json.h"

#include "base/basictypes.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace i18n {
namespace addressinput {

namespace {

class ChromeJson : public Json {
 public:
  ChromeJson() {}

  virtual ~ChromeJson() {}

  virtual bool ParseObject(const std::string& json) {
    dict_.reset();

    // |json| is converted to a |c_str()| here because rapidjson and other parts
    // of the standalone library use char* rather than std::string.
    scoped_ptr<base::Value> parsed(base::JSONReader::Read(json.c_str()));
    if (parsed && parsed->IsType(base::Value::TYPE_DICTIONARY))
      dict_.reset(static_cast<base::DictionaryValue*>(parsed.release()));

    return !!dict_;
  }

  virtual bool HasStringValueForKey(const std::string& key) const {
    base::Value* val = NULL;
    dict_->GetWithoutPathExpansion(key, &val);
    return val && val->IsType(base::Value::TYPE_STRING);
  }

  virtual std::string GetStringValueForKey(const std::string& key) const {
    std::string result;
    dict_->GetStringWithoutPathExpansion(key, &result);
    return result;
  }

 private:
  scoped_ptr<base::DictionaryValue> dict_;

  DISALLOW_COPY_AND_ASSIGN(ChromeJson);
};

}  // namespace

Json::~Json() {}

// static
Json* Json::Build() {
  return new ChromeJson;
}

Json::Json() {}

}  // namespace addressinput
}  // namespace i18n
