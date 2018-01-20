// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SamplingNativeHeapProfiler_h
#define SamplingNativeHeapProfiler_h

#include <unordered_map>
#include <vector>

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "platform/PlatformExport.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace blink {

class PLATFORM_EXPORT SamplingNativeHeapProfiler {
 public:
  struct Sample {
    size_t size;
    size_t count;
    unsigned offset;
    std::vector<void*> stack;
  };

  SamplingNativeHeapProfiler() = default;

  void Start();
  void Stop();
  void SetSamplingInterval(unsigned sampling_interval);
  void SuppressRandomnessForTest();

  std::vector<Sample> GetSamples();

  static SamplingNativeHeapProfiler* GetInstance();

  static inline bool CreateAllocSample(size_t, Sample*);
  void* RecordAlloc(Sample&,
                    void* address,
                    unsigned offset,
                    unsigned skip_frames,
                    bool preserve_data = false);
  void* RecordFree(void* address);

 private:
  static void InstallAllocatorHooksOnce();
  static bool InstallAllocatorHooks();
  static intptr_t GetNextSampleInterval(uint64_t base_interval);

  void RecordStackTrace(Sample*, unsigned skip_frames);

  base::ThreadLocalBoolean entered_;
  base::Lock mutex_;
  std::unordered_map<void*, Sample> samples_;

  friend struct base::DefaultSingletonTraits<SamplingNativeHeapProfiler>;

  DISALLOW_COPY_AND_ASSIGN(SamplingNativeHeapProfiler);
};

}  // namespace blink

#endif  // SamplingNativeHeapProfiler_h
