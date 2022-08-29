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

#include <string>

#include "dawn/common/Assert.h"
#include "dawn/tests/unittests/validation/ValidationTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/TextureUtils.h"
#include "dawn/utils/WGPUHelpers.h"

class StorageTextureValidationTests : public ValidationTest {
  protected:
    void SetUp() override {
        ValidationTest::SetUp();

        mDefaultVSModule = utils::CreateShaderModule(device, R"(
            @vertex fn main() -> @builtin(position) vec4<f32> {
                return vec4<f32>(0.0, 0.0, 0.0, 1.0);
            })");
        mDefaultFSModule = utils::CreateShaderModule(device, R"(
            @fragment fn main() -> @location(0) vec4<f32> {
                return vec4<f32>(1.0, 0.0, 0.0, 1.0);
            })");
    }

    static const char* GetFloatImageTypeDeclaration(wgpu::TextureViewDimension dimension) {
        // TODO(bclayton): Support and test texture_storage_1d_array
        switch (dimension) {
            case wgpu::TextureViewDimension::e1D:
                return "texture_storage_1d";
            case wgpu::TextureViewDimension::e2D:
                return "texture_storage_2d";
            case wgpu::TextureViewDimension::e2DArray:
                return "texture_storage_2d_array";
            case wgpu::TextureViewDimension::e3D:
                return "texture_storage_3d";
            case wgpu::TextureViewDimension::Cube:
                return "texture_storage_cube";  // Note: Doesn't exist in WGSL (yet)
            case wgpu::TextureViewDimension::CubeArray:
                return "texture_storage_cube_array";  // Note: Doesn't exist in WGSL (yet)
            case wgpu::TextureViewDimension::Undefined:
            default:
                UNREACHABLE();
                return "";
        }
    }

    static std::string CreateComputeShaderWithStorageTexture(
        wgpu::StorageTextureAccess storageTextureBindingType,
        wgpu::TextureFormat textureFormat,
        wgpu::TextureViewDimension textureViewDimension = wgpu::TextureViewDimension::e2D) {
        return CreateComputeShaderWithStorageTexture(
            storageTextureBindingType, utils::GetWGSLImageFormatQualifier(textureFormat),
            GetFloatImageTypeDeclaration(textureViewDimension));
    }

    static std::string CreateComputeShaderWithStorageTexture(
        wgpu::StorageTextureAccess storageTextureBindingType,
        const char* imageFormatQualifier,
        const char* imageTypeDeclaration = "texture_storage_2d") {
        const char* access = "";
        switch (storageTextureBindingType) {
            case wgpu::StorageTextureAccess::WriteOnly:
                access = "write";
                break;
            default:
                UNREACHABLE();
                break;
        }

        std::ostringstream ostream;
        ostream << "@group(0) @binding(0) var image0 : " << imageTypeDeclaration << "<"
                << imageFormatQualifier << ", " << access
                << ">;\n"
                   "@compute @workgroup_size(1) fn main() {\n"
                   "    textureDimensions(image0);\n"
                   "}\n";

        return ostream.str();
    }

    wgpu::Texture CreateTexture(wgpu::TextureUsage usage,
                                wgpu::TextureFormat format,
                                uint32_t sampleCount = 1,
                                uint32_t arrayLayerCount = 1,
                                wgpu::TextureDimension dimension = wgpu::TextureDimension::e2D) {
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = dimension;
        descriptor.size = {16, 16, arrayLayerCount};
        descriptor.sampleCount = sampleCount;
        descriptor.format = format;
        descriptor.mipLevelCount = 1;
        descriptor.usage = usage;
        return device.CreateTexture(&descriptor);
    }

    wgpu::ShaderModule mDefaultVSModule;
    wgpu::ShaderModule mDefaultFSModule;

    const std::array<wgpu::StorageTextureAccess, 1> kSupportedStorageTextureAccess = {
        wgpu::StorageTextureAccess::WriteOnly};
};

// Validate read-only storage textures can be declared in vertex and fragment shaders, while
// writeonly storage textures cannot be used in vertex shaders.
TEST_F(StorageTextureValidationTests, RenderPipeline) {
    // Write-only storage textures cannot be declared in a vertex shader.
    {
        wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, R"(
            @group(0) @binding(0) var image0 : texture_storage_2d<rgba8unorm, write>;
            @vertex
            fn main(@builtin(vertex_index) vertex_index : u32) -> @builtin(position) vec4<f32> {
                textureStore(image0, vec2<i32>(i32(vertex_index), 0), vec4<f32>(1.0, 0.0, 0.0, 1.0));
                return vec4<f32>(0.0);
            })");

        utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.layout = nullptr;
        descriptor.vertex.module = vsModule;
        descriptor.cFragment.module = mDefaultFSModule;
        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }

    // Write-only storage textures can be declared in a fragment shader.
    {
        wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, R"(
            @group(0) @binding(0) var image0 : texture_storage_2d<rgba8unorm, write>;
            @fragment fn main(@builtin(position) position : vec4<f32>) {
                textureStore(image0, vec2<i32>(position.xy), vec4<f32>(1.0, 0.0, 0.0, 1.0));
            })");

        utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.layout = nullptr;
        descriptor.vertex.module = mDefaultVSModule;
        descriptor.cFragment.module = fsModule;
        descriptor.cTargets[0].writeMask = wgpu::ColorWriteMask::None;
        device.CreateRenderPipeline(&descriptor);
    }
}

