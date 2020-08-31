// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_policy_helpers.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"

namespace chromeos {
namespace app_time {
namespace policy {

const char kUrlList[] = "url_list";
const char kAppList[] = "app_list";
const char kAppId[] = "app_id";
const char kAppType[] = "app_type";
const char kAppLimitsArray[] = "app_limits";
const char kAppInfoDict[] = "app_info";
const char kRestrictionEnum[] = "restriction";
const char kDailyLimitInt[] = "daily_limit_mins";
const char kLastUpdatedString[] = "last_updated_millis";
const char kResetAtDict[] = "reset_at";
const char kHourInt[] = "hour";
const char kMinInt[] = "minute";
const char kActivityReportingEnabled[] = "activity_reporting_enabled";

apps::mojom::AppType PolicyStringToAppType(const std::string& app_type) {
  if (app_type == "ARC")
    return apps::mojom::AppType::kArc;
  if (app_type == "BUILT-IN")
    return apps::mojom::AppType::kBuiltIn;
  if (app_type == "CROSTINI")
    return apps::mojom::AppType::kCrostini;
  if (app_type == "EXTENSION")
    return apps::mojom::AppType::kExtension;
  if (app_type == "PLUGIN-VM")
    return apps::mojom::AppType::kPluginVm;
  if (app_type == "WEB")
    return apps::mojom::AppType::kWeb;

  NOTREACHED();
  return apps::mojom::AppType::kUnknown;
}

std::string AppTypeToPolicyString(apps::mojom::AppType app_type) {
  switch (app_type) {
    case apps::mojom::AppType::kArc:
      return "ARC";
    case apps::mojom::AppType::kBuiltIn:
      return "BUILT-IN";
    case apps::mojom::AppType::kCrostini:
      return "CROSTINI";
    case apps::mojom::AppType::kExtension:
      return "EXTENSION";
    case apps::mojom::AppType::kPluginVm:
      return "PLUGIN-VM";
    case apps::mojom::AppType::kWeb:
      return "WEB";
    default:
      NOTREACHED();
      return "";
  }
}

AppRestriction PolicyStringToAppRestriction(const std::string& restriction) {
  if (restriction == "BLOCK")
    return AppRestriction::kBlocked;
  if (restriction == "TIME_LIMIT")
    return AppRestriction::kTimeLimit;

  NOTREACHED();
  return AppRestriction::kUnknown;
}

std::string AppRestrictionToPolicyString(const AppRestriction& restriction) {
  switch (restriction) {
    case AppRestriction::kBlocked:
      return "BLOCK";
    case AppRestriction::kTimeLimit:
      return "TIME_LIMIT";
    default:
      NOTREACHED();
      return "";
  }
}

base::Optional<AppId> AppIdFromDict(const base::Value& dict) {
  if (!dict.is_dict())
    return base::nullopt;

  const std::string* id = dict.FindStringKey(kAppId);
  if (!id || id->empty()) {
    DLOG(ERROR) << "Invalid id.";
    return base::nullopt;
  }

  const std::string* type_string = dict.FindStringKey(kAppType);
  if (!type_string || type_string->empty()) {
    DLOG(ERROR) << "Invalid type.";
    return base::nullopt;
  }

  return AppId(PolicyStringToAppType(*type_string), *id);
}

base::Value AppIdToDict(const AppId& app_id) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(kAppId, base::Value(app_id.app_id()));
  value.SetKey(kAppType, base::Value(AppTypeToPolicyString(app_id.app_type())));

