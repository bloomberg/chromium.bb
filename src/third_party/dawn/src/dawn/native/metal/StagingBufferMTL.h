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

#ifndef SRC_DAWN_NATIVE_METAL_STAGINGBUFFERMTL_H_
#define SRC_DAWN_NATIVE_METAL_STAGINGBUFFERMTL_H_

#include "dawn/native/StagingBuffer.h"

#include "dawn/common/NSRef.h"

#import <Metal/Metal.h>

namespace dawn::native::metal {

class Device;

class StagingBuffer : public StagingBufferBase {
  public:
    StagingBuffer(size_t size, Device* device);
    ~StagingBuffer() override;

    id<MTLBuffer> GetBufferHandle() const;

    MaybeError Initialize() override;

  private:
    Device* mDevice;
    NSPRef<id<MTLBuffer>> mBuffer;
};
}  // namespace dawn::native::metal

#endif  // SRC_DAWN_NATIVE_METAL_STAGINGBUFFERMTL_H_
