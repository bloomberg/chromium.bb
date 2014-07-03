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

#include <libaddressinput/address_metadata.h>

#include <libaddressinput/address_field.h>

#include <algorithm>
#include <string>

#include "region_data_constants.h"
#include "rule.h"

namespace i18n {
namespace addressinput {

bool IsFieldRequired(AddressField field, const std::string& region_code) {
  if (field == COUNTRY) {
    return true;
  }

  Rule rule;
  rule.CopyFrom(Rule::GetDefault());
  if (!rule.ParseSerializedRule(
          RegionDataConstants::GetRegionData(region_code))) {
    return false;
  }

  return std::find(rule.GetRequired().begin(),
                   rule.GetRequired().end(),
                   field) != rule.GetRequired().end();
}

bool IsFieldUsed(AddressField field, const std::string& region_code) {
  if (field == COUNTRY) {
    return true;
  }

  Rule rule;
  rule.CopyFrom(Rule::GetDefault());
  if (!rule.ParseSerializedRule(
          RegionDataConstants::GetRegionData(region_code))) {
    return false;
  }

  FormatElement to_find(field);
  for (std::vector<std::vector<FormatElement> >::const_iterator line_it =
           rule.GetFormat().begin();
       line_it != rule.GetFormat().end();
       ++line_it) {
    if (std::find(line_it->begin(), line_it->end(), to_find) != line_it->end())
      return true;
  }

  return false;
}

}  // namespace addressinput
}  // namespace i18n
