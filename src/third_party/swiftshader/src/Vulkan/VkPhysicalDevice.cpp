// Copyright 2018 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "VkPhysicalDevice.hpp"
#include "VkConfig.h"

#include <cstring>

namespace vk
{

PhysicalDevice::PhysicalDevice(const void*, void* mem)
{
}

const VkPhysicalDeviceFeatures& PhysicalDevice::getFeatures() const
{
	static const VkPhysicalDeviceFeatures features
	{
		true,  // robustBufferAccess
		false, // fullDrawIndexUint32
		false, // imageCubeArray
		false, // independentBlend
		false, // geometryShader
		false, // tessellationShader
		false, // sampleRateShading
		false, // dualSrcBlend
		false, // logicOp
		false, // multiDrawIndirect
		false, // drawIndirectFirstInstance
		false, // depthClamp
		false, // depthBiasClamp
		false, // fillModeNonSolid
		false, // depthBounds
		false, // wideLines
		false, // largePoints
		false, // alphaToOne
		false, // multiViewport
		false, // samplerAnisotropy
		true,  // textureCompressionETC2
		false, // textureCompressionASTC_LDR
		false, // textureCompressionBC
		false, // occlusionQueryPrecise
		false, // pipelineStatisticsQuery
		false, // vertexPipelineStoresAndAtomics
		false, // fragmentStoresAndAtomics
		false, // shaderTessellationAndGeometryPointSize
		false, // shaderImageGatherExtended
		false, // shaderStorageImageExtendedFormats
		false, // shaderStorageImageMultisample
		false, // shaderStorageImageReadWithoutFormat
		false, // shaderStorageImageWriteWithoutFormat
		false, // shaderUniformBufferArrayDynamicIndexing
		false, // shaderSampledImageArrayDynamicIndexing
		false, // shaderStorageBufferArrayDynamicIndexing
		false, // shaderStorageImageArrayDynamicIndexing
		false, // shaderClipDistance
		false, // shaderCullDistance
		false, // shaderFloat64
		false, // shaderInt64
		false, // shaderInt16
		false, // shaderResourceResidency
		false, // shaderResourceMinLod
		false, // sparseBinding
		false, // sparseResidencyBuffer
		false, // sparseResidencyImage2D
		false, // sparseResidencyImage3D
		false, // sparseResidency2Samples
		false, // sparseResidency4Samples
		false, // sparseResidency8Samples
		false, // sparseResidency16Samples
		false, // sparseResidencyAliased
		false, // variableMultisampleRate
		false, // inheritedQueries
	};

	return features;
}

void PhysicalDevice::getFeatures(VkPhysicalDeviceSamplerYcbcrConversionFeatures* features) const
{
	features->samplerYcbcrConversion = VK_FALSE;
}

void PhysicalDevice::getFeatures(VkPhysicalDevice16BitStorageFeatures* features) const
{
	features->storageBuffer16BitAccess = VK_FALSE;
	features->storageInputOutput16 = VK_FALSE;
	features->storagePushConstant16 = VK_FALSE;
	features->uniformAndStorageBuffer16BitAccess = VK_FALSE;
}

void PhysicalDevice::getFeatures(VkPhysicalDeviceVariablePointerFeatures* features) const
{
	features->variablePointersStorageBuffer = VK_FALSE;
	features->variablePointers = VK_FALSE;
}

void PhysicalDevice::getFeatures(VkPhysicalDevice8BitStorageFeaturesKHR* features) const
{
	features->storageBuffer8BitAccess = VK_FALSE;
	features->uniformAndStorageBuffer8BitAccess = VK_FALSE;
	features->storagePushConstant8 = VK_FALSE;
}

void PhysicalDevice::getFeatures(VkPhysicalDeviceMultiviewFeatures* features) const
{
	features->multiview = VK_FALSE;
	features->multiviewGeometryShader = VK_FALSE;
	features->multiviewTessellationShader = VK_FALSE;
}

void PhysicalDevice::getFeatures(VkPhysicalDeviceProtectedMemoryFeatures* features) const
{
	features->protectedMemory = VK_FALSE;
}

VkSampleCountFlags PhysicalDevice::getSampleCounts() const
{
	return VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;
}

const VkPhysicalDeviceLimits& PhysicalDevice::getLimits() const
{
	VkSampleCountFlags sampleCounts = getSampleCounts();

	static const VkPhysicalDeviceLimits limits =
	{
		(1 << vk::MAX_IMAGE_LEVELS_1D), // maxImageDimension1D
		(1 << vk::MAX_IMAGE_LEVELS_2D), // maxImageDimension2D
		(1 << vk::MAX_IMAGE_LEVELS_3D), // maxImageDimension3D
		(1 << vk::MAX_IMAGE_LEVELS_CUBE), // maxImageDimensionCube
		(1 << vk::MAX_IMAGE_ARRAY_LAYERS), // maxImageArrayLayers
		65536, // maxTexelBufferElements
		16384, // maxUniformBufferRange
		(1ul << 27), // maxStorageBufferRange
		128, // maxPushConstantsSize
		4096, // maxMemoryAllocationCount
		4000, // maxSamplerAllocationCount
		131072, // bufferImageGranularity
		0, // sparseAddressSpaceSize (unsupported)
		MAX_BOUND_DESCRIPTOR_SETS, // maxBoundDescriptorSets
		16, // maxPerStageDescriptorSamplers
		12, // maxPerStageDescriptorUniformBuffers
		4, // maxPerStageDescriptorStorageBuffers
		16, // maxPerStageDescriptorSampledImages
		4, // maxPerStageDescriptorStorageImages
		4, // maxPerStageDescriptorInputAttachments
		128, // maxPerStageResources
		96, // maxDescriptorSetSamplers
		72, // maxDescriptorSetUniformBuffers
		8, // maxDescriptorSetUniformBuffersDynamic
		24, // maxDescriptorSetStorageBuffers
		4, // maxDescriptorSetStorageBuffersDynamic
		96, // maxDescriptorSetSampledImages
		24, // maxDescriptorSetStorageImages
		4, // maxDescriptorSetInputAttachments
		16, // maxVertexInputAttributes
		vk::MAX_VERTEX_INPUT_BINDINGS, // maxVertexInputBindings
		2047, // maxVertexInputAttributeOffset
		2048, // maxVertexInputBindingStride
		64, // maxVertexOutputComponents
		0, // maxTessellationGenerationLevel (unsupported)
		0, // maxTessellationPatchSize (unsupported)
		0, // maxTessellationControlPerVertexInputComponents (unsupported)
		0, // maxTessellationControlPerVertexOutputComponents (unsupported)
		0, // maxTessellationControlPerPatchOutputComponents (unsupported)
		0, // maxTessellationControlTotalOutputComponents (unsupported)
		0, // maxTessellationEvaluationInputComponents (unsupported)
		0, // maxTessellationEvaluationOutputComponents (unsupported)
		0, // maxGeometryShaderInvocations (unsupported)
		0, // maxGeometryInputComponents (unsupported)
		0, // maxGeometryOutputComponents (unsupported)
		0, // maxGeometryOutputVertices (unsupported)
		0, // maxGeometryTotalOutputComponents (unsupported)
		64, // maxFragmentInputComponents
		4, // maxFragmentOutputAttachments
		1, // maxFragmentDualSrcAttachments
		4, // maxFragmentCombinedOutputResources
		16384, // maxComputeSharedMemorySize
		{ 65535, 65535, 65535 }, // maxComputeWorkGroupCount[3]
		128, // maxComputeWorkGroupInvocations
		{ 128, 128, 64, }, // maxComputeWorkGroupSize[3]
		4, // subPixelPrecisionBits
		4, // subTexelPrecisionBits
		4, // mipmapPrecisionBits
		UINT32_MAX, // maxDrawIndexedIndexValue
		UINT32_MAX, // maxDrawIndirectCount
		2, // maxSamplerLodBias
		16, // maxSamplerAnisotropy
		16, // maxViewports
		{ 4096, 4096 }, // maxViewportDimensions[2]
		{ -8192, 8191 }, // viewportBoundsRange[2]
		0, // viewportSubPixelBits
		64, // minMemoryMapAlignment
		vk::MIN_TEXEL_BUFFER_OFFSET_ALIGNMENT, // minTexelBufferOffsetAlignment
		vk::MIN_UNIFORM_BUFFER_OFFSET_ALIGNMENT, // minUniformBufferOffsetAlignment
		vk::MIN_STORAGE_BUFFER_OFFSET_ALIGNMENT, // minStorageBufferOffsetAlignment
		-8, // minTexelOffset
		7, // maxTexelOffset
		-8, // minTexelGatherOffset
		7, // maxTexelGatherOffset
		-0.5, // minInterpolationOffset
		0.5, // maxInterpolationOffset
		4, // subPixelInterpolationOffsetBits
		4096, // maxFramebufferWidth
		4096, // maxFramebufferHeight
		256, // maxFramebufferLayers
		sampleCounts, // framebufferColorSampleCounts
		sampleCounts, // framebufferDepthSampleCounts
		sampleCounts, // framebufferStencilSampleCounts
		sampleCounts, // framebufferNoAttachmentsSampleCounts
		4,  // maxColorAttachments
		sampleCounts, // sampledImageColorSampleCounts
		VK_SAMPLE_COUNT_1_BIT, // sampledImageIntegerSampleCounts
		sampleCounts, // sampledImageDepthSampleCounts
		sampleCounts, // sampledImageStencilSampleCounts
		VK_SAMPLE_COUNT_1_BIT, // storageImageSampleCounts (unsupported)
		1, // maxSampleMaskWords
		false, // timestampComputeAndGraphics
		60, // timestampPeriod
		8, // maxClipDistances
		8, // maxCullDistances
		8, // maxCombinedClipAndCullDistances
		2, // discreteQueuePriorities
		{ 1.0, vk::MAX_POINT_SIZE }, // pointSizeRange[2]
		{ 1.0, 1.0 }, // lineWidthRange[2] (unsupported)
		0.0, // pointSizeGranularity (unsupported)
		0.0, // lineWidthGranularity (unsupported)
		false, // strictLines
		true, // standardSampleLocations
		64, // optimalBufferCopyOffsetAlignment
		64, // optimalBufferCopyRowPitchAlignment
		256, // nonCoherentAtomSize
	};

	return limits;
}

const VkPhysicalDeviceProperties& PhysicalDevice::getProperties() const
{
	static const VkPhysicalDeviceProperties properties
	{
		API_VERSION,
		DRIVER_VERSION,
		VENDOR_ID,
		DEVICE_ID,
		VK_PHYSICAL_DEVICE_TYPE_CPU, // deviceType
		SWIFTSHADER_DEVICE_NAME, // deviceName
		SWIFTSHADER_UUID, // pipelineCacheUUID
		getLimits(), // limits
		{ 0 } // sparseProperties
	};

	return properties;
}

void PhysicalDevice::getProperties(VkPhysicalDeviceIDProperties* properties) const
{
	memset(properties->deviceUUID, 0, VK_UUID_SIZE);
	memset(properties->driverUUID, 0, VK_UUID_SIZE);
	memset(properties->deviceLUID, 0, VK_LUID_SIZE);

	memcpy(properties->deviceUUID, SWIFTSHADER_UUID, VK_UUID_SIZE);
	*((uint64_t*)properties->driverUUID) = DRIVER_VERSION;

	properties->deviceNodeMask = 0;
	properties->deviceLUIDValid = VK_FALSE;
}

void PhysicalDevice::getProperties(VkPhysicalDeviceMaintenance3Properties* properties) const
{
	properties->maxMemoryAllocationSize = 1 << 31;
	properties->maxPerSetDescriptors = 1024;
}

void PhysicalDevice::getProperties(VkPhysicalDeviceMultiviewProperties* properties) const
{
	properties->maxMultiviewViewCount = 0;
	properties->maxMultiviewInstanceIndex = 0;
}

void PhysicalDevice::getProperties(VkPhysicalDevicePointClippingProperties* properties) const
{
	properties->pointClippingBehavior = VK_POINT_CLIPPING_BEHAVIOR_ALL_CLIP_PLANES;
}

void PhysicalDevice::getProperties(VkPhysicalDeviceProtectedMemoryProperties* properties) const
{
	properties->protectedNoFault = VK_FALSE;
}

void PhysicalDevice::getProperties(VkPhysicalDeviceSubgroupProperties* properties) const
{
	properties->subgroupSize = 1;
	properties->supportedStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
	properties->supportedOperations = VK_SUBGROUP_FEATURE_BASIC_BIT;
	properties->quadOperationsInAllStages = VK_FALSE;
}

bool PhysicalDevice::hasFeatures(const VkPhysicalDeviceFeatures& requestedFeatures) const
{
	const VkPhysicalDeviceFeatures& supportedFeatures = getFeatures();
	const VkBool32* supportedFeature = reinterpret_cast<const VkBool32*>(&supportedFeatures);
	const VkBool32* requestedFeature = reinterpret_cast<const VkBool32*>(&requestedFeatures);
	constexpr auto featureCount = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);

