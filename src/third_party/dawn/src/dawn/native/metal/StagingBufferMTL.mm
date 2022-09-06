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

#include "dawn/native/metal/StagingBufferMTL.h"
#include "dawn/native/metal/DeviceMTL.h"

namespace dawn::native::metal {

StagingBuffer::StagingBuffer(size_t size, Device* device)
    : StagingBufferBase(size), mDevice(device) {}

StagingBuffer::~StagingBuffer() = default;

MaybeError StagingBuffer::Initialize() {
    const size_t bufferSize = GetSize();
    mBuffer =
        AcquireNSPRef([mDevice->GetMTLDevice() newBufferWithLength:bufferSize
                                                           options:MTLResourceStorageModeShared]);

    if (mBuffer == nullptr) {
        return DAWN_OUT_OF_MEMORY_ERROR("Unable to allocate buffer.");
    }

    mMappedPointer = [*mBuffer contents];
    if (mMappedPointer == nullptr) {
        return DAWN_INTERNAL_ERROR("Unable to map staging buffer.");
    }

    return {};
}

id<MTLBuffer> StagingBuffer::GetBufferHandle() const {
    return mBuffer.Get();
}

}  // namespace dawn::native::metal
