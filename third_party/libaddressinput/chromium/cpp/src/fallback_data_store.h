// Copyright (C) 2014 Google Inc.
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

#ifndef I18N_ADDRESSINPUT_FALLBACK_DATA_STORE_H_
#define I18N_ADDRESSINPUT_FALLBACK_DATA_STORE_H_

#include <string>

namespace i18n {
namespace addressinput {

class FallbackDataStore {
 public:
  // Gets default data for |key|. Should only be used as a last resort after
  // attempts to check the local cache or the webserver have failed.
  static bool Get(const std::string& key, std::string* data);
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_FALLBACK_DATA_STORE_H_