	for(unsigned int i = 0; i < featureCount; i++)
	{
		if((requestedFeature[i] == VK_TRUE) && (supportedFeature[i] != VK_TRUE))
		{
			return false;
		}
	}

	return true;
}

void PhysicalDevice::getFormatProperties(VkFormat format, VkFormatProperties* pFormatProperties) const
{
	pFormatProperties->linearTilingFeatures = 0; // Unsupported format
	pFormatProperties->optimalTilingFeatures = 0; // Unsupported format
	pFormatProperties->bufferFeatures = 0; // Unsupported format

	switch(format)
	{
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
	case VK_FORMAT_R5G6B5_UNORM_PACK16:
	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
	case VK_FORMAT_R8_UNORM:
	case VK_FORMAT_R8_SNORM:
	case VK_FORMAT_R8G8_UNORM:
	case VK_FORMAT_R8G8_SNORM:
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_SRGB:
	case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
	case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
	case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
	case VK_FORMAT_R16_SFLOAT:
	case VK_FORMAT_R16G16_SFLOAT:
	case VK_FORMAT_R16G16B16A16_SFLOAT:
	case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
	case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
		pFormatProperties->optimalTilingFeatures |=
			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
		// Fall through
	case VK_FORMAT_R8_UINT:
	case VK_FORMAT_R8_SINT:
	case VK_FORMAT_R8G8_UINT:
	case VK_FORMAT_R8G8_SINT:
	case VK_FORMAT_R8G8B8A8_UINT:
	case VK_FORMAT_R8G8B8A8_SINT:
	case VK_FORMAT_A8B8G8R8_UINT_PACK32:
	case VK_FORMAT_A8B8G8R8_SINT_PACK32:
	case VK_FORMAT_A2B10G10R10_UINT_PACK32:
	case VK_FORMAT_R16_UINT:
	case VK_FORMAT_R16_SINT:
	case VK_FORMAT_R16G16_UINT:
	case VK_FORMAT_R16G16_SINT:
	case VK_FORMAT_R16G16B16A16_UINT:
	case VK_FORMAT_R16G16B16A16_SINT:
	case VK_FORMAT_R32_UINT:
	case VK_FORMAT_R32_SINT:
	case VK_FORMAT_R32_SFLOAT:
	case VK_FORMAT_R32G32_UINT:
	case VK_FORMAT_R32G32_SINT:
	case VK_FORMAT_R32G32_SFLOAT:
	case VK_FORMAT_R32G32B32A32_UINT:
	case VK_FORMAT_R32G32B32A32_SINT:
	case VK_FORMAT_R32G32B32A32_SFLOAT:
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_D32_SFLOAT:
		pFormatProperties->optimalTilingFeatures |=
			VK_FORMAT_FEATURE_BLIT_SRC_BIT |
			VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
			VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
		// Fall through
	case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
	case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
	case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
	case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
	case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
	case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
	case VK_FORMAT_EAC_R11_UNORM_BLOCK:
	case VK_FORMAT_EAC_R11_SNORM_BLOCK:
	case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
	case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
		pFormatProperties->optimalTilingFeatures |=
			VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
		break;
	default:
		break;
	}

	switch(format)
	{
	case VK_FORMAT_R32_UINT:
	case VK_FORMAT_R32_SINT:
		pFormatProperties->optimalTilingFeatures |=
			VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT;
		pFormatProperties->bufferFeatures |=
			VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT;
		// Fall through
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SNORM:
	case VK_FORMAT_R8G8B8A8_UINT:
	case VK_FORMAT_R8G8B8A8_SINT:
	case VK_FORMAT_R16G16B16A16_UINT:
	case VK_FORMAT_R16G16B16A16_SINT:
	case VK_FORMAT_R16G16B16A16_SFLOAT:
	case VK_FORMAT_R32_SFLOAT:
	case VK_FORMAT_R32G32_UINT:
	case VK_FORMAT_R32G32_SINT:
	case VK_FORMAT_R32G32_SFLOAT:
	case VK_FORMAT_R32G32B32A32_UINT:
	case VK_FORMAT_R32G32B32A32_SINT:
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		pFormatProperties->optimalTilingFeatures |=
			VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
		// Fall through
	case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
	case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
	case VK_FORMAT_A8B8G8R8_UINT_PACK32:
	case VK_FORMAT_A8B8G8R8_SINT_PACK32:
		pFormatProperties->bufferFeatures |=
			VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT;
		break;
	default:
		break;
	}

	switch(format)
	{
	case VK_FORMAT_R5G6B5_UNORM_PACK16:
	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
	case VK_FORMAT_R8_UNORM:
	case VK_FORMAT_R8G8_UNORM:
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_SRGB:
	case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
	case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
	case VK_FORMAT_R16_SFLOAT:
	case VK_FORMAT_R16G16_SFLOAT:
	case VK_FORMAT_R16G16B16A16_SFLOAT:
		pFormatProperties->optimalTilingFeatures |=
			VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
		// Fall through
	case VK_FORMAT_R8_UINT:
	case VK_FORMAT_R8_SINT:
	case VK_FORMAT_R8G8_UINT:
	case VK_FORMAT_R8G8_SINT:
	case VK_FORMAT_R8G8B8A8_UINT:
	case VK_FORMAT_R8G8B8A8_SINT:
	case VK_FORMAT_A8B8G8R8_UINT_PACK32:
	case VK_FORMAT_A8B8G8R8_SINT_PACK32:
	case VK_FORMAT_A2B10G10R10_UINT_PACK32:
	case VK_FORMAT_R16_UINT:
	case VK_FORMAT_R16_SINT:
	case VK_FORMAT_R16G16_UINT:
	case VK_FORMAT_R16G16_SINT:
	case VK_FORMAT_R16G16B16A16_UINT:
	case VK_FORMAT_R16G16B16A16_SINT:
	case VK_FORMAT_R32_UINT:
	case VK_FORMAT_R32_SINT:
	case VK_FORMAT_R32_SFLOAT:
	case VK_FORMAT_R32G32_UINT:
	case VK_FORMAT_R32G32_SINT:
	case VK_FORMAT_R32G32_SFLOAT:
	case VK_FORMAT_R32G32B32A32_UINT:
	case VK_FORMAT_R32G32B32A32_SINT:
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		pFormatProperties->optimalTilingFeatures |=
			VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
			VK_FORMAT_FEATURE_BLIT_DST_BIT;
		break;
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_D32_SFLOAT: // Note: either VK_FORMAT_D32_SFLOAT or VK_FORMAT_X8_D24_UNORM_PACK32 must be supported
	case VK_FORMAT_D32_SFLOAT_S8_UINT: // Note: either VK_FORMAT_D24_UNORM_S8_UINT or VK_FORMAT_D32_SFLOAT_S8_UINT must be supported
		pFormatProperties->optimalTilingFeatures |=
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
		break;
	default:
		break;
	}

	switch(format)
	{
	case VK_FORMAT_R8_UNORM:
	case VK_FORMAT_R8_SNORM:
	case VK_FORMAT_R8_UINT:
	case VK_FORMAT_R8_SINT:
	case VK_FORMAT_R8G8_UNORM:
	case VK_FORMAT_R8G8_SNORM:
	case VK_FORMAT_R8G8_UINT:
	case VK_FORMAT_R8G8_SINT:
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SNORM:
	case VK_FORMAT_R8G8B8A8_UINT:
	case VK_FORMAT_R8G8B8A8_SINT:
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
	case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
	case VK_FORMAT_A8B8G8R8_UINT_PACK32:
	case VK_FORMAT_A8B8G8R8_SINT_PACK32:
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
	case VK_FORMAT_R16_UNORM:
	case VK_FORMAT_R16_SNORM:
	case VK_FORMAT_R16_UINT:
	case VK_FORMAT_R16_SINT:
	case VK_FORMAT_R16_SFLOAT:
	case VK_FORMAT_R16G16_UNORM:
	case VK_FORMAT_R16G16_SNORM:
	case VK_FORMAT_R16G16_UINT:
	case VK_FORMAT_R16G16_SINT:
	case VK_FORMAT_R16G16_SFLOAT:
	case VK_FORMAT_R16G16B16A16_UNORM:
	case VK_FORMAT_R16G16B16A16_SNORM:
	case VK_FORMAT_R16G16B16A16_UINT:
	case VK_FORMAT_R16G16B16A16_SINT:
	case VK_FORMAT_R16G16B16A16_SFLOAT:
	case VK_FORMAT_R32_UINT:
	case VK_FORMAT_R32_SINT:
	case VK_FORMAT_R32_SFLOAT:
	case VK_FORMAT_R32G32_UINT:
	case VK_FORMAT_R32G32_SINT:
	case VK_FORMAT_R32G32_SFLOAT:
	case VK_FORMAT_R32G32B32_UINT:
	case VK_FORMAT_R32G32B32_SINT:
	case VK_FORMAT_R32G32B32_SFLOAT:
	case VK_FORMAT_R32G32B32A32_UINT:
	case VK_FORMAT_R32G32B32A32_SINT:
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		pFormatProperties->bufferFeatures |=
			VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;
		break;
	default:
		break;
	}

	switch(format)
	{
	case VK_FORMAT_R8_UNORM:
	case VK_FORMAT_R8_SNORM:
	case VK_FORMAT_R8_UINT:
	case VK_FORMAT_R8_SINT:
	case VK_FORMAT_R8G8_UNORM:
	case VK_FORMAT_R8G8_SNORM:
	case VK_FORMAT_R8G8_UINT:
	case VK_FORMAT_R8G8_SINT:
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SNORM:
	case VK_FORMAT_R8G8B8A8_UINT:
	case VK_FORMAT_R8G8B8A8_SINT:
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
	case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
	case VK_FORMAT_A8B8G8R8_UINT_PACK32:
	case VK_FORMAT_A8B8G8R8_SINT_PACK32:
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
	case VK_FORMAT_A2B10G10R10_UINT_PACK32:
	case VK_FORMAT_R16_UINT:
	case VK_FORMAT_R16_SINT:
	case VK_FORMAT_R16_SFLOAT:
	case VK_FORMAT_R16G16_UINT:
	case VK_FORMAT_R16G16_SINT:
	case VK_FORMAT_R16G16_SFLOAT:
	case VK_FORMAT_R16G16B16A16_UINT:
	case VK_FORMAT_R16G16B16A16_SINT:
	case VK_FORMAT_R16G16B16A16_SFLOAT:
	case VK_FORMAT_R32_UINT:
	case VK_FORMAT_R32_SINT:
	case VK_FORMAT_R32_SFLOAT:
	case VK_FORMAT_R32G32_UINT:
	case VK_FORMAT_R32G32_SINT:
	case VK_FORMAT_R32G32_SFLOAT:
	case VK_FORMAT_R32G32B32A32_UINT:
	case VK_FORMAT_R32G32B32A32_SINT:
	case VK_FORMAT_R32G32B32A32_SFLOAT:
	case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
		pFormatProperties->bufferFeatures |=
			VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT;
		break;
	default:
		break;
	}
}

