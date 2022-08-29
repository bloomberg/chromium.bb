// Copyright 2018 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_VULKAN_QUEUEVK_H_
#define SRC_DAWN_NATIVE_VULKAN_QUEUEVK_H_

#include "dawn/native/Queue.h"

namespace dawn::native::vulkan {

class Device;

class Queue final : public QueueBase {
  public:
    static Ref<Queue> Create(Device* device, const QueueDescriptor* descriptor);

  private:
    Queue(Device* device, const QueueDescriptor* descriptor);
    ~Queue() override;
    using QueueBase::QueueBase;

    void Initialize();

    MaybeError SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) override;

    // Dawn API
    void SetLabelImpl() override;
};

}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_NATIVE_VULKAN_QUEUEVK_H_
