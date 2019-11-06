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

#ifndef DAWNNATIVE_COMPUTEPASSENCODER_H_
#define DAWNNATIVE_COMPUTEPASSENCODER_H_

#include "dawn_native/Error.h"
#include "dawn_native/ProgrammablePassEncoder.h"

namespace dawn_native {

    // This is called ComputePassEncoderBase to match the code generator expectations. Note that it
    // is a pure frontend type to record in its parent CommandEncoder and never has a backend
    // implementation.
    // TODO(cwallez@chromium.org): Remove that generator limitation and rename to ComputePassEncoder
    class ComputePassEncoderBase : public ProgrammablePassEncoder {
      public:
        ComputePassEncoderBase(DeviceBase* device,
                               CommandEncoderBase* commandEncoder,
                               EncodingContext* encodingContext);

        static ComputePassEncoderBase* MakeError(DeviceBase* device,
                                                 CommandEncoderBase* commandEncoder,
                                                 EncodingContext* encodingContext);

        void EndPass();

        void Dispatch(uint32_t x, uint32_t y, uint32_t z);
        void DispatchIndirect(BufferBase* indirectBuffer, uint64_t indirectOffset);
        void SetPipeline(ComputePipelineBase* pipeline);

      protected:
        ComputePassEncoderBase(DeviceBase* device,
                               CommandEncoderBase* commandEncoder,
                               EncodingContext* encodingContext,
                               ErrorTag errorTag);

      private:
        // For render and compute passes, the encoding context is borrowed from the command encoder.
        // Keep a reference to the encoder to make sure the context isn't freed.
        Ref<CommandEncoderBase> mCommandEncoder;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_COMPUTEPASSENCODER_H_
