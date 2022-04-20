// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "components/services/app_service/public/cpp/macros.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

// The concept of match level is taken from Android. The values are not
// necessary the same.
// See
// https://developer.android.com/reference/android/content/IntentFilter.html#constants_2
// for more details.
enum class IntentFilterMatchLevel {
  kNone = 0,
  kScheme = 1,
  kHost = 2,
  kPattern = 4,
  kMimeType = 8,
};

// The type of a condition in an IntentFilter, which determines what Intent
// field will be matched against.
ENUM(ConditionType,
     // Matches the URL scheme (e.g. https, tel).
     kScheme,
     // Matches the URL host (e.g. www.google.com).
     kHost,
     // Matches the URL path (e.g. /abc/*). Does not include the URL query or
     // hash.
     kPattern,
     // Matches the action type (e.g. view, send).
     kAction,
     // Matches the top-level mime type (e.g. text/plain).
     kMimeType,
     // Matches against files. All files in the Intent must separately match a
     // ConditionValue for this Condition to match. kFile conditions may only
     // use the following PatternMatchTypes: kMimeType, kFileExtension,
     // kIsDirectory, and kGlob.
     kFile)

// Describes what pattern matching rules are applied to a ConditionValue.
enum class PatternMatchType {
  kNone = 0,
  // The ConditionValue is a literal string which must match the value in the
  // Intent exactly.
  kLiteral,
  // The ConditionValue matches if it is a prefix of the value in the Intent.
  // For example, a ConditionValue of "/users/" matches a value of "/users/me".
  kPrefix,
  // The ConditionValue is a simple glob pattern which matches against the value
  // in the Intent. The syntax allows the following special characters:
  //  *  - match 0 or more occurrences of the previous character
  //  .* - match 0 or more occurrences of any character
  //  \  - escape character
  // All wildcard matching is non-greedy. This syntax is the same as Android:
  // https://developer.android.com/reference/android/os/PatternMatcher#PATTERN_SIMPLE_GLOB
  kGlob,
  // The ConditionValue is a mime type with optional wildcards (e.g.
  // "image/png", or "image/*", or "*/*"), which matches against a mime type
  // from the Intent.
  kMimeType,
  // The ConditionValue is a file extension (e.g. "png") or a wildcard ("*")
  // which is matched against file names in the Intent. Common double extension
  // file types are supported: for example, a file named "file.tar.gz" matches
  // both "gz" and "tar.gz" ConditionValues.
  kFileExtension,
  // The ConditionValue matches any files which are directories.
  kIsDirectory,
  // The ConditionValue matches if it is a suffix of the value in the Intent.
  // For example, a ConditionValue of ".google.com" matches a value of
  // "maps.google.com".
  kSuffix
};

// A ConditionValue is a possible value that is accepted by a Condition. The
// ConditionValue matches |value| against a field from the Intent, using the
// matching rules determined by |match_type|.
struct COMPONENT_EXPORT(APP_TYPES) ConditionValue {
  ConditionValue(const std::string& value, PatternMatchType match_type);
  ConditionValue(const ConditionValue&) = delete;
  ConditionValue& operator=(const ConditionValue&) = delete;
  ~ConditionValue();

  bool operator==(const ConditionValue& other) const;
  bool operator!=(const ConditionValue& other) const;

  std::string ToString() const;

  std::string value;
  PatternMatchType match_type;
};

using ConditionValuePtr = std::unique_ptr<ConditionValue>;
using ConditionValues = std::vector<ConditionValuePtr>;

// A single Condition that must match as part of an IntentFilter. An Intent
// matches this Condition if the appropriate field in the Intent (as determined
// by |condition_type|) matches any of the possible values in
// |condition_values|.
struct COMPONENT_EXPORT(APP_TYPES) Condition {
  Condition(ConditionType condition_type, ConditionValues condition_values);
  Condition(const Condition&) = delete;
  Condition& operator=(const Condition&) = delete;
  ~Condition();

  bool operator==(const Condition& other) const;
  bool operator!=(const Condition& other) const;

  std::unique_ptr<Condition> Clone() const;

  std::string ToString() const;

  ConditionType condition_type;
  ConditionValues condition_values;
};

using ConditionPtr = std::unique_ptr<Condition>;
using Conditions = std::vector<ConditionPtr>;

// An IntentFilter is a matcher for a set of possible Intents. Apps have a
// list of IntentFilters to define all Intents that the app can handle. Each
// IntentFilter contains a list of Conditions, an Intent matches the
// IntentFilter if it matches all of the Conditions.
struct COMPONENT_EXPORT(APP_TYPES) IntentFilter {
  IntentFilter();
  IntentFilter(const IntentFilter&) = delete;
  IntentFilter& operator=(const IntentFilter&) = delete;
  ~IntentFilter();

