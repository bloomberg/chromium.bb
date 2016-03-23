// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Histogram_h
#define Histogram_h

#include "base/metrics/histogram_base.h"
#include "platform/PlatformExport.h"
#include <stdint.h>

namespace base {
class HistogramBase;
};

namespace blink {

class PLATFORM_EXPORT CustomCountHistogram {
public:
    CustomCountHistogram(const char* name, base::HistogramBase::Sample min, base::HistogramBase::Sample max, int32_t bucketCount);
    void count(base::HistogramBase::Sample);

protected:
    explicit CustomCountHistogram(base::HistogramBase*);

private:
    base::HistogramBase* m_histogram;
};

class PLATFORM_EXPORT EnumerationHistogram : public CustomCountHistogram {
public:
    EnumerationHistogram(const char* name, base::HistogramBase::Sample boundaryValue);
};

class PLATFORM_EXPORT SparseHistogram {
public:
    explicit SparseHistogram(const char* name);

    void sample(base::HistogramBase::Sample);

private:
    base::HistogramBase* m_histogram;
};

} // namespace blink

#endif // Histogram_h
