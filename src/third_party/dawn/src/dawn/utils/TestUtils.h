// Copyright 2020 The Dawn Authors
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

#ifndef SRC_DAWN_UTILS_TESTUTILS_H_
#define SRC_DAWN_UTILS_TESTUTILS_H_

#include <dawn/webgpu_cpp.h>

namespace utils {

    struct TextureDataCopyLayout {
        uint64_t byteLength;
        uint64_t texelBlockCount;
        uint32_t bytesPerRow;
        uint32_t rowsPerImage;
        uint32_t texelBlocksPerRow;
        uint32_t bytesPerImage;
        uint32_t texelBlocksPerImage;
        wgpu::Extent3D mipSize;
    };

    uint32_t GetMinimumBytesPerRow(wgpu::TextureFormat format, uint32_t width);
    TextureDataCopyLayout GetTextureDataCopyLayoutForTextureAtLevel(
        wgpu::TextureFormat format,
        wgpu::Extent3D textureSizeAtLevel0,
        uint32_t mipmapLevel,
        wgpu::TextureDimension dimension = wgpu::TextureDimension::e2D,
        uint32_t rowsPerImage = wgpu::kCopyStrideUndefined);

    uint64_t RequiredBytesInCopy(uint64_t bytesPerRow,
                                 uint64_t rowsPerImage,
                                 wgpu::Extent3D copyExtent,
                                 wgpu::TextureFormat textureFormat);
    uint64_t RequiredBytesInCopy(uint64_t bytesPerRow,
                                 uint64_t rowsPerImage,
                                 uint64_t widthInBlocks,
                                 uint64_t heightInBlocks,
                                 uint64_t depth,
                                 uint64_t bytesPerBlock);

    uint64_t GetTexelCountInCopyRegion(uint64_t bytesPerRow,
                                       uint64_t rowsPerImage,
                                       wgpu::Extent3D copyExtent,
                                       wgpu::TextureFormat textureFormat);

    // A helper function used for testing DynamicUploader offset alignment.
    // A call of this function will do a Queue::WriteTexture with 1 byte of data,
    // so that assuming that WriteTexture uses DynamicUploader, the first RingBuffer
    // in it will contain 1 byte of data.
    void UnalignDynamicUploader(wgpu::Device device);

    uint32_t VertexFormatSize(wgpu::VertexFormat format);

}  // namespace utils

#endif  // SRC_DAWN_UTILS_TESTUTILS_H_
