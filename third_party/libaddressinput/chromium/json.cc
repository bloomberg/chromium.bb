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

// A base class for Chrome Json objects. JSON gets parsed into a
// base::DictionaryValue and data is accessed via the Json interface.
class ChromeJson : public Json {
 public:
  virtual bool GetStringValueForKey(const std::string& key, std::string* value)
      const OVERRIDE;
  virtual bool GetJsonValueForKey(const std::string& key,
                                  scoped_ptr<Json>* value) const OVERRIDE;
 protected:
  ChromeJson() {}
  virtual ~ChromeJson() {}

  virtual const base::DictionaryValue* GetDict() const = 0;

  DISALLOW_COPY_AND_ASSIGN(ChromeJson);
};

// A Json object that will parse a string and own the parsed data.
class JsonDataOwner : public ChromeJson {
 public:
  JsonDataOwner() {}
  virtual ~JsonDataOwner() {}

  virtual bool ParseObject(const std::string& json) OVERRIDE {
    dict_.reset();

    // |json| is converted to a |c_str()| here because rapidjson and other parts
    // of the standalone library use char* rather than std::string.
    scoped_ptr<base::Value> parsed(base::JSONReader::Read(json.c_str()));
    if (parsed && parsed->IsType(base::Value::TYPE_DICTIONARY))
      dict_.reset(static_cast<base::DictionaryValue*>(parsed.release()));

    return !!dict_;
  }

 protected:
  virtual const base::DictionaryValue* GetDict() const OVERRIDE {
    return dict_.get();
  }

 private:
  scoped_ptr<base::DictionaryValue> dict_;

  DISALLOW_COPY_AND_ASSIGN(JsonDataOwner);
};

// A Json object which will point to data that's been parsed by a different
// ChromeJson. It does not own its data and is only valid as long as its parent
// ChromeJson is valid.
class JsonDataCopy : public ChromeJson {
 public:
  explicit JsonDataCopy(const base::DictionaryValue* dict) :
      dict_(dict) {}
  virtual ~JsonDataCopy() {}

  virtual bool ParseObject(const std::string& json) OVERRIDE {
    NOTREACHED();
    return false;
  }

 protected:
  virtual const base::DictionaryValue* GetDict() const OVERRIDE {
    return dict_;
  }

 private:
  const base::DictionaryValue* dict_;  // weak reference.

  DISALLOW_COPY_AND_ASSIGN(JsonDataCopy);
};

// ChromeJson ------------------------------------------------------------------

bool ChromeJson::GetStringValueForKey(const std::string& key,
                                      std::string* value) const {
  return GetDict()->GetStringWithoutPathExpansion(key, value);
}

bool ChromeJson::GetJsonValueForKey(const std::string& key,
                                    scoped_ptr<Json>* value) const {
  const base::DictionaryValue* sub_dict = NULL;
  if (!GetDict()->GetDictionaryWithoutPathExpansion(key, &sub_dict) ||
      !sub_dict) {
    return false;
  }

  if (value)
    value->reset(new JsonDataCopy(sub_dict));

  return true;
}

}  // namespace

Json::~Json() {}

// static
scoped_ptr<Json> Json::Build() {
  return scoped_ptr<Json>(new JsonDataOwner);
}

Json::Json() {}

}  // namespace addressinput
}  // namespace i18n
