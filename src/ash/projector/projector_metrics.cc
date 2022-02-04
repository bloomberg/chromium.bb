// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_metrics.h"

#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

namespace {

constexpr char kProjectorToolbarHistogramName[] = "Ash.Projector.Toolbar";

constexpr char kProjectorMarkerColorHistogramName[] =
    "Ash.Projector.MarkerColor";

constexpr char kProjectorCreationFlowHistogramName[] =
    "Ash.Projector.CreationFlow";

constexpr char kProjectorTranscriptsCountHistogramName[] =
    "Ash.Projector.TranscriptsCount";

// Appends the proper suffix to |prefix| based on whether the user is in tablet
// mode or not.
std::string GetHistogramName(const std::string& prefix) {
  std::string mode =
      Shell::Get()->IsInTabletMode() ? ".TabletMode" : ".ClamshellMode";
  return prefix + mode;
}

}  // namespace

void RecordToolbarMetrics(ProjectorToolbar button) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kProjectorToolbarHistogramName), button);
}

void RecordMarkerColorMetrics(ProjectorMarkerColor color) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kProjectorMarkerColorHistogramName), color);
}

void RecordCreationFlowMetrics(ProjectorCreationFlow step) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kProjectorCreationFlowHistogramName), step);
}

void RecordTranscriptsCount(size_t count) {
  // We don't expect most screencasts to exceed 10,000 transcripts. If this
  // limit is exceeded, then the metric would fall into an overflow bucket.
  base::UmaHistogramCounts10000(
      GetHistogramName(kProjectorTranscriptsCountHistogramName), count);
}

}  // namespace ash
