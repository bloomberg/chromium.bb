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

  virtual bool ParseObject(const std::string& json) OVERRIDE {
    dict_.reset();

    // |json| is converted to a |c_str()| here because rapidjson and other parts
    // of the standalone library use char* rather than std::string.
    scoped_ptr<base::Value> parsed(base::JSONReader::Read(json.c_str()));
    if (parsed && parsed->IsType(base::Value::TYPE_DICTIONARY))
      dict_.reset(static_cast<base::DictionaryValue*>(parsed.release()));

    return !!dict_;
  }

  virtual bool GetStringValueForKey(const std::string& key, std::string* value)
      const OVERRIDE {
    return dict_->GetStringWithoutPathExpansion(key, value);
  }

 private:
  scoped_ptr<base::DictionaryValue> dict_;

  DISALLOW_COPY_AND_ASSIGN(ChromeJson);
};

}  // namespace

Json::~Json() {}

// static
scoped_ptr<Json> Json::Build() {
  return scoped_ptr<Json>(new ChromeJson);
}

Json::Json() {}

}  // namespace addressinput
}  // namespace i18n
