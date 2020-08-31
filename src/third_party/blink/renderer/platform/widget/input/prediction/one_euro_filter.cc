// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/prediction/one_euro_filter.h"

#include "third_party/blink/public/common/features.h"

namespace blink {

const double OneEuroFilter::kDefaultFrequency;
const double OneEuroFilter::kDefaultMincutoff;
const double OneEuroFilter::kDefaultBeta;
const double OneEuroFilter::kDefaultDcutoff;

const char OneEuroFilter::kParamBeta[];
const char OneEuroFilter::kParamMincutoff[];

OneEuroFilter::OneEuroFilter(double mincutoff, double beta) {
  x_filter_ = std::make_unique<one_euro_filter::OneEuroFilter>(
      kDefaultFrequency, mincutoff, beta, kDefaultDcutoff);
  y_filter_ = std::make_unique<one_euro_filter::OneEuroFilter>(
      kDefaultFrequency, mincutoff, beta, kDefaultDcutoff);
}

OneEuroFilter::~OneEuroFilter() {}

bool OneEuroFilter::Filter(const base::TimeTicks& timestamp,
                           gfx::PointF* position) const {
  if (!position)
    return false;
  one_euro_filter::TimeStamp ts = (timestamp - base::TimeTicks()).InSecondsF();
  position->set_x(x_filter_->Filter(position->x(), ts));
  position->set_y(y_filter_->Filter(position->y(), ts));
  return true;
}

const char* OneEuroFilter::GetName() const {
  return blink::features::kFilterNameOneEuro;
}

InputFilter* OneEuroFilter::Clone() {
  OneEuroFilter* new_filter = new OneEuroFilter();
  new_filter->x_filter_.reset(x_filter_->Clone());
  new_filter->y_filter_.reset(y_filter_->Clone());
  return new_filter;
}

void OneEuroFilter::Reset() {
  x_filter_->Reset();
  y_filter_->Reset();
}

}  // namespace blink