// Validate both read-only and write-only storage textures can be declared in
// compute shaders.
TEST_F(StorageTextureValidationTests, ComputePipeline) {
    // Write-only storage textures can be declared in a compute shader.
    {
        wgpu::ShaderModule csModule = utils::CreateShaderModule(device, R"(
            @group(0) @binding(0) var image0 : texture_storage_2d<rgba8unorm, write>;

            @compute @workgroup_size(1) fn main(@builtin(local_invocation_id) LocalInvocationID : vec3<u32>) {
                textureStore(image0, vec2<i32>(LocalInvocationID.xy), vec4<f32>(0.0, 0.0, 0.0, 0.0));
            })");

        wgpu::ComputePipelineDescriptor descriptor;
        descriptor.layout = nullptr;
        descriptor.compute.module = csModule;
        descriptor.compute.entryPoint = "main";

        device.CreateComputePipeline(&descriptor);
    }
}

// Validate read-write storage textures are not currently supported.
TEST_F(StorageTextureValidationTests, ReadWriteStorageTexture) {
    // Read-write storage textures cannot be declared in a vertex shader by default.
    {
        ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
            @group(0) @binding(0) var image0 : texture_storage_2d<rgba8unorm, read_write>;
            @vertex fn main() {
                textureDimensions(image0);
            })"));
    }

    // Read-write storage textures cannot be declared in a fragment shader by default.
    {
        ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
            @group(0) @binding(0) var image0 : texture_storage_2d<rgba8unorm, read_write>;
            @fragment fn main() {
                textureDimensions(image0);
            })"));
    }

    // Read-write storage textures cannot be declared in a compute shader by default.
    {
        ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
            @group(0) @binding(0) var image0 : texture_storage_2d<rgba8unorm, read_write>;
            @compute @workgroup_size(1) fn main() {
                textureDimensions(image0);
            })"));
    }
}

// Test that using read-only storage texture and write-only storage texture in
// BindGroupLayout is valid, while using read-write storage texture is not allowed now.
TEST_F(StorageTextureValidationTests, BindGroupLayoutWithStorageTextureBindingType) {
    struct TestSpec {
        wgpu::ShaderStage stage;
        wgpu::StorageTextureAccess type;
        bool valid;
    };
    constexpr std::array<TestSpec, 6> kTestSpecs = {
        {{wgpu::ShaderStage::Vertex, wgpu::StorageTextureAccess::WriteOnly, false},
         {wgpu::ShaderStage::Fragment, wgpu::StorageTextureAccess::WriteOnly, true},
         {wgpu::ShaderStage::Compute, wgpu::StorageTextureAccess::WriteOnly, true}}};

    for (const auto& testSpec : kTestSpecs) {
        wgpu::BindGroupLayoutEntry entry = utils::BindingLayoutEntryInitializationHelper(
            0, testSpec.stage, testSpec.type, wgpu::TextureFormat::R32Uint);

        wgpu::BindGroupLayoutDescriptor descriptor;
        descriptor.entryCount = 1;
        descriptor.entries = &entry;

        if (testSpec.valid) {
            device.CreateBindGroupLayout(&descriptor);
        } else {
            ASSERT_DEVICE_ERROR(device.CreateBindGroupLayout(&descriptor));
        }
    }
}

