// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::ElementsAre;

namespace media_router {

namespace {

const base::Time init_time = base::Time::Now();
const base::Time paint_time = init_time + base::TimeDelta::FromMilliseconds(50);
const base::Time sink_load_time =
    init_time + base::TimeDelta::FromMilliseconds(300);
const base::Time start_casting_time =
    init_time + base::TimeDelta::FromMilliseconds(2000);
const base::Time close_dialog_time =
    init_time + base::TimeDelta::FromMilliseconds(3000);

}  // namespace

class CastDialogMetricsTest : public testing::Test {
 public:
  CastDialogMetricsTest() : metrics_(init_time) {}
  ~CastDialogMetricsTest() override = default;

 protected:
  CastDialogMetrics metrics_;
  base::HistogramTester tester_;
};

TEST_F(CastDialogMetricsTest, OnSinksLoaded) {
  metrics_.OnSinksLoaded(sink_load_time);
  tester_.ExpectUniqueSample(
      MediaRouterMetrics::kHistogramUiDialogLoadedWithData,
      (sink_load_time - init_time).InMilliseconds(), 1);
}

TEST_F(CastDialogMetricsTest, OnPaint) {
  metrics_.OnPaint(paint_time);
  tester_.ExpectUniqueSample(MediaRouterMetrics::kHistogramUiDialogPaint,
                             (paint_time - init_time).InMilliseconds(), 1);
}

TEST_F(CastDialogMetricsTest, OnStartCasting) {
  constexpr int kSinkIndex = 4;
  metrics_.OnSinksLoaded(sink_load_time);
  metrics_.OnStartCasting(start_casting_time, kSinkIndex);
  tester_.ExpectUniqueSample(
      MediaRouterMetrics::kHistogramStartLocalLatency,
      (start_casting_time - sink_load_time).InMilliseconds(), 1);
}

TEST_F(CastDialogMetricsTest, OnCloseDialog) {
  metrics_.OnPaint(paint_time);
  metrics_.OnCloseDialog(close_dialog_time);
  tester_.ExpectUniqueSample(MediaRouterMetrics::kHistogramCloseLatency,
                             (close_dialog_time - paint_time).InMilliseconds(),
                             1);
}

TEST_F(CastDialogMetricsTest, OnRecordSinkCount) {
  constexpr int kSinkCount = 3;
  metrics_.OnRecordSinkCount(kSinkCount);
  tester_.ExpectUniqueSample(MediaRouterMetrics::kHistogramUiDeviceCount,
                             kSinkCount, 1);
}

}  // namespace media_router