  bool operator==(const IntentFilter& other) const;
  bool operator!=(const IntentFilter& other) const;

  std::unique_ptr<IntentFilter> Clone() const;

  // Creates condition that only contain one value and adds the condition to
  // the intent filter.
  void AddSingleValueCondition(apps::ConditionType condition_type,
                               const std::string& value,
                               apps::PatternMatchType pattern_match_type);

  // Gets the intent_filter match level. The higher the return value, the better
  // the match is. For example, a filter with scheme, host and path is better
  // match compare with filter with only scheme. Each condition type has a
  // matching level value, and this function will return the sum of the matching
  // level values of all existing condition types.
  int GetFilterMatchLevel();

  void GetMimeTypesAndExtensions(std::set<std::string>& mime_types,
                                 std::set<std::string>& file_extensions);

  // Returns true if the filter is a browser filter, i.e. can handle all https
  // or http scheme.
  bool IsBrowserFilter();

  // Returns true if the filter only contains file extension pattern matches.
  bool IsFileExtensionsFilter();

  std::string ToString() const;

  // A list of Conditions which Intents must match.
  Conditions conditions;

  // Publisher-specific identifier for the activity which registered this
  // filter. Used to determine what action to take when Intents are launched
  // through this filter.
  absl::optional<std::string> activity_name;

  // The label shown to the user for this activity.
  absl::optional<std::string> activity_label;
};

using IntentFilterPtr = std::unique_ptr<IntentFilter>;
using IntentFilters = std::vector<IntentFilterPtr>;

// Creates a deep copy of `intent_filters`.
COMPONENT_EXPORT(APP_TYPES)
IntentFilters CloneIntentFilters(const IntentFilters& intent_filters);

COMPONENT_EXPORT(APP_TYPES)
bool IsEqual(const IntentFilters& source, const IntentFilters& target);

// TODO(crbug.com/1253250): Remove these functions after migrating to non-mojo
// AppService.
COMPONENT_EXPORT(APP_TYPES)
ConditionType ConvertMojomConditionTypeToConditionType(
    const apps::mojom::ConditionType& mojom_condition_type);

COMPONENT_EXPORT(APP_TYPES)
apps::mojom::ConditionType ConvertConditionTypeToMojomConditionType(
    const ConditionType& condition_type);

COMPONENT_EXPORT(APP_TYPES)
PatternMatchType ConvertMojomPatternMatchTypeToPatternMatchType(
    const apps::mojom::PatternMatchType& mojom_pattern_match_type);

COMPONENT_EXPORT(APP_TYPES)
apps::mojom::PatternMatchType ConvertPatternMatchTypeToMojomPatternMatchType(
    const PatternMatchType& pattern_match_type);

COMPONENT_EXPORT(APP_TYPES)
ConditionValuePtr ConvertMojomConditionValueToConditionValue(
    const apps::mojom::ConditionValuePtr& mojom_condition_value);

COMPONENT_EXPORT(APP_TYPES)
apps::mojom::ConditionValuePtr ConvertConditionValueToMojomConditionValue(
    const ConditionValuePtr& condition_value);

COMPONENT_EXPORT(APP_TYPES)
ConditionPtr ConvertMojomConditionToCondition(
    const apps::mojom::ConditionPtr& mojom_condition);

COMPONENT_EXPORT(APP_TYPES)
apps::mojom::ConditionPtr ConvertConditionToMojomCondition(
    const ConditionPtr& condition);

COMPONENT_EXPORT(APP_TYPES)
IntentFilterPtr ConvertMojomIntentFilterToIntentFilter(
    const apps::mojom::IntentFilterPtr& mojom_intent_filter);

COMPONENT_EXPORT(APP_TYPES)
apps::mojom::IntentFilterPtr ConvertIntentFilterToMojomIntentFilter(
    const IntentFilterPtr& intent_filter);

COMPONENT_EXPORT(APP_TYPES)
base::flat_map<std::string, std::vector<apps::mojom::IntentFilterPtr>>
ConvertIntentFiltersToMojomIntentFilters(
    const base::flat_map<std::string, apps::IntentFilters>& intent_filter);

COMPONENT_EXPORT(APP_TYPES)
base::flat_map<std::string, apps::IntentFilters>
ConvertMojomIntentFiltersToIntentFilters(
    const base::flat_map<std::string,
                         std::vector<apps::mojom::IntentFilterPtr>>&
        mojom_intent_filter);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_H_