// Validate it is an error to declare a read-only or write-only storage texture in shaders with any
// format that doesn't support TextureUsage::StorageBinding texture usages.
TEST_F(StorageTextureValidationTests, StorageTextureFormatInShaders) {
    // Not include RGBA8UnormSrgb, BGRA8Unorm, BGRA8UnormSrgb because they are not related to any
    // SPIR-V Image Formats.
    constexpr std::array<wgpu::TextureFormat, 32> kWGPUTextureFormatSupportedAsSPIRVImageFormats = {
        wgpu::TextureFormat::R32Uint,      wgpu::TextureFormat::R32Sint,
        wgpu::TextureFormat::R32Float,     wgpu::TextureFormat::RGBA8Unorm,
        wgpu::TextureFormat::RGBA8Snorm,   wgpu::TextureFormat::RGBA8Uint,
        wgpu::TextureFormat::RGBA8Sint,    wgpu::TextureFormat::RG32Uint,
        wgpu::TextureFormat::RG32Sint,     wgpu::TextureFormat::RG32Float,
        wgpu::TextureFormat::RGBA16Uint,   wgpu::TextureFormat::RGBA16Sint,
        wgpu::TextureFormat::RGBA16Float,  wgpu::TextureFormat::RGBA32Uint,
        wgpu::TextureFormat::RGBA32Sint,   wgpu::TextureFormat::RGBA32Float,
        wgpu::TextureFormat::R8Unorm,      wgpu::TextureFormat::R8Snorm,
        wgpu::TextureFormat::R8Uint,       wgpu::TextureFormat::R8Sint,
        wgpu::TextureFormat::R16Uint,      wgpu::TextureFormat::R16Sint,
        wgpu::TextureFormat::R16Float,     wgpu::TextureFormat::RG8Unorm,
        wgpu::TextureFormat::RG8Snorm,     wgpu::TextureFormat::RG8Uint,
        wgpu::TextureFormat::RG8Sint,      wgpu::TextureFormat::RG16Uint,
        wgpu::TextureFormat::RG16Sint,     wgpu::TextureFormat::RG16Float,
        wgpu::TextureFormat::RGB10A2Unorm, wgpu::TextureFormat::RG11B10Ufloat};

    for (wgpu::StorageTextureAccess storageTextureBindingType : kSupportedStorageTextureAccess) {
        for (wgpu::TextureFormat format : kWGPUTextureFormatSupportedAsSPIRVImageFormats) {
            std::string computeShader =
                CreateComputeShaderWithStorageTexture(storageTextureBindingType, format);
            if (utils::TextureFormatSupportsStorageTexture(format)) {
                utils::CreateShaderModule(device, computeShader.c_str());
            } else {
                ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, computeShader.c_str()));
            }
        }
    }
}

// Verify that declaring a storage texture format that is not supported in WebGPU causes validation
// error.
TEST_F(StorageTextureValidationTests, UnsupportedWGSLStorageTextureFormat) {
    constexpr std::array<wgpu::TextureFormat, 16> kUnsupportedTextureFormats = {
        wgpu::TextureFormat::R8Unorm,      wgpu::TextureFormat::R8Snorm,
        wgpu::TextureFormat::R8Uint,       wgpu::TextureFormat::R8Sint,
        wgpu::TextureFormat::R16Uint,      wgpu::TextureFormat::R16Sint,
        wgpu::TextureFormat::R16Float,     wgpu::TextureFormat::RG8Unorm,
        wgpu::TextureFormat::RG8Snorm,     wgpu::TextureFormat::RG8Uint,
        wgpu::TextureFormat::RG8Sint,      wgpu::TextureFormat::RG16Uint,
        wgpu::TextureFormat::RG16Sint,     wgpu::TextureFormat::RG16Float,
        wgpu::TextureFormat::RGB10A2Unorm, wgpu::TextureFormat::RG11B10Ufloat,
    };

    for (wgpu::StorageTextureAccess bindingType : kSupportedStorageTextureAccess) {
        for (wgpu::TextureFormat format : kUnsupportedTextureFormats) {
            std::string computeShader = CreateComputeShaderWithStorageTexture(bindingType, format);
            ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, computeShader.c_str()));
        }
    }
}

// Verify that declaring a storage texture dimension that isn't supported by
// WebGPU causes a compile failure. WebGPU doesn't support using cube map
// texture views and cube map array texture views as storage textures.
TEST_F(StorageTextureValidationTests, UnsupportedTextureViewDimensionInShader) {
    constexpr std::array<wgpu::TextureViewDimension, 2> kUnsupportedTextureViewDimensions = {
        wgpu::TextureViewDimension::Cube, wgpu::TextureViewDimension::CubeArray};
    constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::R32Float;

    for (wgpu::StorageTextureAccess bindingType : kSupportedStorageTextureAccess) {
        for (wgpu::TextureViewDimension dimension : kUnsupportedTextureViewDimensions) {
            std::string computeShader =
                CreateComputeShaderWithStorageTexture(bindingType, kFormat, dimension);
            ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, computeShader.c_str()));
        }
    }
}

// Verify that declaring a texture view dimension that is not supported to be used as storage
// textures in WebGPU in bind group layout causes validation error. WebGPU doesn't support using
// cube map texture views and cube map array texture views as storage textures.
TEST_F(StorageTextureValidationTests, UnsupportedTextureViewDimensionInBindGroupLayout) {
    constexpr std::array<wgpu::TextureViewDimension, 2> kUnsupportedTextureViewDimensions = {
        wgpu::TextureViewDimension::Cube, wgpu::TextureViewDimension::CubeArray};
    constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::R32Float;

    for (wgpu::StorageTextureAccess bindingType : kSupportedStorageTextureAccess) {
        for (wgpu::TextureViewDimension dimension : kUnsupportedTextureViewDimensions) {
            ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
                device, {{0, wgpu::ShaderStage::Compute, bindingType, kFormat, dimension}}));
        }
    }
}

