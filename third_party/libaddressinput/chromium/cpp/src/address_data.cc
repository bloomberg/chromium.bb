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

#include <libaddressinput/address_data.h>

#include <libaddressinput/address_field.h>

#include <cassert>
#include <string>

namespace i18n {
namespace addressinput {

const std::string& AddressData::GetField(AddressField field) const {
  switch (field) {
    case COUNTRY:
      return country_code;
    case ADMIN_AREA:
      return administrative_area;
    case LOCALITY:
      return locality;
    case DEPENDENT_LOCALITY:
      return dependent_locality;
    case SORTING_CODE:
      return sorting_code;
    case POSTAL_CODE:
      return postal_code;
    case ORGANIZATION:
      return organization;
    case RECIPIENT:
      return recipient;
    default:
      assert(false);
      return recipient;
  }
}

}  // namespace addressinput
}  // namespace i18n
