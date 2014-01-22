// Copyright (C) 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "json.h"

#include <libaddressinput/util/basictypes.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/reader.h>

namespace i18n {
namespace addressinput {

namespace {

class Rapidjson : public Json {
 public:
  Rapidjson() : dict_() {}

  virtual ~Rapidjson() {}

  virtual bool ParseObject(const std::string& json) {
    scoped_ptr<rapidjson::Document> document(new rapidjson::Document);
    document->Parse<rapidjson::kParseValidateEncodingFlag>(json.c_str());
    if (!document->HasParseError() && document->IsObject()) {
      dict_.reset(document.release());
      return true;
    }
    return false;
  }

  virtual bool GetStringValueForKey(const std::string& key,
                                    std::string* value) const {
    assert(dict_ != NULL);

    // Owned by |dict_|.
    const rapidjson::Value::Member* member = dict_->FindMember(key.c_str());
    if (member == NULL || !member->value.IsString()) {
      return false;
    }

    if (value) {
      value->assign(member->value.GetString(), member->value.GetStringLength());
    }

    return true;
  }

  virtual bool GetJsonValueForKey(const std::string& key,
                                  scoped_ptr<Json>* value) const {
    assert(dict_ != NULL);

    // Owned by |dict_|.
    const rapidjson::Value::Member* member = dict_->FindMember(key.c_str());
    if (member == NULL || !member->value.IsObject()) {
      return false;
    }

    if (value) {
      scoped_ptr<rapidjson::Value> copy(new rapidjson::Value);

      // Rapidjson provides only move operations in public API, but implements
      // the move operation with a memcpy and delete:
      //
      // https://code.google.com/p/rapidjson/source/browse/trunk/include/rapidjson/document.h?r=131#173
      //
      // We need a copy of the object, so we use memcpy manually.
      memcpy(copy.get(), &member->value, sizeof(rapidjson::Value));

      value->reset(new Rapidjson(copy.Pass()));
    }

    return true;
  }

 protected:
  explicit Rapidjson(scoped_ptr<rapidjson::Value> dict)
      : dict_(dict.Pass()) {}

  // JSON value.
  scoped_ptr<rapidjson::Value> dict_;

  DISALLOW_COPY_AND_ASSIGN(Rapidjson);
};

}  // namespace

Json::~Json() {}

// static
scoped_ptr<Json> Json::Build() {
  return scoped_ptr<Json>(new Rapidjson);
}

Json::Json() {}

}  // namespace addressinput
}  // namespace i18n