// Verify when we create and use a bind group layout with storage textures in the creation of
// render and compute pipeline, the binding type in the bind group layout must match the
// declaration in the shader.
TEST_F(StorageTextureValidationTests, BindGroupLayoutEntryTypeMatchesShaderDeclaration) {
    constexpr wgpu::TextureFormat kStorageTextureFormat = wgpu::TextureFormat::R32Float;

    std::initializer_list<utils::BindingLayoutEntryInitializationHelper> kSupportedBindingTypes = {
        {0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},
        {0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},
        {0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::ReadOnlyStorage},
        {0, wgpu::ShaderStage::Compute, wgpu::SamplerBindingType::Filtering},
        {0, wgpu::ShaderStage::Compute, wgpu::TextureSampleType::Float},
        {0, wgpu::ShaderStage::Compute, wgpu::StorageTextureAccess::WriteOnly,
         kStorageTextureFormat}};

    for (wgpu::StorageTextureAccess bindingTypeInShader : kSupportedStorageTextureAccess) {
        // Create the compute shader with the given binding type.
        std::string computeShader =
            CreateComputeShaderWithStorageTexture(bindingTypeInShader, kStorageTextureFormat);
        wgpu::ShaderModule csModule = utils::CreateShaderModule(device, computeShader.c_str());

        // Set common fields of compute pipeline descriptor.
        wgpu::ComputePipelineDescriptor defaultComputePipelineDescriptor;
        defaultComputePipelineDescriptor.compute.module = csModule;
        defaultComputePipelineDescriptor.compute.entryPoint = "main";

        for (utils::BindingLayoutEntryInitializationHelper bindingLayoutEntry :
             kSupportedBindingTypes) {
            wgpu::ComputePipelineDescriptor computePipelineDescriptor =
                defaultComputePipelineDescriptor;

            // Create bind group layout with different binding types.
            wgpu::BindGroupLayout bindGroupLayout =
                utils::MakeBindGroupLayout(device, {bindingLayoutEntry});
            computePipelineDescriptor.layout =
                utils::MakeBasicPipelineLayout(device, &bindGroupLayout);

            // The binding type in the bind group layout must the same as the related image object
            // declared in shader.
            if (bindingLayoutEntry.storageTexture.access == bindingTypeInShader) {
                device.CreateComputePipeline(&computePipelineDescriptor);
            } else {
                ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&computePipelineDescriptor));
            }
        }
    }
}

// Verify it is invalid not to set a valid texture format in a bind group layout when the binding
// type is read-only or write-only storage texture.
TEST_F(StorageTextureValidationTests, UndefinedStorageTextureFormatInBindGroupLayout) {
    wgpu::BindGroupLayoutEntry errorBindGroupLayoutEntry;
    errorBindGroupLayoutEntry.binding = 0;
    errorBindGroupLayoutEntry.visibility = wgpu::ShaderStage::Compute;
    errorBindGroupLayoutEntry.storageTexture.format = wgpu::TextureFormat::Undefined;

    for (wgpu::StorageTextureAccess bindingType : kSupportedStorageTextureAccess) {
        errorBindGroupLayoutEntry.storageTexture.access = bindingType;
        ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(device, {errorBindGroupLayoutEntry}));
    }
}

// Verify it is invalid to create a bind group layout with storage textures and an unsupported
// storage texture format.
TEST_F(StorageTextureValidationTests, StorageTextureFormatInBindGroupLayout) {
    wgpu::BindGroupLayoutEntry defaultBindGroupLayoutEntry;
    defaultBindGroupLayoutEntry.binding = 0;
    defaultBindGroupLayoutEntry.visibility = wgpu::ShaderStage::Compute;

    for (wgpu::StorageTextureAccess bindingType : kSupportedStorageTextureAccess) {
        for (wgpu::TextureFormat textureFormat : utils::kAllTextureFormats) {
            wgpu::BindGroupLayoutEntry bindGroupLayoutBinding = defaultBindGroupLayoutEntry;
            bindGroupLayoutBinding.storageTexture.access = bindingType;
            bindGroupLayoutBinding.storageTexture.format = textureFormat;
            if (utils::TextureFormatSupportsStorageTexture(textureFormat)) {
                utils::MakeBindGroupLayout(device, {bindGroupLayoutBinding});
            } else {
                ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(device, {bindGroupLayoutBinding}));
            }
        }
    }
}

