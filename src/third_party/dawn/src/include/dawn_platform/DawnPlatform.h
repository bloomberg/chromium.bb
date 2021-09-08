// Copyright 2019 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DAWNPLATFORM_DAWNPLATFORM_H_
#define DAWNPLATFORM_DAWNPLATFORM_H_

#include "dawn_platform/dawn_platform_export.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include <dawn/webgpu.h>

namespace dawn_platform {

    enum class TraceCategory {
        General,     // General trace events
        Validation,  // Dawn validation
        Recording,   // Native command recording
        GPUWork,     // Actual GPU work
    };

    class DAWN_PLATFORM_EXPORT CachingInterface {
      public:
        CachingInterface();
        virtual ~CachingInterface();

        // LoadData has two modes. The first mode is used to get a value which
        // corresponds to the |key|. The |valueOut| is a caller provided buffer
        // allocated to the size |valueSize| which is loaded with data of the
        // size returned. The second mode is used to query for the existence of
        // the |key| where |valueOut| is nullptr and |valueSize| must be 0.
        // The return size is non-zero if the |key| exists.
        virtual size_t LoadData(const WGPUDevice device,
                                const void* key,
                                size_t keySize,
                                void* valueOut,
                                size_t valueSize) = 0;

        // StoreData puts a |value| in the cache which corresponds to the |key|.
        virtual void StoreData(const WGPUDevice device,
                               const void* key,
                               size_t keySize,
                               const void* value,
                               size_t valueSize) = 0;

      private:
        CachingInterface(const CachingInterface&) = delete;
        CachingInterface& operator=(const CachingInterface&) = delete;
    };

    class DAWN_PLATFORM_EXPORT WaitableEvent {
      public:
        WaitableEvent() = default;
        virtual ~WaitableEvent() = default;
        virtual void Wait() = 0;        // Wait for completion
        virtual bool IsComplete() = 0;  // Non-blocking check if the event is complete
    };

    using PostWorkerTaskCallback = void (*)(void* userdata);

    class DAWN_PLATFORM_EXPORT WorkerTaskPool {
      public:
        WorkerTaskPool() = default;
        virtual ~WorkerTaskPool() = default;
        virtual std::unique_ptr<WaitableEvent> PostWorkerTask(PostWorkerTaskCallback,
                                                              void* userdata) = 0;
    };

    class DAWN_PLATFORM_EXPORT Platform {
      public:
        Platform();
        virtual ~Platform();

        virtual const unsigned char* GetTraceCategoryEnabledFlag(TraceCategory category);

        virtual double MonotonicallyIncreasingTime();

        virtual uint64_t AddTraceEvent(char phase,
                                       const unsigned char* categoryGroupEnabled,
                                       const char* name,
                                       uint64_t id,
                                       double timestamp,
                                       int numArgs,
                                       const char** argNames,
                                       const unsigned char* argTypes,
                                       const uint64_t* argValues,
                                       unsigned char flags);

        // The |fingerprint| is provided by Dawn to inform the client to discard the Dawn caches
        // when the fingerprint changes. The returned CachingInterface is expected to outlive the
        // device which uses it to persistently cache objects.
        virtual CachingInterface* GetCachingInterface(const void* fingerprint,
                                                      size_t fingerprintSize);
        virtual std::unique_ptr<WorkerTaskPool> CreateWorkerTaskPool();

      private:
        Platform(const Platform&) = delete;
        Platform& operator=(const Platform&) = delete;
    };

}  // namespace dawn_platform

#endif  // DAWNPLATFORM_DAWNPLATFORM_H_
