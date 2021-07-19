// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"

#include "base/util/values/values_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace segmentation_platform {

SelectedSegment::SelectedSegment(OptimizationTarget segment_id)
    : segment_id(segment_id), in_use(false) {}

SelectedSegment::~SelectedSegment() = default;

SegmentationResultPrefs::SegmentationResultPrefs(PrefService* pref_service)
    : prefs_(pref_service) {}

void SegmentationResultPrefs::SaveSegmentationResultToPref(
    const std::string& result_key,
    const absl::optional<SelectedSegment>& selected_segment) {
  DictionaryPrefUpdate update(prefs_, kSegmentationResultPref);
  base::DictionaryValue* dictionary = update.Get();
  if (!selected_segment.has_value()) {
    dictionary->RemoveKey(result_key);
    return;
  }

  base::Value segmentation_result(base::Value::Type::DICTIONARY);
  segmentation_result.SetIntKey("segment_id", selected_segment->segment_id);
  segmentation_result.SetBoolKey("in_use", selected_segment->in_use);
  segmentation_result.SetKey(
      "selection_time", util::TimeToValue(selected_segment->selection_time));
  dictionary->SetKey(result_key, std::move(segmentation_result));
}

absl::optional<SelectedSegment>
SegmentationResultPrefs::ReadSegmentationResultFromPref(
    const std::string& result_key) {
  const base::DictionaryValue* dictionary =
      prefs_->GetDictionary(kSegmentationResultPref);
  DCHECK(dictionary);

  const base::Value* value = dictionary->FindKey(result_key);
  if (!value)
    return absl::nullopt;

  const base::DictionaryValue& segmentation_result =
      base::Value::AsDictionaryValue(*value);

  absl::optional<int> segment_id = segmentation_result.FindIntKey("segment_id");
  absl::optional<bool> in_use = segmentation_result.FindBoolKey("in_use");
  absl::optional<base::Time> selection_time =
      util::ValueToTime(segmentation_result.FindPath("selection_time"));

  SelectedSegment selected_segment(
      static_cast<OptimizationTarget>(segment_id.value()));
  if (in_use.has_value())
    selected_segment.in_use = in_use.value();
  if (selection_time.has_value())
    selected_segment.selection_time = selection_time.value();

  return selected_segment;
}

}  // namespace segmentation_platform
