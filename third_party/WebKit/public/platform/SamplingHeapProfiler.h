// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SamplingHeapProfiler_h
#define SamplingHeapProfiler_h

#include "WebCommon.h"

namespace blink {

class BLINK_PLATFORM_EXPORT SamplingHeapProfiler {
 public:
  static SamplingHeapProfiler* GetInstance();

  virtual uint32_t Start() = 0;
  virtual void Stop() = 0;

  virtual void SetSamplingInterval(size_t sampling_interval) = 0;

 protected:
  SamplingHeapProfiler() = default;
  virtual ~SamplingHeapProfiler() = default;
};

}  // namespace blink

#endif  // SamplingHeapProfiler_h
