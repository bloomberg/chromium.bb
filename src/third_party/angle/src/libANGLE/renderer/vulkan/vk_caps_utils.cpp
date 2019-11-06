//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_utils:
//    Helper functions for the Vulkan Caps.
//

#include "libANGLE/renderer/vulkan/vk_caps_utils.h"

#include <type_traits>

#include "common/utilities.h"
#include "libANGLE/Caps.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "vk_format_utils.h"

namespace
{
constexpr unsigned int kComponentsPerVector = 4;
}  // anonymous namespace

namespace rx
{

void RendererVk::ensureCapsInitialized() const
{
    if (mCapsInitialized)
        return;
    mCapsInitialized = true;

    ASSERT(mCurrentQueueFamilyIndex < mQueueFamilyProperties.size());
    const VkQueueFamilyProperties &queueFamilyProperties =
        mQueueFamilyProperties[mCurrentQueueFamilyIndex];
    const VkPhysicalDeviceLimits &limitsVk = mPhysicalDeviceProperties.limits;

    mNativeExtensions.setTextureExtensionSupport(mNativeTextureCaps);

    // Vulkan technically only supports the LDR profile but driver all appear to support the HDR
    // profile as well. http://anglebug.com/1185#c8
    mNativeExtensions.textureCompressionASTCHDRKHR = mNativeExtensions.textureCompressionASTCLDRKHR;

    // Enable this for simple buffer readback testing, but some functionality is missing.
    // TODO(jmadill): Support full mapBufferRange extension.
    mNativeExtensions.mapBuffer              = true;
    mNativeExtensions.mapBufferRange         = true;
    mNativeExtensions.textureStorage         = true;
    mNativeExtensions.drawBuffers            = true;
    mNativeExtensions.fragDepth              = true;
    mNativeExtensions.framebufferBlit        = true;
    mNativeExtensions.framebufferMultisample = true;
    mNativeExtensions.copyTexture            = true;
    mNativeExtensions.copyCompressedTexture  = true;
    mNativeExtensions.debugMarker            = true;
    mNativeExtensions.robustness             = true;
    mNativeExtensions.textureBorderClamp     = false;  // not implemented yet
    mNativeExtensions.translatedShaderSource = true;
    mNativeExtensions.discardFramebuffer     = true;

    // Enable EXT_texture_type_2_10_10_10_REV
    mNativeExtensions.textureFormat2101010REV = true;

    // Enable ANGLE_base_vertex_base_instance
    mNativeExtensions.baseVertexBaseInstance = true;

    // Enable EXT_blend_minmax
    mNativeExtensions.blendMinMax = true;

    mNativeExtensions.eglImage         = true;
    mNativeExtensions.eglImageExternal = true;
    // TODO(geofflang): Support GL_OES_EGL_image_external_essl3. http://anglebug.com/2668
    mNativeExtensions.eglImageExternalEssl3 = false;

    mNativeExtensions.memoryObject   = true;
    mNativeExtensions.memoryObjectFd = getFeatures().supportsExternalMemoryFd.enabled;

    mNativeExtensions.semaphore   = true;
    mNativeExtensions.semaphoreFd = getFeatures().supportsExternalSemaphoreFd.enabled;

    mNativeExtensions.vertexHalfFloat = true;

    // TODO: Enable this always and emulate instanced draws if any divisor exceeds the maximum
    // supported.  http://anglebug.com/2672
    mNativeExtensions.instancedArraysANGLE = mMaxVertexAttribDivisor > 1;
    mNativeExtensions.instancedArraysEXT   = mMaxVertexAttribDivisor > 1;

    // Only expose robust buffer access if the physical device supports it.
    mNativeExtensions.robustBufferAccessBehavior = mPhysicalDeviceFeatures.robustBufferAccess;

    mNativeExtensions.eglSync = true;

    // We use secondary command buffers almost everywhere and they require a feature to be
    // able to execute in the presence of queries.  As a result, we won't support queries
    // unless that feature is available.
    mNativeExtensions.occlusionQueryBoolean =
        vk::CommandBuffer::SupportsQueries(mPhysicalDeviceFeatures);

    // From the Vulkan specs:
    // > The number of valid bits in a timestamp value is determined by the
    // > VkQueueFamilyProperties::timestampValidBits property of the queue on which the timestamp is
    // > written. Timestamps are supported on any queue which reports a non-zero value for
    // > timestampValidBits via vkGetPhysicalDeviceQueueFamilyProperties.
    mNativeExtensions.disjointTimerQuery          = queueFamilyProperties.timestampValidBits > 0;
    mNativeExtensions.queryCounterBitsTimeElapsed = queueFamilyProperties.timestampValidBits;
    mNativeExtensions.queryCounterBitsTimestamp   = queueFamilyProperties.timestampValidBits;

    mNativeExtensions.textureFilterAnisotropic =
        mPhysicalDeviceFeatures.samplerAnisotropy && limitsVk.maxSamplerAnisotropy > 1.0f;
    mNativeExtensions.maxTextureAnisotropy =
        mNativeExtensions.textureFilterAnisotropic ? limitsVk.maxSamplerAnisotropy : 0.0f;

    // Vulkan natively supports non power-of-two textures
    mNativeExtensions.textureNPOT = true;

    mNativeExtensions.texture3DOES = true;

    // Vulkan natively supports standard derivatives
    mNativeExtensions.standardDerivatives = true;

    // Vulkan natively supports 32-bit indices, entry in kIndexTypeMap
    mNativeExtensions.elementIndexUint = true;

    // https://vulkan.lunarg.com/doc/view/1.0.30.0/linux/vkspec.chunked/ch31s02.html
    mNativeCaps.maxElementIndex       = std::numeric_limits<GLuint>::max() - 1;
    mNativeCaps.max3DTextureSize      = limitsVk.maxImageDimension3D;
    mNativeCaps.max2DTextureSize      = limitsVk.maxImageDimension2D;
    mNativeCaps.maxArrayTextureLayers = limitsVk.maxImageArrayLayers;
    mNativeCaps.maxLODBias            = limitsVk.maxSamplerLodBias;
    mNativeCaps.maxCubeMapTextureSize = limitsVk.maxImageDimensionCube;
    mNativeCaps.maxRenderbufferSize   = mNativeCaps.max2DTextureSize;
    mNativeCaps.minAliasedPointSize   = std::max(1.0f, limitsVk.pointSizeRange[0]);
    mNativeCaps.maxAliasedPointSize   = limitsVk.pointSizeRange[1];

    mNativeCaps.minAliasedLineWidth = 1.0f;
    mNativeCaps.maxAliasedLineWidth = 1.0f;

    mNativeCaps.maxDrawBuffers =
        std::min<uint32_t>(limitsVk.maxColorAttachments, limitsVk.maxFragmentOutputAttachments);
    mNativeCaps.maxFramebufferWidth    = limitsVk.maxFramebufferWidth;
    mNativeCaps.maxFramebufferHeight   = limitsVk.maxFramebufferHeight;
    mNativeCaps.maxColorAttachments    = limitsVk.maxColorAttachments;
    mNativeCaps.maxViewportWidth       = limitsVk.maxViewportDimensions[0];
    mNativeCaps.maxViewportHeight      = limitsVk.maxViewportDimensions[1];
    mNativeCaps.maxSampleMaskWords     = limitsVk.maxSampleMaskWords;
    mNativeCaps.maxColorTextureSamples = limitsVk.sampledImageColorSampleCounts;
    mNativeCaps.maxDepthTextureSamples = limitsVk.sampledImageDepthSampleCounts;
    mNativeCaps.maxIntegerSamples      = limitsVk.sampledImageIntegerSampleCounts;

    mNativeCaps.maxVertexAttributes     = limitsVk.maxVertexInputAttributes;
    mNativeCaps.maxVertexAttribBindings = limitsVk.maxVertexInputBindings;
    // Offset and stride are stored as uint16_t in PackedAttribDesc.
    mNativeCaps.maxVertexAttribRelativeOffset =
        std::min(static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()),
                 limitsVk.maxVertexInputAttributeOffset);
    mNativeCaps.maxVertexAttribStride =
        std::min(static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()),
                 limitsVk.maxVertexInputBindingStride);

    mNativeCaps.maxElementsIndices  = std::numeric_limits<GLint>::max();
    mNativeCaps.maxElementsVertices = std::numeric_limits<GLint>::max();

    // Looks like all floats are IEEE according to the docs here:
    // https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/html/vkspec.html#spirvenv-precision-operation
    mNativeCaps.vertexHighpFloat.setIEEEFloat();
    mNativeCaps.vertexMediumpFloat.setIEEEFloat();
    mNativeCaps.vertexLowpFloat.setIEEEFloat();
    mNativeCaps.fragmentHighpFloat.setIEEEFloat();
    mNativeCaps.fragmentMediumpFloat.setIEEEFloat();
    mNativeCaps.fragmentLowpFloat.setIEEEFloat();

    // Can't find documentation on the int precision in Vulkan.
    mNativeCaps.vertexHighpInt.setTwosComplementInt(32);
    mNativeCaps.vertexMediumpInt.setTwosComplementInt(32);
    mNativeCaps.vertexLowpInt.setTwosComplementInt(32);
    mNativeCaps.fragmentHighpInt.setTwosComplementInt(32);
    mNativeCaps.fragmentMediumpInt.setTwosComplementInt(32);
    mNativeCaps.fragmentLowpInt.setTwosComplementInt(32);

    // Compute shader limits.
    mNativeCaps.maxComputeWorkGroupCount[0]    = limitsVk.maxComputeWorkGroupCount[0];
    mNativeCaps.maxComputeWorkGroupCount[1]    = limitsVk.maxComputeWorkGroupCount[1];
    mNativeCaps.maxComputeWorkGroupCount[2]    = limitsVk.maxComputeWorkGroupCount[2];
    mNativeCaps.maxComputeWorkGroupSize[0]     = limitsVk.maxComputeWorkGroupSize[0];
    mNativeCaps.maxComputeWorkGroupSize[1]     = limitsVk.maxComputeWorkGroupSize[1];
    mNativeCaps.maxComputeWorkGroupSize[2]     = limitsVk.maxComputeWorkGroupSize[2];
    mNativeCaps.maxComputeWorkGroupInvocations = limitsVk.maxComputeWorkGroupInvocations;
    mNativeCaps.maxComputeSharedMemorySize     = limitsVk.maxComputeSharedMemorySize;

    // TODO(lucferron): This is something we'll need to implement custom in the back-end.
    // Vulkan doesn't do any waiting for you, our back-end code is going to manage sync objects,
    // and we'll have to check that we've exceeded the max wait timeout. Also, this is ES 3.0 so
    // we'll defer the implementation until we tackle the next version.
    // mNativeCaps.maxServerWaitTimeout

    GLuint maxUniformBlockSize = limitsVk.maxUniformBufferRange;

    // Clamp the maxUniformBlockSize to 64KB (majority of devices support up to this size
    // currently), on AMD the maxUniformBufferRange is near uint32_t max.
    maxUniformBlockSize = std::min(0x10000u, maxUniformBlockSize);

    const GLuint maxUniformVectors = maxUniformBlockSize / (sizeof(GLfloat) * kComponentsPerVector);
    const GLuint maxUniformComponents = maxUniformVectors * kComponentsPerVector;

    // Uniforms are implemented using a uniform buffer, so the max number of uniforms we can
    // support is the max buffer range divided by the size of a single uniform (4X float).
    mNativeCaps.maxVertexUniformVectors                              = maxUniformVectors;
    mNativeCaps.maxFragmentUniformVectors                            = maxUniformVectors;
    mNativeCaps.maxFragmentInputComponents                           = maxUniformComponents;
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mNativeCaps.maxShaderUniformComponents[shaderType] = maxUniformComponents;
    }
    mNativeCaps.maxUniformLocations                                  = maxUniformVectors;

    // Every stage has 1 reserved uniform buffer for the default uniforms, and 1 for the driver
    // uniforms.
    constexpr uint32_t kTotalReservedPerStageUniformBuffers =
        kReservedDriverUniformBindingCount + kReservedPerStageDefaultUniformBindingCount;
    constexpr uint32_t kTotalReservedUniformBuffers =
        kReservedDriverUniformBindingCount + kReservedDefaultUniformBindingCount;

    const uint32_t maxPerStageUniformBuffers =
        limitsVk.maxPerStageDescriptorUniformBuffers - kTotalReservedPerStageUniformBuffers;
    const uint32_t maxCombinedUniformBuffers =
        limitsVk.maxDescriptorSetUniformBuffers - kTotalReservedUniformBuffers;
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mNativeCaps.maxShaderUniformBlocks[shaderType] = maxPerStageUniformBuffers;
    }
    mNativeCaps.maxCombinedUniformBlocks                         = maxCombinedUniformBuffers;

    mNativeCaps.maxUniformBufferBindings = maxCombinedUniformBuffers;
    mNativeCaps.maxUniformBlockSize      = maxUniformBlockSize;
    mNativeCaps.uniformBufferOffsetAlignment =
        static_cast<GLuint>(limitsVk.minUniformBufferOffsetAlignment);

    // Note that Vulkan currently implements textures as combined image+samplers, so the limit is
    // the minimum of supported samplers and sampled images.
    const uint32_t maxPerStageTextures = std::min(limitsVk.maxPerStageDescriptorSamplers,
                                                  limitsVk.maxPerStageDescriptorSampledImages);
    const uint32_t maxCombinedTextures =
        std::min(limitsVk.maxDescriptorSetSamplers, limitsVk.maxDescriptorSetSampledImages);
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mNativeCaps.maxShaderTextureImageUnits[shaderType] = maxPerStageTextures;
    }
    mNativeCaps.maxCombinedTextureImageUnits                         = maxCombinedTextures;

    uint32_t maxPerStageStorageBuffers    = limitsVk.maxPerStageDescriptorStorageBuffers;
    uint32_t maxVertexStageStorageBuffers = maxPerStageStorageBuffers;
    uint32_t maxCombinedStorageBuffers    = limitsVk.maxDescriptorSetStorageBuffers;

    // A number of storage buffer slots are used in the vertex shader to emulate transform feedback.
    // Note that Vulkan requires maxPerStageDescriptorStorageBuffers to be at least 4 (i.e. the same
    // as gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS).
    // TODO(syoussefi): This should be conditioned to transform feedback extension not being
    // present.  http://anglebug.com/3206.
    // TODO(syoussefi): If geometry shader is supported, emulation will be done at that stage, and
    // so the reserved storage buffers should be accounted in that stage.  http://anglebug.com/3606
    static_assert(
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS == 4,
        "Limit to ES2.0 if supported SSBO count < supporting transform feedback buffer count");
    if (mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics)
    {
        ASSERT(maxVertexStageStorageBuffers >= gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS);
        maxVertexStageStorageBuffers -= gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS;
        maxCombinedStorageBuffers -= gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS;

        // Cap the per-stage limit of the other stages to the combined limit, in case the combined
        // limit is now lower than that.
        maxPerStageStorageBuffers = std::min(maxPerStageStorageBuffers, maxCombinedStorageBuffers);
    }

    // Reserve up to IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS storage buffers in the fragment and
    // compute stages for atomic counters.  This is only possible if the number of per-stage storage
    // buffers is greater than 4, which is the required GLES minimum for compute.
    //
    // For each stage, we'll either not support atomic counter buffers, or support exactly
    // IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS.  This is due to restrictions in the shader
    // translator where we can't know how many atomic counter buffers we would really need after
    // linking so we can't create a packed buffer array.
    //
    // For the vertex stage, we could support atomic counters without storage buffers, but that's
    // likely not very useful, so we use the same limit (4 + MAX_ATOMIC_COUNTER_BUFFERS) for the
    // vertex stage to determine if we would want to add support for atomic counter buffers.
    constexpr uint32_t kMinimumStorageBuffersForAtomicCounterBufferSupport =
        gl::limits::kMinimumComputeStorageBuffers + gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS;
    uint32_t maxVertexStageAtomicCounterBuffers = 0;
    uint32_t maxPerStageAtomicCounterBuffers    = 0;
    uint32_t maxCombinedAtomicCounterBuffers    = 0;

    if (maxPerStageStorageBuffers >= kMinimumStorageBuffersForAtomicCounterBufferSupport)
    {
        maxPerStageAtomicCounterBuffers = gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS;
        maxCombinedAtomicCounterBuffers = gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS;
    }

    if (maxVertexStageStorageBuffers >= kMinimumStorageBuffersForAtomicCounterBufferSupport)
    {
        maxVertexStageAtomicCounterBuffers = gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS;
    }

    maxVertexStageStorageBuffers -= maxVertexStageAtomicCounterBuffers;
    maxPerStageStorageBuffers -= maxPerStageAtomicCounterBuffers;
    maxCombinedStorageBuffers -= maxCombinedAtomicCounterBuffers;

    mNativeCaps.maxShaderStorageBlocks[gl::ShaderType::Vertex] =
        mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics ? maxVertexStageStorageBuffers : 0;
    mNativeCaps.maxShaderStorageBlocks[gl::ShaderType::Fragment] =
        mPhysicalDeviceFeatures.fragmentStoresAndAtomics ? maxPerStageStorageBuffers : 0;
    mNativeCaps.maxShaderStorageBlocks[gl::ShaderType::Compute] = maxPerStageStorageBuffers;
    mNativeCaps.maxCombinedShaderStorageBlocks                  = maxCombinedStorageBuffers;

    mNativeCaps.maxShaderStorageBufferBindings = maxCombinedStorageBuffers;
    mNativeCaps.maxShaderStorageBlockSize      = limitsVk.maxStorageBufferRange;
    mNativeCaps.shaderStorageBufferOffsetAlignment =
        static_cast<GLuint>(limitsVk.minStorageBufferOffsetAlignment);

    mNativeCaps.maxShaderAtomicCounterBuffers[gl::ShaderType::Vertex] =
        mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics ? maxVertexStageAtomicCounterBuffers
                                                               : 0;
    mNativeCaps.maxShaderAtomicCounterBuffers[gl::ShaderType::Fragment] =
        mPhysicalDeviceFeatures.fragmentStoresAndAtomics ? maxPerStageAtomicCounterBuffers : 0;
    mNativeCaps.maxShaderAtomicCounterBuffers[gl::ShaderType::Compute] =
        maxPerStageAtomicCounterBuffers;
    mNativeCaps.maxCombinedAtomicCounterBuffers = maxCombinedAtomicCounterBuffers;

    mNativeCaps.maxAtomicCounterBufferBindings = maxCombinedAtomicCounterBuffers;
    // Emulated as storage buffers, atomic counter buffers have the same size limit.  However, the
    // limit is a signed integer and values above int max will end up as a negative size.
    mNativeCaps.maxAtomicCounterBufferSize =
        std::min<uint32_t>(std::numeric_limits<int32_t>::max(), limitsVk.maxStorageBufferRange);

    // There is no particular limit to how many atomic counters there can be, other than the size of
    // a storage buffer.  We nevertheless limit this to something sane (4096 arbitrarily).
    const uint32_t maxAtomicCounters =
        std::min<uint32_t>(4096, limitsVk.maxStorageBufferRange / sizeof(uint32_t));
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mNativeCaps.maxShaderAtomicCounters[shaderType] = maxAtomicCounters;
    }
    mNativeCaps.maxCombinedAtomicCounters = maxAtomicCounters;

    // GL Images correspond to Vulkan Storage Images.
    const uint32_t maxPerStageImages = limitsVk.maxPerStageDescriptorStorageImages;
    const uint32_t maxCombinedImages = limitsVk.maxDescriptorSetStorageImages;
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mNativeCaps.maxShaderImageUniforms[shaderType] = maxPerStageImages;
    }
    mNativeCaps.maxCombinedImageUniforms = maxCombinedImages;
    mNativeCaps.maxImageUnits            = maxCombinedImages;

    mNativeCaps.minProgramTexelOffset = mPhysicalDeviceProperties.limits.minTexelOffset;
    mNativeCaps.maxProgramTexelOffset = mPhysicalDeviceProperties.limits.maxTexelOffset;

    // There is no additional limit to the combined number of components.  We can have up to a
    // maximum number of uniform buffers, each having the maximum number of components.  Note that
    // this limit includes both components in and out of uniform buffers.
    const uint32_t maxCombinedUniformComponents =
        (maxPerStageUniformBuffers + kReservedPerStageDefaultUniformBindingCount) *
        maxUniformComponents;
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        mNativeCaps.maxCombinedShaderUniformComponents[shaderType] = maxCombinedUniformComponents;
    }

    // Total number of resources available to the user are as many as Vulkan allows minus everything
    // that ANGLE uses internally.  That is, one dynamic uniform buffer used per stage for default
    // uniforms and a single dynamic uniform buffer for driver uniforms.  Additionally, Vulkan uses
    // up to IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS + 1 buffers for transform feedback (Note:
    // +1 is for the "counter" buffer of transform feedback, which will be necessary for transform
    // feedback extension and ES3.2 transform feedback emulation, but is not yet present).
    constexpr uint32_t kReservedPerStageUniformBufferCount = 1;
    constexpr uint32_t kReservedPerStageBindingCount =
        kReservedDriverUniformBindingCount + kReservedPerStageUniformBufferCount +
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS + 1;

    // Note: maxPerStageResources is required to be at least the sum of per stage UBOs, SSBOs etc
    // which total a minimum of 44 resources, so no underflow is possible here.  Limit the total
    // number of resources reported by Vulkan to 2 billion though to avoid seeing negative numbers
    // in applications that take the value as signed int (including dEQP).
    const uint32_t maxPerStageResources =
        std::min<uint32_t>(std::numeric_limits<int32_t>::max(), limitsVk.maxPerStageResources);
    mNativeCaps.maxCombinedShaderOutputResources =
        maxPerStageResources - kReservedPerStageBindingCount;

    // The max vertex output components should not include gl_Position.
    // The gles2.0 section 2.10 states that "gl_Position is not a varying variable and does
    // not count against this limit.", but the Vulkan spec has no such mention in its Built-in
    // vars section. It is implicit that we need to actually reserve it for Vulkan in that case.
    //
    // Note: AMD has a weird behavior when we edge toward the maximum number of varyings and can
    // often crash. Reserving an additional varying just for them bringing the total to 2.
    constexpr GLint kReservedVaryingCount = 2;
    mNativeCaps.maxVaryingVectors =
        (limitsVk.maxVertexOutputComponents / 4) - kReservedVaryingCount;
    mNativeCaps.maxVertexOutputComponents = mNativeCaps.maxVaryingVectors * 4;

    mNativeCaps.maxTransformFeedbackInterleavedComponents =
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS;
    mNativeCaps.maxTransformFeedbackSeparateAttributes =
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS;
    mNativeCaps.maxTransformFeedbackSeparateComponents =
        gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS;

    mNativeCaps.minProgramTexelOffset = limitsVk.minTexelOffset;
    mNativeCaps.maxProgramTexelOffset = limitsVk.maxTexelOffset;

    const uint32_t sampleCounts = limitsVk.framebufferColorSampleCounts &
                                  limitsVk.framebufferDepthSampleCounts &
                                  limitsVk.framebufferStencilSampleCounts;

    mNativeCaps.maxSamples            = vk_gl::GetMaxSampleCount(sampleCounts);
    mNativeCaps.maxFramebufferSamples = mNativeCaps.maxSamples;

    mNativeCaps.subPixelBits = limitsVk.subPixelPrecisionBits;

    // Enable Program Binary extension.
    mNativeExtensions.getProgramBinary = true;
    mNativeCaps.programBinaryFormats.push_back(GL_PROGRAM_BINARY_ANGLE);

    // Enable GL_NV_pixel_buffer_object extension.
    mNativeExtensions.pixelBufferObject = true;
}

