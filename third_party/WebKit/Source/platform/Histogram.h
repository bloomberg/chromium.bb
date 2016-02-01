// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Histogram_h
#define Histogram_h

#include "platform/PlatformExport.h"
#include <stdint.h>

namespace base {
class HistogramBase;
};

namespace blink {

class PLATFORM_EXPORT CustomCountHistogram {
public:
    CustomCountHistogram(const char* name, int32_t min, int32_t max, int32_t bucketCount);
    void count(int32_t sample);

protected:
    explicit CustomCountHistogram(base::HistogramBase*);

private:
    base::HistogramBase* m_histogram;
};

class PLATFORM_EXPORT EnumerationHistogram : public CustomCountHistogram {
public:
    EnumerationHistogram(const char* name, int32_t boundaryValue);
};

class PLATFORM_EXPORT SparseHistogram {
public:
    explicit SparseHistogram(const char* name);

    void sample(int32_t sample);

private:
    base::HistogramBase* m_histogram;
};

} // namespace blink

#endif // Histogram_h
