// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_result.h"

#include <stdint.h>

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/jni_headers/AutocompleteResult_jni.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/query_tiles/android/tile_conversion_bridge.h"
#include "url/android/gurl_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaBooleanArray;
using base::android::ToJavaIntArray;

namespace {
// Special value passed to VerifyCoherency() suggesting that the action
// requesting verification has no specific index associated with it.
constexpr const int kNoMatchIndex = -1;

// Used for histograms, append only.
enum class MatchVerificationResult {
  VALID_MATCH = 0,
  WRONG_MATCH = 1,
  BAD_RESULT_SIZE = 2,
  OBSOLETE_NATIVE_MATCH_DEAD = 3,
  INVALID_MATCH_POSITION = 4,
  // Keep as the last entry:
  COUNT
};

enum class MatchVerificationPoint {
  INVALID = 0,
  SELECT_MATCH = 1,
  UPDATE_MATCH = 2,
  DELETE_MATCH = 3,
  GROUP_BY_SEARCH_VS_URL_BEFORE = 4,
  GROUP_BY_SEARCH_VS_URL_AFTER = 5,
};

const char* MatchVerificationPointToString(int verification_point) {
  switch (static_cast<MatchVerificationPoint>(verification_point)) {
    case MatchVerificationPoint::SELECT_MATCH:
      return "Select";
    case MatchVerificationPoint::UPDATE_MATCH:
      return "Update";
    case MatchVerificationPoint::DELETE_MATCH:
      return "Delete";
    case MatchVerificationPoint::GROUP_BY_SEARCH_VS_URL_BEFORE:
      return "Group/Before";
    case MatchVerificationPoint::GROUP_BY_SEARCH_VS_URL_AFTER:
      return "Group/After";
    default:
      return "Invalid";
  }
}

bool sInvalidMatchMetricsUploaded = false;

void ReportInvalidMatchData(std::string debug_info, int verification_point) {
  if (sInvalidMatchMetricsUploaded)
    return;

  sInvalidMatchMetricsUploaded = true;

  SCOPED_CRASH_KEY_STRING32("ACMatch", "wrong-match-info", debug_info);
  SCOPED_CRASH_KEY_STRING32("ACMatch", "verification-point",
                            MatchVerificationPointToString(verification_point));
  base::debug::DumpWithoutCrashing();
}
}  // namespace

ScopedJavaLocalRef<jobject> AutocompleteResult::GetOrCreateJavaObject(
    JNIEnv* env) const {
  // Short circuit if we already built the java object.
  if (java_result_)
    return ScopedJavaLocalRef<jobject>(java_result_);

  const size_t groups_count = headers_map_.size();

  std::vector<int> group_ids(groups_count);
  std::vector<std::u16string> group_names(groups_count);
  bool group_collapsed_states[groups_count];

  size_t index = 0;
  for (const auto& group_header : headers_map_) {
    group_ids[index] = group_header.first;
    group_names[index] = group_header.second;
    group_collapsed_states[index] =
        base::Contains(hidden_group_ids_, group_header.first);
    ++index;
  }

  ScopedJavaLocalRef<jintArray> j_group_ids = ToJavaIntArray(env, group_ids);
  ScopedJavaLocalRef<jbooleanArray> j_group_collapsed_states =
      ToJavaBooleanArray(env, group_collapsed_states, groups_count);
  ScopedJavaLocalRef<jobjectArray> j_group_names =
      ToJavaArrayOfStrings(env, group_names);

  java_result_ = Java_AutocompleteResult_fromNative(
      env, reinterpret_cast<intptr_t>(this), BuildJavaMatches(env), j_group_ids,
      j_group_names, j_group_collapsed_states);

  return ScopedJavaLocalRef<jobject>(java_result_);
}

void AutocompleteResult::DestroyJavaObject() const {
  if (!java_result_)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutocompleteResult_notifyNativeDestroyed(env, java_result_);
  java_result_.Reset();
}

