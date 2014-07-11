// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/src/cpp/src/util/json.h"

#include <map>
#include <utility>

#include "base/basictypes.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"

namespace i18n {
namespace addressinput {

namespace {

// Returns |json| parsed into a JSON dictionary. Sets |parser_error| to true if
// parsing failed.
::scoped_ptr<const base::DictionaryValue> Parse(const std::string& json,
                                                bool* parser_error) {
  DCHECK(parser_error);
  ::scoped_ptr<const base::DictionaryValue> result;

  // |json| is converted to a |c_str()| here because rapidjson and other parts
  // of the standalone library use char* rather than std::string.
  ::scoped_ptr<const base::Value> parsed(base::JSONReader::Read(json.c_str()));
  *parser_error = !parsed || !parsed->IsType(base::Value::TYPE_DICTIONARY);

  if (*parser_error)
    result.reset(new base::DictionaryValue);
  else
    result.reset(static_cast<const base::DictionaryValue*>(parsed.release()));

  return result.Pass();
}

// Returns the list of keys in |dict|.
std::vector<std::string> GetKeysFromDictionary(
    const base::DictionaryValue& dict) {
  std::vector<std::string> keys;
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance())
    keys.push_back(it.key());
  return keys;
}

}  // namespace

// Implementation of JSON parser for libaddressinput using JSON parser in
// Chrome.
class Json::JsonImpl {
 public:
  explicit JsonImpl(const std::string& json)
      : owned_(Parse(json, &parser_error_)),
        dict_(*owned_),
        keys_(GetKeysFromDictionary(dict_)) {}

  ~JsonImpl() { STLDeleteValues(&sub_dicts_); }

  bool parser_error() const { return parser_error_; }

  const std::vector<std::string>& keys() const { return keys_; }

  bool GetStringValueForKey(const std::string& key, std::string* value) const {
    return dict_.GetStringWithoutPathExpansion(key, value);
  }

  bool HasDictionaryValueForKey(const std::string& key) {
    return !!FindDictionary(key);
  }

  const Json& GetDictionaryValueForKey(const std::string& key) {
    const Json* result = FindDictionary(key);
    DCHECK(result);
    return *result;
  }

 private:
  explicit JsonImpl(const base::DictionaryValue& dict)
      : parser_error_(false), dict_(dict), keys_(GetKeysFromDictionary(dict)) {}

  // The caller does not own the returned value, which can be NULL if there's no
  // dictionary for |key|.
  const Json* FindDictionary(const std::string& key) {
    std::map<std::string, Json*>::const_iterator it = sub_dicts_.find(key);
    if (it != sub_dicts_.end())
      return it->second;

    const base::DictionaryValue* sub_dict = NULL;
    if (!dict_.GetDictionaryWithoutPathExpansion(key, &sub_dict) || !sub_dict)
      return NULL;

    std::pair<std::map<std::string, Json*>::iterator, bool> result =
        sub_dicts_.insert(std::make_pair(key, new Json));
    DCHECK(result.second);

    Json* sub_json = result.first->second;
    sub_json->impl_.reset(new JsonImpl(*sub_dict));

    return sub_json;
  }

  const ::scoped_ptr<const base::DictionaryValue> owned_;
  bool parser_error_;
  const base::DictionaryValue& dict_;
  const std::vector<std::string> keys_;
  std::map<std::string, Json*> sub_dicts_;

  DISALLOW_COPY_AND_ASSIGN(JsonImpl);
};

Json::Json() {}

Json::~Json() {}

bool Json::ParseObject(const std::string& json) {
  DCHECK(!impl_);
  impl_.reset(new JsonImpl(json));
  if (impl_->parser_error())
    impl_.reset();
  return !!impl_;
}

const std::vector<std::string>& Json::GetKeys() const {
  return impl_->keys();
}

bool Json::GetStringValueForKey(const std::string& key,
                                std::string* value) const {
  return impl_->GetStringValueForKey(key, value);
}

bool Json::HasDictionaryValueForKey(const std::string& key) const {
  return impl_->HasDictionaryValueForKey(key);
}

const Json& Json::GetDictionaryValueForKey(const std::string& key) const {
  return impl_->GetDictionaryValueForKey(key);
}

}  // namespace addressinput
}  // namespace i18n