// Verify the storage texture format in the bind group layout must match the declaration in shader.
TEST_F(StorageTextureValidationTests, BindGroupLayoutStorageTextureFormatMatchesShaderDeclaration) {
    for (wgpu::StorageTextureAccess bindingType : kSupportedStorageTextureAccess) {
        for (wgpu::TextureFormat storageTextureFormatInShader : utils::kAllTextureFormats) {
            if (!utils::TextureFormatSupportsStorageTexture(storageTextureFormatInShader)) {
                continue;
            }

            // Create the compute shader module with the given binding type and storage texture
            // format.
            std::string computeShader =
                CreateComputeShaderWithStorageTexture(bindingType, storageTextureFormatInShader);
            wgpu::ShaderModule csModule = utils::CreateShaderModule(device, computeShader.c_str());

            // Set common fields of compute pipeline descriptor.
            wgpu::ComputePipelineDescriptor defaultComputePipelineDescriptor;
            defaultComputePipelineDescriptor.compute.module = csModule;
            defaultComputePipelineDescriptor.compute.entryPoint = "main";

            // Set common fileds of bind group layout binding.
            utils::BindingLayoutEntryInitializationHelper defaultBindGroupLayoutEntry = {
                0, wgpu::ShaderStage::Compute, bindingType, utils::kAllTextureFormats[0]};

            for (wgpu::TextureFormat storageTextureFormatInBindGroupLayout :
                 utils::kAllTextureFormats) {
                if (!utils::TextureFormatSupportsStorageTexture(
                        storageTextureFormatInBindGroupLayout)) {
                    continue;
                }

                // Create the bind group layout with the given storage texture format.
                wgpu::BindGroupLayoutEntry bindGroupLayoutBinding = defaultBindGroupLayoutEntry;
                bindGroupLayoutBinding.storageTexture.format =
                    storageTextureFormatInBindGroupLayout;
                wgpu::BindGroupLayout bindGroupLayout =
                    utils::MakeBindGroupLayout(device, {bindGroupLayoutBinding});

                // Create the compute pipeline with the bind group layout.
                wgpu::ComputePipelineDescriptor computePipelineDescriptor =
                    defaultComputePipelineDescriptor;
                computePipelineDescriptor.layout =
                    utils::MakeBasicPipelineLayout(device, &bindGroupLayout);

                // The storage texture format in the bind group layout must be the same as the one
                // declared in the shader.
                if (storageTextureFormatInShader == storageTextureFormatInBindGroupLayout) {
                    device.CreateComputePipeline(&computePipelineDescriptor);
                } else {
                    ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&computePipelineDescriptor));
                }
            }
        }
    }
}

// Verify the dimension of the bind group layout with storage textures must match the one declared
// in shader.
TEST_F(StorageTextureValidationTests, BindGroupLayoutViewDimensionMatchesShaderDeclaration) {
    constexpr std::array<wgpu::TextureViewDimension, 4> kSupportedDimensions = {
        wgpu::TextureViewDimension::e1D, wgpu::TextureViewDimension::e2D,
        wgpu::TextureViewDimension::e2DArray, wgpu::TextureViewDimension::e3D};
    constexpr wgpu::TextureFormat kStorageTextureFormat = wgpu::TextureFormat::R32Float;

    for (wgpu::StorageTextureAccess bindingType : kSupportedStorageTextureAccess) {
        for (wgpu::TextureViewDimension dimensionInShader : kSupportedDimensions) {
            // Create the compute shader with the given texture view dimension.
            std::string computeShader = CreateComputeShaderWithStorageTexture(
                bindingType, kStorageTextureFormat, dimensionInShader);
            wgpu::ShaderModule csModule = utils::CreateShaderModule(device, computeShader.c_str());

            // Set common fields of compute pipeline descriptor.
            wgpu::ComputePipelineDescriptor defaultComputePipelineDescriptor;
            defaultComputePipelineDescriptor.compute.module = csModule;
            defaultComputePipelineDescriptor.compute.entryPoint = "main";

            // Set common fields of bind group layout binding.
            utils::BindingLayoutEntryInitializationHelper defaultBindGroupLayoutEntry = {
                0, wgpu::ShaderStage::Compute, bindingType, kStorageTextureFormat};

            for (wgpu::TextureViewDimension dimensionInBindGroupLayout : kSupportedDimensions) {
                // Create the bind group layout with the given texture view dimension.
                wgpu::BindGroupLayoutEntry bindGroupLayoutBinding = defaultBindGroupLayoutEntry;
                bindGroupLayoutBinding.storageTexture.viewDimension = dimensionInBindGroupLayout;
                wgpu::BindGroupLayout bindGroupLayout =
                    utils::MakeBindGroupLayout(device, {bindGroupLayoutBinding});

                // Create the compute pipeline with the bind group layout.
                wgpu::ComputePipelineDescriptor computePipelineDescriptor =
                    defaultComputePipelineDescriptor;
                computePipelineDescriptor.layout =
                    utils::MakeBasicPipelineLayout(device, &bindGroupLayout);

                // The texture dimension in the bind group layout must be the same as the one
                // declared in the shader.
                if (dimensionInShader == dimensionInBindGroupLayout) {
                    device.CreateComputePipeline(&computePipelineDescriptor);
                } else {
                    ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&computePipelineDescriptor));
                }
            }
        }
    }
}