namespace egl_vk
{

namespace
{

EGLint ComputeMaximumPBufferPixels(const VkPhysicalDeviceProperties &physicalDeviceProperties)
{
    // EGLints are signed 32-bit integers, it's fairly easy to overflow them, especially since
    // Vulkan's minimum guaranteed VkImageFormatProperties::maxResourceSize is 2^31 bytes.
    constexpr uint64_t kMaxValueForEGLint =
        static_cast<uint64_t>(std::numeric_limits<EGLint>::max());

    // TODO(geofflang): Compute the maximum size of a pbuffer by using the maxResourceSize result
    // from vkGetPhysicalDeviceImageFormatProperties for both the color and depth stencil format and
    // the exact image creation parameters that would be used to create the pbuffer. Because it is
    // always safe to return out-of-memory errors on pbuffer allocation, it's fine to simply return
    // the number of pixels in a max width by max height pbuffer for now. http://anglebug.com/2622

    // Storing the result of squaring a 32-bit unsigned int in a 64-bit unsigned int is safe.
    static_assert(std::is_same<decltype(physicalDeviceProperties.limits.maxImageDimension2D),
                               uint32_t>::value,
                  "physicalDeviceProperties.limits.maxImageDimension2D expected to be a uint32_t.");
    const uint64_t maxDimensionsSquared =
        static_cast<uint64_t>(physicalDeviceProperties.limits.maxImageDimension2D) *
        static_cast<uint64_t>(physicalDeviceProperties.limits.maxImageDimension2D);

    return static_cast<EGLint>(std::min(maxDimensionsSquared, kMaxValueForEGLint));
}

// Generates a basic config for a combination of color format, depth stencil format and sample
// count.
egl::Config GenerateDefaultConfig(const RendererVk *renderer,
                                  const gl::InternalFormat &colorFormat,
                                  const gl::InternalFormat &depthStencilFormat,
                                  EGLint sampleCount)
{
    const VkPhysicalDeviceProperties &physicalDeviceProperties =
        renderer->getPhysicalDeviceProperties();
    gl::Version maxSupportedESVersion = renderer->getMaxSupportedESVersion();

    EGLint es2Support = (maxSupportedESVersion.major >= 2 ? EGL_OPENGL_ES2_BIT : 0);
    EGLint es3Support = (maxSupportedESVersion.major >= 3 ? EGL_OPENGL_ES3_BIT : 0);

    egl::Config config;

    config.renderTargetFormat = colorFormat.internalFormat;
    config.depthStencilFormat = depthStencilFormat.internalFormat;
    config.bufferSize         = colorFormat.pixelBytes * 8;
    config.redSize            = colorFormat.redBits;
    config.greenSize          = colorFormat.greenBits;
    config.blueSize           = colorFormat.blueBits;
    config.alphaSize          = colorFormat.alphaBits;
    config.alphaMaskSize      = 0;
    config.bindToTextureRGB   = colorFormat.format == GL_RGB;
    config.bindToTextureRGBA  = colorFormat.format == GL_RGBA || colorFormat.format == GL_BGRA_EXT;
    config.colorBufferType    = EGL_RGB_BUFFER;
    config.configCaveat       = EGL_NONE;
    config.conformant         = es2Support | es3Support;
    config.depthSize          = depthStencilFormat.depthBits;
    config.stencilSize        = depthStencilFormat.stencilBits;
    config.level              = 0;
    config.matchNativePixmap  = EGL_NONE;
    config.maxPBufferWidth    = physicalDeviceProperties.limits.maxImageDimension2D;
    config.maxPBufferHeight   = physicalDeviceProperties.limits.maxImageDimension2D;
    config.maxPBufferPixels   = ComputeMaximumPBufferPixels(physicalDeviceProperties);
    config.maxSwapInterval    = 1;
    config.minSwapInterval    = 0;
    config.nativeRenderable   = EGL_TRUE;
    config.nativeVisualID     = 0;
    config.nativeVisualType   = EGL_NONE;
    config.renderableType     = es2Support | es3Support;
    config.sampleBuffers      = (sampleCount > 0) ? 1 : 0;
    config.samples            = sampleCount;
    config.surfaceType        = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;
    // Vulkan surfaces use a different origin than OpenGL, always prefer to be flipped vertically if
    // possible.
    config.optimalOrientation    = EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE;
    config.transparentType       = EGL_NONE;
    config.transparentRedValue   = 0;
    config.transparentGreenValue = 0;
    config.transparentBlueValue  = 0;
    config.colorComponentType =
        gl_egl::GLComponentTypeToEGLColorComponentType(colorFormat.componentType);

    return config;
}

}  // anonymous namespace

egl::ConfigSet GenerateConfigs(const GLenum *colorFormats,
                               size_t colorFormatsCount,
                               const GLenum *depthStencilFormats,
                               size_t depthStencilFormatCount,
                               DisplayVk *display)
{
    ASSERT(colorFormatsCount > 0);
    ASSERT(display != nullptr);

    gl::SupportedSampleSet colorSampleCounts;
    gl::SupportedSampleSet depthStencilSampleCounts;
    gl::SupportedSampleSet sampleCounts;

    const VkPhysicalDeviceLimits &limits =
        display->getRenderer()->getPhysicalDeviceProperties().limits;
    const uint32_t depthStencilSampleCountsLimit =
        limits.framebufferDepthSampleCounts & limits.framebufferStencilSampleCounts;

    vk_gl::AddSampleCounts(limits.framebufferColorSampleCounts, &colorSampleCounts);
    vk_gl::AddSampleCounts(depthStencilSampleCountsLimit, &depthStencilSampleCounts);

    // Always support 0 samples
    colorSampleCounts.insert(0);
    depthStencilSampleCounts.insert(0);

    std::set_intersection(colorSampleCounts.begin(), colorSampleCounts.end(),
                          depthStencilSampleCounts.begin(), depthStencilSampleCounts.end(),
                          std::inserter(sampleCounts, sampleCounts.begin()));

    egl::ConfigSet configSet;

    for (size_t colorFormatIdx = 0; colorFormatIdx < colorFormatsCount; colorFormatIdx++)
    {
        const gl::InternalFormat &colorFormatInfo =
            gl::GetSizedInternalFormatInfo(colorFormats[colorFormatIdx]);
        ASSERT(colorFormatInfo.sized);

        for (size_t depthStencilFormatIdx = 0; depthStencilFormatIdx < depthStencilFormatCount;
             depthStencilFormatIdx++)
        {
            const gl::InternalFormat &depthStencilFormatInfo =
                gl::GetSizedInternalFormatInfo(depthStencilFormats[depthStencilFormatIdx]);
            ASSERT(depthStencilFormats[depthStencilFormatIdx] == GL_NONE ||
                   depthStencilFormatInfo.sized);

            const gl::SupportedSampleSet *configSampleCounts = &sampleCounts;
            // If there is no depth/stencil buffer, use the color samples set.
            if (depthStencilFormats[depthStencilFormatIdx] == GL_NONE)
            {
                configSampleCounts = &colorSampleCounts;
            }
            // If there is no color buffer, use the depth/stencil samples set.
            else if (colorFormats[colorFormatIdx] == GL_NONE)
            {
                configSampleCounts = &depthStencilSampleCounts;
            }

            for (EGLint sampleCount : *configSampleCounts)
            {
                egl::Config config = GenerateDefaultConfig(display->getRenderer(), colorFormatInfo,
                                                           depthStencilFormatInfo, sampleCount);
                if (display->checkConfigSupport(&config))
                {
                    configSet.add(config);
                }
            }
        }
    }

    return configSet;
}

}  // namespace egl_vk

}  // namespace rx
