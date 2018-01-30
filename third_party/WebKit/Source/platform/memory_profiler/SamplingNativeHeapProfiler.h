// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SamplingNativeHeapProfiler_h
#define SamplingNativeHeapProfiler_h

#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "platform/PlatformExport.h"
#include "public/platform/SamplingHeapProfiler.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace blink {

class PLATFORM_EXPORT SamplingNativeHeapProfiler : public SamplingHeapProfiler {
 public:
  class Sample {
   public:
    size_t size;
    size_t count;
    std::vector<void*> stack;

   private:
    friend class SamplingNativeHeapProfiler;

    Sample(size_t, size_t count, uint32_t ordinal);

    uint32_t ordinal;
  };

  uint32_t Start() override;
  void Stop() override;
  void SetSamplingInterval(size_t sampling_interval) override;
  void SuppressRandomnessForTest();

  std::vector<Sample> GetSamples(uint32_t profile_id);

  static inline void MaybeRecordAlloc(void* address, size_t);
  static inline void MaybeRecordFree(void* address);

  static SamplingNativeHeapProfiler* GetInstance();

 private:
  SamplingNativeHeapProfiler();

  static void InstallAllocatorHooksOnce();
  static bool InstallAllocatorHooks();
  static size_t GetNextSampleInterval(size_t base_interval);

  void RecordAlloc(size_t total_allocated,
                   size_t allocation_size,
                   void* address,
                   unsigned skip_frames);
  void RecordFree(void* address);
  void RecordStackTrace(Sample*, unsigned skip_frames);

  base::ThreadLocalBoolean entered_;
  base::Lock mutex_;
  std::unordered_map<void*, Sample> samples_;

  friend struct base::DefaultSingletonTraits<SamplingNativeHeapProfiler>;

  DISALLOW_COPY_AND_ASSIGN(SamplingNativeHeapProfiler);
};

}  // namespace blink

#endif  // SamplingNativeHeapProfiler_h