// Verify that only a texture view can be used as a read-only or write-only storage texture in a
// bind group.
TEST_F(StorageTextureValidationTests, StorageTextureBindingTypeInBindGroup) {
    constexpr wgpu::TextureFormat kStorageTextureFormat = wgpu::TextureFormat::R32Float;
    for (wgpu::StorageTextureAccess storageBindingType : kSupportedStorageTextureAccess) {
        // Create a bind group layout.
        wgpu::BindGroupLayoutEntry bindGroupLayoutBinding;
        bindGroupLayoutBinding.binding = 0;
        bindGroupLayoutBinding.visibility = wgpu::ShaderStage::Compute;
        bindGroupLayoutBinding.storageTexture.access = storageBindingType;
        bindGroupLayoutBinding.storageTexture.format = kStorageTextureFormat;
        wgpu::BindGroupLayout bindGroupLayout =
            utils::MakeBindGroupLayout(device, {bindGroupLayoutBinding});

        // Buffers are not allowed to be used as storage textures in a bind group.
        {
            wgpu::BufferDescriptor descriptor;
            descriptor.size = 1024;
            descriptor.usage = wgpu::BufferUsage::Uniform;
            wgpu::Buffer buffer = device.CreateBuffer(&descriptor);
            ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, bindGroupLayout, {{0, buffer}}));
        }

        // Samplers are not allowed to be used as storage textures in a bind group.
        {
            wgpu::Sampler sampler = device.CreateSampler();
            ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, bindGroupLayout, {{0, sampler}}));
        }

        // Texture views are allowed to be used as storage textures in a bind group.
        {
            wgpu::TextureView textureView =
                CreateTexture(wgpu::TextureUsage::StorageBinding, kStorageTextureFormat)
                    .CreateView();
            utils::MakeBindGroup(device, bindGroupLayout, {{0, textureView}});
        }
    }
}

// Verify that a texture used as read-only or write-only storage texture in a bind group must be
// created with the texture usage wgpu::TextureUsage::StorageBinding.
TEST_F(StorageTextureValidationTests, StorageTextureUsageInBindGroup) {
    constexpr wgpu::TextureFormat kStorageTextureFormat = wgpu::TextureFormat::R32Float;
    constexpr std::array<wgpu::TextureUsage, 6> kTextureUsages = {
        wgpu::TextureUsage::CopySrc,          wgpu::TextureUsage::CopyDst,
        wgpu::TextureUsage::TextureBinding,   wgpu::TextureUsage::StorageBinding,
        wgpu::TextureUsage::RenderAttachment, wgpu::TextureUsage::Present};

    for (wgpu::StorageTextureAccess storageBindingType : kSupportedStorageTextureAccess) {
        // Create a bind group layout.
        wgpu::BindGroupLayoutEntry bindGroupLayoutBinding;
        bindGroupLayoutBinding.binding = 0;
        bindGroupLayoutBinding.visibility = wgpu::ShaderStage::Compute;
        bindGroupLayoutBinding.storageTexture.access = storageBindingType;
        bindGroupLayoutBinding.storageTexture.format = wgpu::TextureFormat::R32Float;
        wgpu::BindGroupLayout bindGroupLayout =
            utils::MakeBindGroupLayout(device, {bindGroupLayoutBinding});

        for (wgpu::TextureUsage usage : kTextureUsages) {
            // Create texture views with different texture usages
            wgpu::TextureView textureView =
                CreateTexture(usage, kStorageTextureFormat).CreateView();

            // Verify that the texture used as storage texture must be created with the texture
            // usage wgpu::TextureUsage::StorageBinding.
            if (usage & wgpu::TextureUsage::StorageBinding) {
                utils::MakeBindGroup(device, bindGroupLayout, {{0, textureView}});
            } else {
                ASSERT_DEVICE_ERROR(
                    utils::MakeBindGroup(device, bindGroupLayout, {{0, textureView}}));
            }
        }
    }
}

// Verify that the format of a texture used as read-only or write-only storage texture in a bind
// group must match the corresponding bind group binding.
TEST_F(StorageTextureValidationTests, StorageTextureFormatInBindGroup) {
    for (wgpu::StorageTextureAccess storageBindingType : kSupportedStorageTextureAccess) {
        wgpu::BindGroupLayoutEntry defaultBindGroupLayoutEntry;
        defaultBindGroupLayoutEntry.binding = 0;
        defaultBindGroupLayoutEntry.visibility = wgpu::ShaderStage::Compute;
        defaultBindGroupLayoutEntry.storageTexture.access = storageBindingType;

        for (wgpu::TextureFormat formatInBindGroupLayout : utils::kAllTextureFormats) {
            if (!utils::TextureFormatSupportsStorageTexture(formatInBindGroupLayout)) {
                continue;
            }

            // Create a bind group layout with given storage texture format.
            wgpu::BindGroupLayoutEntry bindGroupLayoutBinding = defaultBindGroupLayoutEntry;
            bindGroupLayoutBinding.storageTexture.format = formatInBindGroupLayout;
            wgpu::BindGroupLayout bindGroupLayout =
                utils::MakeBindGroupLayout(device, {bindGroupLayoutBinding});

            for (wgpu::TextureFormat textureViewFormat : utils::kAllTextureFormats) {
                if (!utils::TextureFormatSupportsStorageTexture(textureViewFormat)) {
                    continue;
                }

                // Create texture views with different texture formats.
                wgpu::TextureView storageTextureView =
                    CreateTexture(wgpu::TextureUsage::StorageBinding, textureViewFormat)
                        .CreateView();

                // Verify that the format of the texture view used as storage texture in a bind
                // group must match the storage texture format declaration in the bind group layout.
                if (textureViewFormat == formatInBindGroupLayout) {
                    utils::MakeBindGroup(device, bindGroupLayout, {{0, storageTextureView}});
                } else {
                    ASSERT_DEVICE_ERROR(
                        utils::MakeBindGroup(device, bindGroupLayout, {{0, storageTextureView}}));
                }
            }
        }
    }
}

