// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_METRICS_UTIL_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_METRICS_UTIL_H_

#include "ash/public/cpp/desk_template.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash {

// Histogram names for desk templates.
constexpr char kLoadTemplateGridHistogramName[] =
    "Ash.DeskTemplate.LoadTemplateGrid";
constexpr char kDeleteTemplateHistogramName[] =
    "Ash.DeskTemplate.DeleteTemplate";
constexpr char kNewTemplateHistogramName[] = "Ash.DeskTemplate.NewTemplate";
constexpr char kLaunchTemplateHistogramName[] =
    "Ash.DeskTemplate.LaunchFromTemplate";
constexpr char kAddOrUpdateTemplateStatusHistogramName[] =
    "Ash.DeskTemplate.AddOrUpdateTemplateStatus";
constexpr char kTemplateWindowCountHistogramName[] =
    "Ash.DeskTemplate.WindowCount";
constexpr char kTemplateTabCountHistogramName[] = "Ash.DeskTemplate.TabCount";
constexpr char kTemplateWindowAndTabCountHistogramName[] =
    "Ash.DeskTemplate.WindowAndTabCount";
constexpr char kUserTemplateCountHistogramName[] =
    "Ash.DeskTemplate.UserTemplateCount";
constexpr char kUnsupportedAppDialogShowHistogramName[] =
    "Ash.DeskTemplate.UnsupportedAppDialogShow";
constexpr char kReplaceTemplateHistogramName[] =
    "Ash.DeskTemplate.ReplaceTemplate";

// Histogram names for Save & Recall.
constexpr char kNewSaveAndRecallHistogramName[] =
    "Ash.DeskTemplate.NewSaveAndRecall";
constexpr char kDeleteSaveAndRecallHistogramName[] =
    "Ash.DeskTemplate.DeleteSaveAndRecall";
constexpr char kLaunchSaveAndRecallHistogramName[] =
    "Ash.DeskTemplate.LaunchSaveAndRecall";
constexpr char kSaveAndRecallWindowCountHistogramName[] =
    "Ash.DeskTemplate.SaveAndRecallWindowCount";
constexpr char kSaveAndRecallTabCountHistogramName[] =
    "Ash.DeskTemplate.SaveAndRecallTabCount";
constexpr char kSaveAndRecallWindowAndTabCountHistogramName[] =
    "Ash.DeskTemplate.SaveAndRecallWindowAndTabCount";
constexpr char kUserSaveAndRecallCountHistogramName[] =
    "Ash.DeskTemplate.UserSaveAndRecallCount";

// Wrappers calls base::uma with correct histogram name.
void RecordLoadSavedDeskLibraryHistogram();
void RecordDeleteSavedDeskHistogram(DeskTemplateType type);
void RecordLaunchSavedDeskHistogram(DeskTemplateType type);
void RecordNewSavedDeskHistogram(DeskTemplateType type);
void RecordAddOrUpdateTemplateStatusHistogram(
    desks_storage::DeskModel::AddOrUpdateEntryStatus status);
void RecordUserSavedDeskCountHistogram(DeskTemplateType type,
                                       size_t entry_count,
                                       size_t max_entry_count);
void RecordWindowAndTabCountHistogram(const DeskTemplate& desk_template);
void RecordUnsupportedAppDialogShowHistogram();
void RecordReplaceTemplateHistogram();

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_METRICS_UTIL_H_
