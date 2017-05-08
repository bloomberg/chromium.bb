// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_layout_provider.h"

namespace views {
namespace test {

TestLayoutProvider::TestLayoutProvider() {}
TestLayoutProvider::~TestLayoutProvider() {}

void TestLayoutProvider::SetDistanceMetric(int metric, int value) {
  distance_metrics_[metric] = value;
}
void TestLayoutProvider::SetSnappedDialogWidth(int width) {
  snapped_dialog_width_ = width;
}

int TestLayoutProvider::GetDistanceMetric(int metric) const {
  if (distance_metrics_.count(metric))
    return distance_metrics_.find(metric)->second;
  return LayoutProvider::GetDistanceMetric(metric);
}

int TestLayoutProvider::GetSnappedDialogWidth(int min_width) const {
  return snapped_dialog_width_ ? snapped_dialog_width_ : min_width;
}

}  // namespace test
}  // namespace views