// Verify that the dimension of a texture view used as read-only or write-only storage texture in a
// bind group must match the corresponding bind group binding.
TEST_F(StorageTextureValidationTests, StorageTextureViewDimensionInBindGroup) {
    constexpr wgpu::TextureFormat kStorageTextureFormat = wgpu::TextureFormat::R32Float;
    constexpr uint32_t kDepthOrArrayLayers = 6u;

    // TODO(crbug.com/dawn/814): test the use of 1D texture view dimensions when they are
    // supported in Dawn.
    constexpr std::array<wgpu::TextureViewDimension, 3> kSupportedDimensions = {
        wgpu::TextureViewDimension::e2D, wgpu::TextureViewDimension::e2DArray,
        wgpu::TextureViewDimension::e3D};

    wgpu::TextureViewDescriptor kDefaultTextureViewDescriptor;
    kDefaultTextureViewDescriptor.format = kStorageTextureFormat;
    kDefaultTextureViewDescriptor.baseMipLevel = 0;
    kDefaultTextureViewDescriptor.mipLevelCount = 1;
    kDefaultTextureViewDescriptor.baseArrayLayer = 0;
    kDefaultTextureViewDescriptor.arrayLayerCount = 1u;

    for (wgpu::StorageTextureAccess storageBindingType : kSupportedStorageTextureAccess) {
        wgpu::BindGroupLayoutEntry defaultBindGroupLayoutEntry;
        defaultBindGroupLayoutEntry.binding = 0;
        defaultBindGroupLayoutEntry.visibility = wgpu::ShaderStage::Compute;
        defaultBindGroupLayoutEntry.storageTexture.access = storageBindingType;
        defaultBindGroupLayoutEntry.storageTexture.format = kStorageTextureFormat;

        for (wgpu::TextureViewDimension dimensionInBindGroupLayout : kSupportedDimensions) {
            // Create a bind group layout with given texture view dimension.
            wgpu::BindGroupLayoutEntry bindGroupLayoutBinding = defaultBindGroupLayoutEntry;
            bindGroupLayoutBinding.storageTexture.viewDimension = dimensionInBindGroupLayout;
            wgpu::BindGroupLayout bindGroupLayout =
                utils::MakeBindGroupLayout(device, {bindGroupLayoutBinding});

            for (wgpu::TextureViewDimension dimensionOfTextureView : kSupportedDimensions) {
                // Create a texture view with given texture view dimension.
                wgpu::Texture texture =
                    CreateTexture(wgpu::TextureUsage::StorageBinding, kStorageTextureFormat, 1,
                                  kDepthOrArrayLayers,
                                  utils::ViewDimensionToTextureDimension(dimensionOfTextureView));

                wgpu::TextureViewDescriptor textureViewDescriptor = kDefaultTextureViewDescriptor;
                textureViewDescriptor.dimension = dimensionOfTextureView;
                wgpu::TextureView storageTextureView = texture.CreateView(&textureViewDescriptor);

                // Verify that the dimension of the texture view used as storage texture in a bind
                // group must match the texture view dimension declaration in the bind group layout.
                if (dimensionInBindGroupLayout == dimensionOfTextureView) {
                    utils::MakeBindGroup(device, bindGroupLayout, {{0, storageTextureView}});
                } else {
                    ASSERT_DEVICE_ERROR(
                        utils::MakeBindGroup(device, bindGroupLayout, {{0, storageTextureView}}));
                }
            }
        }
    }
}

// Verify multisampled storage textures cannot be supported now.
TEST_F(StorageTextureValidationTests, MultisampledStorageTexture) {
    for (wgpu::StorageTextureAccess bindingType : kSupportedStorageTextureAccess) {
        std::string computeShader =
            CreateComputeShaderWithStorageTexture(bindingType, "", "image2DMS");
        ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, computeShader.c_str()));
    }
}