ScopedJavaLocalRef<jobjectArray> AutocompleteResult::BuildJavaMatches(
    JNIEnv* env) const {
  jclass clazz = AutocompleteMatch::GetClazz(env);
  ScopedJavaLocalRef<jobjectArray> j_matches(
      env, env->NewObjectArray(matches_.size(), clazz, nullptr));
  base::android::CheckException(env);

  for (size_t index = 0; index < matches_.size(); ++index) {
    env->SetObjectArrayElement(
        j_matches.obj(), index,
        matches_[index].GetOrCreateJavaObject(env).obj());
  }

  return j_matches;
}

void AutocompleteResult::GroupSuggestionsBySearchVsURL(JNIEnv* env,
                                                       int first_index,
                                                       int last_index) {
  if (first_index == last_index)
    return;
  const int num_elements = matches_.size();
  if (first_index < 0 || last_index <= first_index ||
      last_index > num_elements) {
    DCHECK(false) << "Range [" << first_index << "; " << last_index
                  << ") is not valid for grouping; accepted range: [0; "
                  << num_elements << ").";
    return;
  }

  auto range_start = const_cast<ACMatches&>(matches_).begin();
  GroupSuggestionsBySearchVsURL(range_start + first_index,
                                range_start + last_index);
  Java_AutocompleteResult_updateMatches(env, java_result_,
                                        BuildJavaMatches(env));
}

bool AutocompleteResult::VerifyCoherency(
    JNIEnv* env,
    const JavaParamRef<jlongArray>& j_matches_array,
    jint match_index,
    jint verification_point) {
  DCHECK(j_matches_array);

  std::vector<jlong> j_matches;
  base::android::JavaLongArrayToLongVector(env, j_matches_array, &j_matches);

  if (j_matches.size() != size()) {
    UMA_HISTOGRAM_ENUMERATION("Android.Omnibox.InvalidMatch",
                              MatchVerificationResult::BAD_RESULT_SIZE,
                              MatchVerificationResult::COUNT);
    NOTREACHED() << "AutocompletResult objects are of different size: "
                 << j_matches.size() << " (Java) vs " << size() << " (Native)";
    ReportInvalidMatchData(base::NumberToString(j_matches.size()) +
                               "!=" + base::NumberToString(size()),
                           verification_point);
    return false;
  }

  if (match_index != kNoMatchIndex && match_index >= static_cast<int>(size())) {
    UMA_HISTOGRAM_ENUMERATION("Android.Omnibox.InvalidMatch",
                              MatchVerificationResult::INVALID_MATCH_POSITION,
                              MatchVerificationResult::COUNT);
    NOTREACHED() << "Requested action index is not valid: " << match_index
                 << " outside of " << size() << " limit";
    ReportInvalidMatchData(
        base::NumberToString(match_index) + ">=" + base::NumberToString(size()),
        verification_point);
    return false;
  }

  for (auto index = 0u; index < size(); index++) {
    if (reinterpret_cast<intptr_t>(match_at(index)) != j_matches[index]) {
      UMA_HISTOGRAM_ENUMERATION("Android.Omnibox.InvalidMatch",
                                MatchVerificationResult::WRONG_MATCH,
                                MatchVerificationResult::COUNT);
      // Note: the NDEBUG is defined for release / debug-disabled builds.
#ifndef NDEBUG
      // Print the list of matches at every position on each side.
      // Used for debugging purposes.
      for (auto i = 0u; i < size(); i++) {
        auto* this_match = match_at(i);
        auto* other_match = reinterpret_cast<AutocompleteMatch*>(j_matches[i]);
        DLOG(WARNING) << "Suggestion at index " << i << ": "
                      << "(Native): " << this_match->fill_into_edit
                      << "(Java): "
                      << (other_match ? other_match->fill_into_edit
                                      : u"<null>");
      }
#endif
      NOTREACHED()
          << "AutocompleteMatch mismatch with native-sourced suggestions at "
          << index;

      ReportInvalidMatchData(
          base::NumberToString(index) + "/" + base::NumberToString(size()),
          verification_point);
      return false;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Android.Omnibox.InvalidMatch",
                            MatchVerificationResult::VALID_MATCH,
                            MatchVerificationResult::COUNT);
  return true;
}
