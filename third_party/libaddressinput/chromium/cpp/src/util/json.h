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

#ifndef I18N_ADDRESSINPUT_UTIL_JSON_H_
#define I18N_ADDRESSINPUT_UTIL_JSON_H_

#include <libaddressinput/util/scoped_ptr.h>

#include <string>

namespace i18n {
namespace addressinput {

// Parses a JSON dictionary of strings. Sample usage:
//    scoped_ptr<Json> json(Json::Build());
//    std::string value;
//    if (json->ParseObject("{'key1':'value1', 'key2':'value2'}") &&
//        json->GetStringValueForKey("key1", &value)) {
//      Process(value);
//    }
class Json {
 public:
  virtual ~Json();

  // Returns a new instance of |Json| object.
  static scoped_ptr<Json> Build();

  // Parses the |json| string and returns true if |json| is valid and it is an
  // object.
  virtual bool ParseObject(const std::string& json) = 0;

  // Sets |value| to the string for |key| if it exists and has a string value.
  // Returns false if the key doesn't exist or doesn't correspond to a string.
  // The JSON object must be parsed successfully in ParseObject() before
  // invoking this method.
  virtual bool GetStringValueForKey(const std::string& key,
                                    std::string* value) const = 0;

  // Sets |value| to the dictionary for |key| if it exists and has a dictionary
  // value. Returns false if the key doesn't exist or doesn't correspond to a
  // dictionary. The JSON object must be parsed successfully in ParseObject()
  // before invoking this method. |value| is only guaranteed to be valid as long
  // as |this| is valid.
  virtual bool GetJsonValueForKey(const std::string& key,
                                  scoped_ptr<Json>* value) const = 0;

 protected:
  Json();
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_UTIL_JSON_H_
