// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

bool CurveFromString(const std::string& data, BrightnessCurve* const curve) {
  DCHECK(curve);
  curve->clear();
  if (data.empty())
    return true;

  base::StringPairs key_value_pairs;
  if (!base::SplitStringIntoKeyValuePairs(data, ',', '\n', &key_value_pairs)) {
    LOG(ERROR) << "Ill-formatted curve";
    return false;
  }

  for (base::StringPairs::iterator it = key_value_pairs.begin();
       it != key_value_pairs.end(); ++it) {
    double lux;
    if (!base::StringToDouble(it->first, &lux)) {
      LOG(ERROR) << "Ill-formatted lux";
      curve->clear();
      return false;
    }

    double brightness;
    if (!base::StringToDouble(it->second, &brightness)) {
      LOG(ERROR) << "Ill-formatted brightness";
      curve->clear();
      return false;
    }

    curve->push_back(std::make_pair(lux, brightness));
  }
  return true;
}

std::string CurveToString(const BrightnessCurve& curve) {
  if (curve.empty())
    return "";

  std::vector<std::string> rows;
  for (const auto& kv : curve) {
    rows.push_back(base::JoinString(
        {base::NumberToString(kv.first), base::NumberToString(kv.second)},
        ","));
  }

  return base::JoinString(rows, "\n");
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