  return value;
}

base::Optional<AppId> AppIdFromAppInfoDict(const base::Value& dict) {
  if (!dict.is_dict())
    return base::nullopt;

  const base::Value* app_info = dict.FindKey(kAppInfoDict);
  if (!app_info || !app_info->is_dict()) {
    DLOG(ERROR) << "Invalid app info dictionary.";
    return base::nullopt;
  }
  return AppIdFromDict(*app_info);
}

base::Optional<AppLimit> AppLimitFromDict(const base::Value& dict) {
  if (!dict.is_dict())
    return base::nullopt;

  const std::string* restriction_string = dict.FindStringKey(kRestrictionEnum);
  if (!restriction_string || restriction_string->empty()) {
    DLOG(ERROR) << "Invalid restriction.";
    return base::nullopt;
  }
  const AppRestriction restriction =
      PolicyStringToAppRestriction(*restriction_string);

  base::Optional<int> daily_limit_mins = dict.FindIntKey(kDailyLimitInt);
  if ((restriction == AppRestriction::kTimeLimit && !daily_limit_mins) ||
      (restriction == AppRestriction::kBlocked && daily_limit_mins)) {
    DLOG(ERROR) << "Invalid restriction.";
    return base::nullopt;
  }

  base::Optional<base::TimeDelta> daily_limit;
  if (daily_limit_mins) {
    daily_limit = base::TimeDelta::FromMinutes(*daily_limit_mins);
    if (daily_limit && (*daily_limit < base::TimeDelta::FromHours(0) ||
                        *daily_limit > base::TimeDelta::FromHours(24))) {
      DLOG(ERROR) << "Invalid daily limit.";
      return base::nullopt;
    }
  }

  const std::string* last_updated_string =
      dict.FindStringKey(kLastUpdatedString);
  int64_t last_updated_millis;
  if (!last_updated_string || last_updated_string->empty() ||
      !base::StringToInt64(*last_updated_string, &last_updated_millis)) {
    DLOG(ERROR) << "Invalid last updated time.";
    return base::nullopt;
  }

  const base::Time last_updated =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(last_updated_millis);

  return AppLimit(restriction, daily_limit, last_updated);
}

base::Value AppLimitToDict(const AppLimit& limit) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(kRestrictionEnum,
               base::Value(AppRestrictionToPolicyString(limit.restriction())));
  if (limit.daily_limit())
    value.SetKey(kDailyLimitInt, base::Value(limit.daily_limit()->InMinutes()));
  const std::string last_updated_string = base::NumberToString(
      (limit.last_updated() - base::Time::UnixEpoch()).InMilliseconds());
  value.SetKey(kLastUpdatedString, base::Value(last_updated_string));

  return value;
}

base::Optional<base::TimeDelta> ResetTimeFromDict(const base::Value& dict) {
  if (!dict.is_dict())
    return base::nullopt;

  const base::Value* reset_dict = dict.FindKey(kResetAtDict);
  if (!reset_dict || !reset_dict->is_dict()) {
    DLOG(ERROR) << "Invalid reset time dictionary.";
    return base::nullopt;
  }

  base::Optional<int> hour = reset_dict->FindIntKey(kHourInt);
  if (!hour) {
    DLOG(ERROR) << "Invalid reset hour.";
    return base::nullopt;
  }

  base::Optional<int> minutes = reset_dict->FindIntKey(kMinInt);
  if (!minutes) {
    DLOG(ERROR) << "Invalid reset minutes.";
    return base::nullopt;
  }

  const int hour_in_mins = base::TimeDelta::FromHours(1).InMinutes();
  return base::TimeDelta::FromMinutes(hour.value() * hour_in_mins +
                                      minutes.value());
}

base::Value ResetTimeToDict(int hour, int minutes) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(kHourInt, base::Value(hour));
  value.SetKey(kMinInt, base::Value(minutes));

  return value;
}

base::Optional<bool> ActivityReportingEnabledFromDict(const base::Value& dict) {
  if (!dict.is_dict())
    return base::nullopt;
  return dict.FindBoolPath(kActivityReportingEnabled);
}

std::map<AppId, AppLimit> AppLimitsFromDict(const base::Value& dict) {
  std::map<AppId, AppLimit> app_limits;

  const base::Value* limits_array = dict.FindListKey(kAppLimitsArray);
  if (!limits_array) {
    DLOG(ERROR) << "Invalid app limits list.";
    return app_limits;
  }

  base::Value::ConstListView list_view = limits_array->GetList();
  for (const base::Value& dict : list_view) {
    if (!dict.is_dict()) {
      DLOG(ERROR) << "Invalid app limits entry. ";
      continue;
    }

    base::Optional<AppId> app_id = AppIdFromAppInfoDict(dict);
    if (!app_id) {
      DLOG(ERROR) << "Invalid app id.";
      continue;
    }

    base::Optional<AppLimit> app_limit = AppLimitFromDict(dict);
    if (!app_limit) {
      DLOG(ERROR) << "Invalid app limit.";
      continue;
    }

    app_limits.emplace(*app_id, *app_limit);
  }

  return app_limits;
}

}  // namespace policy
}  // namespace app_time
}  // namespace chromeos
