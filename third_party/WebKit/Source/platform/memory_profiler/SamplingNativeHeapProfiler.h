// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SamplingNativeHeapProfiler_h
#define SamplingNativeHeapProfiler_h

#include <unordered_map>
#include <vector>

#include "base/allocator/allocator_shim.h"
#include "base/atomicops.h"
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

    Sample(size_t, size_t count, unsigned ordinal, unsigned offset);

    uint32_t ordinal;
    uint32_t offset;
  };

  uint32_t Start() override;
  void Stop() override;
  void SetSamplingInterval(size_t sampling_interval) override;
  void SuppressRandomnessForTest();

  std::vector<Sample> GetSamples(uint32_t profile_id);

  static SamplingNativeHeapProfiler* GetInstance();

 private:
  SamplingNativeHeapProfiler();

  static void InstallAllocatorHooksOnce();
  static bool InstallAllocatorHooks();
  static size_t GetNextSampleInterval(size_t base_interval);

  static inline bool ShouldRecordSample(size_t, size_t* accumulated);
  void* RecordAlloc(size_t total_allocated,
                    size_t allocation_size,
                    void* address,
                    uint32_t offset,
                    unsigned skip_frames,
                    bool preserve_data = false);
  void* RecordFree(void* address);
  void RecordStackTrace(Sample*, unsigned skip_frames);

  static void* AllocFn(const base::allocator::AllocatorDispatch* self,
                       size_t,
                       void* context);
  static void* AllocZeroInitializedFn(
      const base::allocator::AllocatorDispatch* self,
      size_t n,
      size_t,
      void* context);
  static void* AllocAlignedFn(const base::allocator::AllocatorDispatch* self,
                              size_t alignment,
                              size_t,
                              void* context);
  static void* ReallocFn(const base::allocator::AllocatorDispatch* self,
                         void* address,
                         size_t,
                         void* context);
  static void FreeFn(const base::allocator::AllocatorDispatch* self,
                     void* address,
                     void* context);
  static size_t GetSizeEstimateFn(
      const base::allocator::AllocatorDispatch* self,
      void* address,
      void* context);
  static unsigned BatchMallocFn(const base::allocator::AllocatorDispatch* self,
                                size_t,
                                void** results,
                                unsigned num_requested,
                                void* context);
  static void BatchFreeFn(const base::allocator::AllocatorDispatch* self,
                          void** to_be_freed,
                          unsigned num_to_be_freed,
                          void* context);
  static void FreeDefiniteSizeFn(const base::allocator::AllocatorDispatch* self,
                                 void* ptr,
                                 size_t,
                                 void* context);

  base::ThreadLocalBoolean entered_;
  base::Lock mutex_;
  std::unordered_map<void*, Sample> samples_;

  static base::allocator::AllocatorDispatch allocator_dispatch_;

  friend struct base::DefaultSingletonTraits<SamplingNativeHeapProfiler>;

  DISALLOW_COPY_AND_ASSIGN(SamplingNativeHeapProfiler);
};

}  // namespace blink

#endif  // SamplingNativeHeapProfiler_h
