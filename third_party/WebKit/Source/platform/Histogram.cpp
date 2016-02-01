// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/Histogram.h"

#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"

namespace blink {

CustomCountHistogram::CustomCountHistogram(const char* name, int32_t min, int32_t max, int32_t bucketCount)
{
    m_histogram = base::Histogram::FactoryGet(name, min, max, bucketCount, base::HistogramBase::kUmaTargetedHistogramFlag);
}

CustomCountHistogram::CustomCountHistogram(base::HistogramBase* histogram)
    : m_histogram(histogram)
{
}

void CustomCountHistogram::count(int32_t sample)
{
    m_histogram->Add(sample);
}

EnumerationHistogram::EnumerationHistogram(const char* name, int32_t boundaryValue)
    : CustomCountHistogram(base::LinearHistogram::FactoryGet(name, 1, boundaryValue, boundaryValue + 1, base::HistogramBase::kUmaTargetedHistogramFlag))
{
}

SparseHistogram::SparseHistogram(const char* name)
{
    m_histogram = base::SparseHistogram::FactoryGet(name, base::HistogramBase::kUmaTargetedHistogramFlag);
}

void SparseHistogram::sample(int32_t sample)
{
    m_histogram->Add(sample);
}

} // namespace blink
