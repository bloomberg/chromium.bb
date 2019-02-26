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

#include "tests/unittests/validation/ValidationTest.h"

namespace {

class TextureViewValidationTest : public ValidationTest {
};

constexpr uint32_t kWidth = 32u;
constexpr uint32_t kHeight = 32u;
constexpr uint32_t kDefaultMipLevels = 6u;

constexpr dawn::TextureFormat kDefaultTextureFormat = dawn::TextureFormat::R8G8B8A8Unorm;

dawn::Texture Create2DArrayTexture(dawn::Device& device,
                                   uint32_t arrayLayers,
                                   uint32_t width = kWidth,
                                   uint32_t height = kHeight) {
    dawn::TextureDescriptor descriptor;
    descriptor.dimension = dawn::TextureDimension::e2D;
    descriptor.size.width = width;
    descriptor.size.height = height;
    descriptor.size.depth = 1;
    descriptor.arrayLayer = arrayLayers;
    descriptor.format = kDefaultTextureFormat;
    descriptor.levelCount = kDefaultMipLevels;
    descriptor.usage = dawn::TextureUsageBit::Sampled;
    return device.CreateTexture(&descriptor);
}

dawn::TextureViewDescriptor CreateDefaultTextureViewDescriptor(dawn::TextureViewDimension dimension) {
    dawn::TextureViewDescriptor descriptor;
    descriptor.format = kDefaultTextureFormat;
    descriptor.dimension = dimension;
    descriptor.baseMipLevel = 0;
    descriptor.levelCount = kDefaultMipLevels;
    descriptor.baseArrayLayer = 0;
    descriptor.layerCount = 1;
    return descriptor;
}

// Test creating texture view on a 2D non-array texture
TEST_F(TextureViewValidationTest, CreateTextureViewOnTexture2D) {
    dawn::Texture texture = Create2DArrayTexture(device, 1);

    dawn::TextureViewDescriptor base2DTextureViewDescriptor =
        CreateDefaultTextureViewDescriptor(dawn::TextureViewDimension::e2D);

    // It is OK to create a 2D texture view on a 2D texture.
    {
        dawn::TextureViewDescriptor descriptor = base2DTextureViewDescriptor;
        descriptor.layerCount = 1;
        texture.CreateTextureView(&descriptor);
    }

    // It is an error to specify the layer count of the texture view > 1 when texture view dimension
    // is 2D.
    {
        dawn::TextureViewDescriptor descriptor = base2DTextureViewDescriptor;
        descriptor.layerCount = 2;
        ASSERT_DEVICE_ERROR(texture.CreateTextureView(&descriptor));
    }

    // It is OK to create a 1-layer 2D array texture view on a 2D texture.
    {
        dawn::TextureViewDescriptor descriptor = base2DTextureViewDescriptor;
        descriptor.dimension = dawn::TextureViewDimension::e2DArray;
        descriptor.layerCount = 1;
        texture.CreateTextureView(&descriptor);
    }

    // It is an error to specify levelCount == 0.
    {
        dawn::TextureViewDescriptor descriptor = base2DTextureViewDescriptor;
        descriptor.levelCount = 0;
        ASSERT_DEVICE_ERROR(texture.CreateTextureView(&descriptor));
    }

    // It is an error to make the mip level out of range.
    {
        dawn::TextureViewDescriptor descriptor = base2DTextureViewDescriptor;
        descriptor.baseMipLevel = kDefaultMipLevels - 1;
        descriptor.levelCount = 2;
        ASSERT_DEVICE_ERROR(texture.CreateTextureView(&descriptor));
    }
}

// Test creating texture view on a 2D array texture
TEST_F(TextureViewValidationTest, CreateTextureViewOnTexture2DArray) {
    constexpr uint32_t kDefaultArrayLayers = 6;

    dawn::Texture texture = Create2DArrayTexture(device, kDefaultArrayLayers);

    dawn::TextureViewDescriptor base2DArrayTextureViewDescriptor =
        CreateDefaultTextureViewDescriptor(dawn::TextureViewDimension::e2DArray);

    // It is OK to create a 2D texture view on a 2D array texture.
    {
        dawn::TextureViewDescriptor descriptor = base2DArrayTextureViewDescriptor;
        descriptor.dimension = dawn::TextureViewDimension::e2D;
        descriptor.layerCount = 1;
        texture.CreateTextureView(&descriptor);
    }

    // It is OK to create a 2D array texture view on a 2D array texture.
    {
        dawn::TextureViewDescriptor descriptor = base2DArrayTextureViewDescriptor;
        descriptor.layerCount = kDefaultArrayLayers;
        texture.CreateTextureView(&descriptor);
    }

    // It is an error to specify layerCount == 0.
    {
        dawn::TextureViewDescriptor descriptor = base2DArrayTextureViewDescriptor;
        descriptor.layerCount = 0;
        ASSERT_DEVICE_ERROR(texture.CreateTextureView(&descriptor));
    }

    // It is an error to make the array layer out of range.
    {
        dawn::TextureViewDescriptor descriptor = base2DArrayTextureViewDescriptor;
        descriptor.layerCount = kDefaultArrayLayers + 1;
        ASSERT_DEVICE_ERROR(texture.CreateTextureView(&descriptor));
    }
}

// Test creating cube map texture view
TEST_F(TextureViewValidationTest, CreateCubeMapTextureView) {
    constexpr uint32_t kDefaultArrayLayers = 16;

    dawn::Texture texture = Create2DArrayTexture(device, kDefaultArrayLayers);

    dawn::TextureViewDescriptor base2DArrayTextureViewDescriptor =
        CreateDefaultTextureViewDescriptor(dawn::TextureViewDimension::e2DArray);

    // It is OK to create a cube map texture view with layerCount == 6.
    {
        dawn::TextureViewDescriptor descriptor = base2DArrayTextureViewDescriptor;
        descriptor.dimension = dawn::TextureViewDimension::Cube;
        descriptor.layerCount = 6;
        texture.CreateTextureView(&descriptor);
    }

    // It is an error to create a cube map texture view with layerCount != 6.
    {
        dawn::TextureViewDescriptor descriptor = base2DArrayTextureViewDescriptor;
        descriptor.dimension = dawn::TextureViewDimension::Cube;
        descriptor.layerCount = 3;
        ASSERT_DEVICE_ERROR(texture.CreateTextureView(&descriptor));
    }

    // It is OK to create a cube map array texture view with layerCount % 6 == 0.
    {
        dawn::TextureViewDescriptor descriptor = base2DArrayTextureViewDescriptor;
        descriptor.dimension = dawn::TextureViewDimension::CubeArray;
        descriptor.layerCount = 12;
        texture.CreateTextureView(&descriptor);
    }

    // It is an error to create a cube map array texture view with layerCount % 6 != 0.
    {
        dawn::TextureViewDescriptor descriptor = base2DArrayTextureViewDescriptor;
        descriptor.dimension = dawn::TextureViewDimension::CubeArray;
        descriptor.layerCount = 11;
        ASSERT_DEVICE_ERROR(texture.CreateTextureView(&descriptor));
    }

    // It is an error to create a cube map texture view with width != height.
    {
        dawn::Texture nonSquareTexture = Create2DArrayTexture(device, 18, 32, 16);

        dawn::TextureViewDescriptor descriptor = base2DArrayTextureViewDescriptor;
        descriptor.dimension = dawn::TextureViewDimension::Cube;
        descriptor.layerCount = 6;
        ASSERT_DEVICE_ERROR(nonSquareTexture.CreateTextureView(&descriptor));
    }

    // It is an error to create a cube map array texture view with width != height.
    {
        dawn::Texture nonSquareTexture = Create2DArrayTexture(device, 18, 32, 16);

        dawn::TextureViewDescriptor descriptor = base2DArrayTextureViewDescriptor;
        descriptor.dimension = dawn::TextureViewDimension::CubeArray;
        descriptor.layerCount = 12;
        ASSERT_DEVICE_ERROR(nonSquareTexture.CreateTextureView(&descriptor));
    }
}

// Test the format compatibility rules when creating a texture view.
// TODO(jiawei.shao@intel.com): add more tests when the rules are fully implemented.
TEST_F(TextureViewValidationTest, TextureViewFormatCompatibility) {
    dawn::Texture texture = Create2DArrayTexture(device, 1);

    dawn::TextureViewDescriptor base2DTextureViewDescriptor =
        CreateDefaultTextureViewDescriptor(dawn::TextureViewDimension::e2D);

    // It is an error to create a texture view in depth-stencil format on a RGBA texture.
    {
        dawn::TextureViewDescriptor descriptor = base2DTextureViewDescriptor;
        descriptor.format = dawn::TextureFormat::D32FloatS8Uint;
        ASSERT_DEVICE_ERROR(texture.CreateTextureView(&descriptor));
    }
}

}