void PhysicalDevice::getImageFormatProperties(VkFormat format, VkImageType type, VkImageTiling tiling,
                                              VkImageUsageFlags usage, VkImageCreateFlags flags,
	                                          VkImageFormatProperties* pImageFormatProperties) const
{
	pImageFormatProperties->maxArrayLayers = 1 << vk::MAX_IMAGE_ARRAY_LAYERS;

	switch(type)
	{
	case VK_IMAGE_TYPE_1D:
		pImageFormatProperties->maxMipLevels = vk::MAX_IMAGE_LEVELS_1D;
		pImageFormatProperties->maxExtent.width = 1 << vk::MAX_IMAGE_LEVELS_1D;
		pImageFormatProperties->maxExtent.height = 1;
		pImageFormatProperties->maxExtent.depth = 1;
		break;
	case VK_IMAGE_TYPE_2D:
		if(flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
		{
			pImageFormatProperties->maxMipLevels = vk::MAX_IMAGE_LEVELS_CUBE;
			pImageFormatProperties->maxExtent.width = 1 << vk::MAX_IMAGE_LEVELS_CUBE;
			pImageFormatProperties->maxExtent.height = 1 << vk::MAX_IMAGE_LEVELS_CUBE;
			pImageFormatProperties->maxExtent.depth = 1;
		}
		else
		{
			pImageFormatProperties->maxMipLevels = vk::MAX_IMAGE_LEVELS_2D;
			pImageFormatProperties->maxExtent.width = 1 << vk::MAX_IMAGE_LEVELS_2D;
			pImageFormatProperties->maxExtent.height = 1 << vk::MAX_IMAGE_LEVELS_2D;
			pImageFormatProperties->maxExtent.depth = 1;
		}
		break;
	case VK_IMAGE_TYPE_3D:
		pImageFormatProperties->maxMipLevels = vk::MAX_IMAGE_LEVELS_3D;
		pImageFormatProperties->maxExtent.width = 1 << vk::MAX_IMAGE_LEVELS_3D;
		pImageFormatProperties->maxExtent.height = 1 << vk::MAX_IMAGE_LEVELS_3D;
		pImageFormatProperties->maxExtent.depth = 1 << vk::MAX_IMAGE_LEVELS_3D;
		break;
	default:
		UNREACHABLE(type);
		break;
	}

	pImageFormatProperties->maxResourceSize = 1 << 31; // Minimum value for maxResourceSize
	pImageFormatProperties->sampleCounts = getSampleCounts();

}

uint32_t PhysicalDevice::getQueueFamilyPropertyCount() const
{
	return 1;
}

void PhysicalDevice::getQueueFamilyProperties(uint32_t pQueueFamilyPropertyCount,
                                              VkQueueFamilyProperties* pQueueFamilyProperties) const
{
	for(uint32_t i = 0; i < pQueueFamilyPropertyCount; i++)
	{
		pQueueFamilyProperties[i].minImageTransferGranularity.width = 1;
		pQueueFamilyProperties[i].minImageTransferGranularity.height = 1;
		pQueueFamilyProperties[i].minImageTransferGranularity.depth = 1;
		pQueueFamilyProperties[i].queueCount = 1;
		pQueueFamilyProperties[i].queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
		pQueueFamilyProperties[i].timestampValidBits = 0; // No support for time stamps
	}
}

const VkPhysicalDeviceMemoryProperties& PhysicalDevice::getMemoryProperties() const
{
	static const VkPhysicalDeviceMemoryProperties properties
	{
		1, // memoryTypeCount
		{
			// vk::MEMORY_TYPE_GENERIC_BIT
			{
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
				VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
				VK_MEMORY_PROPERTY_HOST_CACHED_BIT, // propertyFlags
				0 // heapIndex
			},
		},
		1, // memoryHeapCount
		{
			{
				1ull << 31, // size, FIXME(sugoi): This should be configurable based on available RAM
				VK_MEMORY_HEAP_DEVICE_LOCAL_BIT // flags
			},
		}
	};

	return properties;
}

} // namespace vk
