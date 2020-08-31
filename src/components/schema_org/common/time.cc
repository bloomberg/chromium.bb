// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/schema_org/common/time.h"

#include <sstream>

namespace schema_org {

base::Optional<base::TimeDelta> ParseISO8601Duration(const std::string& str) {
  if (str.empty() || str[0] != 'P')
    return base::nullopt;

  base::TimeDelta duration;

  std::string time = "";
  int time_index = str.find("T");
  if (time_index == -1)
    return base::nullopt;

  time = str.substr(time_index + 1);
  std::stringstream t(time);
  char unit;
  int amount;

  while (t >> amount) {
    t >> unit;
    switch (unit) {
      case 'H':
        duration = duration + base::TimeDelta::FromHours(amount);
        break;
      case 'M':
        duration = duration + base::TimeDelta::FromMinutes(amount);
        break;
      case 'S':
        duration = duration + base::TimeDelta::FromSeconds(amount);
        break;
      default:
        return base::nullopt;
    }
  }

  return duration;
}

}  // namespace schema_org
