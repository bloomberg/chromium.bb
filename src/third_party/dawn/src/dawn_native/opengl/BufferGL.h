// Copyright 2017 The Dawn Authors
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

#ifndef DAWNNATIVE_OPENGL_BUFFERGL_H_
#define DAWNNATIVE_OPENGL_BUFFERGL_H_

#include "dawn_native/Buffer.h"

#include "glad/glad.h"

namespace dawn_native { namespace opengl {

    class Device;

    class Buffer : public BufferBase {
      public:
        Buffer(Device* device, const BufferDescriptor* descriptor);
        ~Buffer();

        GLuint GetHandle() const;

      private:
        // Dawn API
        MaybeError SetSubDataImpl(uint32_t start, uint32_t count, const uint8_t* data) override;
        void MapReadAsyncImpl(uint32_t serial) override;
        void MapWriteAsyncImpl(uint32_t serial) override;
        void UnmapImpl() override;
        void DestroyImpl() override;

        MaybeError MapAtCreationImpl(uint8_t** mappedPointer) override;

        GLuint mBuffer = 0;
    };

}}  // namespace dawn_native::opengl

#endif  // DAWNNATIVE_OPENGL_BUFFERGL_H_