// Verify it is valid to use a texture as either read-only storage texture or write-only storage
// texture in a render pass.
TEST_F(StorageTextureValidationTests, StorageTextureInRenderPass) {
    constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::RGBA8Unorm;
    wgpu::Texture storageTexture = CreateTexture(wgpu::TextureUsage::StorageBinding, kFormat);

    wgpu::Texture outputAttachment = CreateTexture(wgpu::TextureUsage::RenderAttachment, kFormat);
    utils::ComboRenderPassDescriptor renderPassDescriptor({outputAttachment.CreateView()});

    for (wgpu::StorageTextureAccess storageTextureType : kSupportedStorageTextureAccess) {
        // Create a bind group that contains a storage texture.
        wgpu::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, storageTextureType, kFormat}});

        wgpu::BindGroup bindGroupWithStorageTexture =
            utils::MakeBindGroup(device, bindGroupLayout, {{0, storageTexture.CreateView()}});

        // It is valid to use a texture as read-only or write-only storage texture in the render
        // pass.
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder renderPassEncoder = encoder.BeginRenderPass(&renderPassDescriptor);
        renderPassEncoder.SetBindGroup(0, bindGroupWithStorageTexture);
        renderPassEncoder.End();
        encoder.Finish();
    }
}

// Verify it is valid to use a a texture as both read-only storage texture and sampled texture in
// one render pass, while it is invalid to use a texture as both write-only storage texture and
// sampled texture in one render pass.
TEST_F(StorageTextureValidationTests, StorageTextureAndSampledTextureInOneRenderPass) {
    constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::RGBA8Unorm;
    wgpu::Texture storageTexture = CreateTexture(
        wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding, kFormat);

    wgpu::Texture outputAttachment = CreateTexture(wgpu::TextureUsage::RenderAttachment, kFormat);
    utils::ComboRenderPassDescriptor renderPassDescriptor({outputAttachment.CreateView()});

    // Create a bind group that contains a storage texture and a sampled texture.
    for (wgpu::StorageTextureAccess storageTextureType : kSupportedStorageTextureAccess) {
        // Create a bind group that binds the same texture as both storage texture and sampled
        // texture.
        wgpu::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, storageTextureType, kFormat},
                     {1, wgpu::ShaderStage::Fragment, wgpu::TextureSampleType::Float}});
        wgpu::BindGroup bindGroup = utils::MakeBindGroup(
            device, bindGroupLayout,
            {{0, storageTexture.CreateView()}, {1, storageTexture.CreateView()}});

        // It is valid to use a a texture as both read-only storage texture and sampled texture in
        // one render pass, while it is invalid to use a texture as both write-only storage
        // texture an sampled texture in one render pass.
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder renderPassEncoder = encoder.BeginRenderPass(&renderPassDescriptor);
        renderPassEncoder.SetBindGroup(0, bindGroup);
        renderPassEncoder.End();
        switch (storageTextureType) {
            case wgpu::StorageTextureAccess::WriteOnly:
                ASSERT_DEVICE_ERROR(encoder.Finish());
                break;
            default:
                UNREACHABLE();
                break;
        }
    }
}

// Verify it is invalid to use a a texture as both storage texture (either read-only or write-only)
// and render attachment in one render pass.
TEST_F(StorageTextureValidationTests, StorageTextureAndRenderAttachmentInOneRenderPass) {
    constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::RGBA8Unorm;
    wgpu::Texture storageTexture = CreateTexture(
        wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::RenderAttachment, kFormat);
    utils::ComboRenderPassDescriptor renderPassDescriptor({storageTexture.CreateView()});

    for (wgpu::StorageTextureAccess storageTextureType : kSupportedStorageTextureAccess) {
        // Create a bind group that contains a storage texture.
        wgpu::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, storageTextureType, kFormat}});
        wgpu::BindGroup bindGroupWithStorageTexture =
            utils::MakeBindGroup(device, bindGroupLayout, {{0, storageTexture.CreateView()}});

        // It is invalid to use a texture as both storage texture and render attachment in one
        // render pass.
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder renderPassEncoder = encoder.BeginRenderPass(&renderPassDescriptor);
        renderPassEncoder.SetBindGroup(0, bindGroupWithStorageTexture);
        renderPassEncoder.End();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Verify it is valid to use a texture as both storage texture (read-only or write-only) and
// sampled texture in one compute pass.
TEST_F(StorageTextureValidationTests, StorageTextureAndSampledTextureInOneComputePass) {
    constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::RGBA8Unorm;
    wgpu::Texture storageTexture = CreateTexture(
        wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding, kFormat);

    for (wgpu::StorageTextureAccess storageTextureType : kSupportedStorageTextureAccess) {
        // Create a bind group that binds the same texture as both storage texture and sampled
        // texture.
        wgpu::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute, storageTextureType, kFormat},
                     {1, wgpu::ShaderStage::Compute, wgpu::TextureSampleType::Float}});
        wgpu::BindGroup bindGroup = utils::MakeBindGroup(
            device, bindGroupLayout,
            {{0, storageTexture.CreateView()}, {1, storageTexture.CreateView()}});

        // It is valid to use a a texture as both storage texture (read-only or write-only) and
        // sampled texture in one compute pass.
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder computePassEncoder = encoder.BeginComputePass();
        computePassEncoder.SetBindGroup(0, bindGroup);
        computePassEncoder.End();
        encoder.Finish();
    }
}
