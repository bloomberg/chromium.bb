/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Google Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */ /*!
 * \file
 * \brief Subgroups Tests Utils
 */ /*--------------------------------------------------------------------*/

#include "vktSubgroupsTestsUtils.hpp"
#include "deRandom.hpp"
#include "tcuCommandLine.hpp"
#include "tcuStringTemplate.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{
deUint32 getFormatSizeInBytes(const VkFormat format)
{
	switch (format)
	{
		default:
			DE_FATAL("Unhandled format!");
			return 0;
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_UINT:
			return sizeof(deInt32);
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_UINT:
			return static_cast<deUint32>(sizeof(deInt32) * 2);
		case VK_FORMAT_R32G32B32_SINT:
		case VK_FORMAT_R32G32B32_UINT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_UINT:
			return static_cast<deUint32>(sizeof(deInt32) * 4);
		case VK_FORMAT_R32_SFLOAT:
			return 4;
		case VK_FORMAT_R32G32_SFLOAT:
			return 8;
		case VK_FORMAT_R32G32B32_SFLOAT:
			return 16;
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			return 16;
		case VK_FORMAT_R64_SFLOAT:
			return 8;
		case VK_FORMAT_R64G64_SFLOAT:
			return 16;
		case VK_FORMAT_R64G64B64_SFLOAT:
			return 32;
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return 32;
		// The below formats are used to represent bool and bvec* types. These
		// types are passed to the shader as int and ivec* types, before the
		// calculations are done as booleans. We need a distinct type here so
		// that the shader generators can switch on it and generate the correct
		// shader source for testing.
		case VK_FORMAT_R8_USCALED:
			return sizeof(deInt32);
		case VK_FORMAT_R8G8_USCALED:
			return static_cast<deUint32>(sizeof(deInt32) * 2);
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8A8_USCALED:
			return static_cast<deUint32>(sizeof(deInt32) * 4);
	}
}

deUint32 getElementSizeInBytes(
	const VkFormat format,
	const subgroups::SSBOData::InputDataLayoutType layout)
{
	deUint32 bytes = getFormatSizeInBytes(format);
	if (layout == subgroups::SSBOData::LayoutStd140)
		return bytes < 16 ? 16 : bytes;
	else
		return bytes;
}

Move<VkPipelineLayout> makePipelineLayout(
	Context& context, const VkDescriptorSetLayout descriptorSetLayout)
{
	const vk::VkPipelineLayoutCreateInfo pipelineLayoutParams = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
		DE_NULL,			  // const void*            pNext;
		0u,					  // VkPipelineLayoutCreateFlags    flags;
		1u,					  // deUint32             setLayoutCount;
		&descriptorSetLayout, // const VkDescriptorSetLayout*   pSetLayouts;
		0u,					  // deUint32             pushConstantRangeCount;
		DE_NULL, // const VkPushConstantRange*   pPushConstantRanges;
	};
	return createPipelineLayout(context.getDeviceInterface(),
								context.getDevice(), &pipelineLayoutParams);
}

Move<VkRenderPass> makeRenderPass(Context& context, VkFormat format)
{
	VkAttachmentReference colorReference = {
		0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	const VkSubpassDescription subpassDescription = {0u,
													 VK_PIPELINE_BIND_POINT_GRAPHICS, 0, DE_NULL, 1, &colorReference,
													 DE_NULL, DE_NULL, 0, DE_NULL
													};

	const VkSubpassDependency subpassDependencies[2] = {
		{   VK_SUBPASS_EXTERNAL, 0u, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_DEPENDENCY_BY_REGION_BIT
		},
		{   0u, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_MEMORY_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT
		},
	};

	VkAttachmentDescription attachmentDescription = {0u, format,
													 VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
													 VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
													 VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
													 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
													};

	const VkRenderPassCreateInfo renderPassCreateInfo = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, DE_NULL, 0u, 1,
		&attachmentDescription, 1, &subpassDescription, 2, subpassDependencies
	};

	return createRenderPass(context.getDeviceInterface(), context.getDevice(),
							&renderPassCreateInfo);
}

Move<VkFramebuffer> makeFramebuffer(Context& context,
									const VkRenderPass renderPass, const VkImageView imageView, deUint32 width,
									deUint32 height)
{
	const VkFramebufferCreateInfo framebufferCreateInfo = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, DE_NULL, 0u, renderPass, 1,
		&imageView, width, height, 1
	};

	return createFramebuffer(context.getDeviceInterface(), context.getDevice(),
							 &framebufferCreateInfo);
}

Move<VkPipeline> makeGraphicsPipeline(Context&									context,
									  const VkPipelineLayout					pipelineLayout,
									  const VkShaderStageFlags					stages,
									  const VkShaderModule						vertexShaderModule,
									  const VkShaderModule						fragmentShaderModule,
									  const VkShaderModule						geometryShaderModule,
									  const VkShaderModule						tessellationControlModule,
									  const VkShaderModule						tessellationEvaluationModule,
									  const VkRenderPass						renderPass,
									  const VkPrimitiveTopology					topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
									  const VkVertexInputBindingDescription*	vertexInputBindingDescription = DE_NULL,
									  const VkVertexInputAttributeDescription*	vertexInputAttributeDescriptions = DE_NULL,
									  const bool								frameBufferTests = false,
									  const vk::VkFormat						attachmentFormat = VK_FORMAT_R32G32B32A32_SFLOAT)
{
	std::vector<VkViewport>	noViewports;
	std::vector<VkRect2D>	noScissors;

	const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		0u,															// VkPipelineVertexInputStateCreateFlags		flags;
		vertexInputBindingDescription == DE_NULL ? 0u : 1u,			// deUint32										vertexBindingDescriptionCount;
		vertexInputBindingDescription,								// const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		vertexInputAttributeDescriptions == DE_NULL ? 0u : 1u,		// deUint32										vertexAttributeDescriptionCount;
		vertexInputAttributeDescriptions,							// const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions;
	};

	const deUint32 numChannels = getNumUsedChannels(mapVkFormat(attachmentFormat).order);
	const VkColorComponentFlags colorComponent =
												numChannels == 1 ? VK_COLOR_COMPONENT_R_BIT :
												numChannels == 2 ? VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT :
												numChannels == 3 ? VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT :
												VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
	{
		VK_FALSE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
		colorComponent
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, DE_NULL, 0u,
		VK_FALSE, VK_LOGIC_OP_CLEAR, 1, &colorBlendAttachmentState,
		{ 0.0f, 0.0f, 0.0f, 0.0f }
	};

	const deUint32 patchControlPoints = (VK_SHADER_STAGE_FRAGMENT_BIT & stages && frameBufferTests) ? 2u : 1u;

	return vk::makeGraphicsPipeline(context.getDeviceInterface(),	// const DeviceInterface&                        vk
									context.getDevice(),			// const VkDevice                                device
									pipelineLayout,					// const VkPipelineLayout                        pipelineLayout
									vertexShaderModule,				// const VkShaderModule                          vertexShaderModule
									tessellationControlModule,		// const VkShaderModule                          tessellationControlShaderModule
									tessellationEvaluationModule,	// const VkShaderModule                          tessellationEvalShaderModule
									geometryShaderModule,			// const VkShaderModule                          geometryShaderModule
									fragmentShaderModule,			// const VkShaderModule                          fragmentShaderModule
									renderPass,						// const VkRenderPass                            renderPass
									noViewports,					// const std::vector<VkViewport>&                viewports
									noScissors,						// const std::vector<VkRect2D>&                  scissors
									topology,						// const VkPrimitiveTopology                     topology
									0u,								// const deUint32                                subpass
									patchControlPoints,				// const deUint32                                patchControlPoints
									&vertexInputStateCreateInfo,	// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
									DE_NULL,						// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
									DE_NULL,						// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
									DE_NULL,						// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
									&colorBlendStateCreateInfo);	// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
}

Move<VkPipeline> makeComputePipeline(Context& context,
									 const VkPipelineLayout pipelineLayout, const VkShaderModule shaderModule,
									 deUint32 localSizeX, deUint32 localSizeY, deUint32 localSizeZ)
{
	const deUint32 localSize[3] = {localSizeX, localSizeY, localSizeZ};

	const vk::VkSpecializationMapEntry entries[3] =
	{
		{0, sizeof(deUint32) * 0, sizeof(deUint32)},
		{1, sizeof(deUint32) * 1, sizeof(deUint32)},
		{2, static_cast<deUint32>(sizeof(deUint32) * 2), sizeof(deUint32)},
	};

	const vk::VkSpecializationInfo info =
	{
		/* mapEntryCount = */ 3,
		/* pMapEntries   = */ entries,
		/* dataSize      = */ sizeof(localSize),
		/* pData         = */ localSize
	};

	const vk::VkPipelineShaderStageCreateInfo pipelineShaderStageParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		0u,														// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits			stage;
		shaderModule,											// VkShaderModule					module;
		"main",													// const char*						pName;
		&info,													// const VkSpecializationInfo*		pSpecializationInfo;
	};

	const vk::VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType	sType;
		DE_NULL,										// const void*						pNext;
		0u,												// VkPipelineCreateFlags			flags;
		pipelineShaderStageParams,						// VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout,									// VkPipelineLayout					layout;
		DE_NULL,										// VkPipeline						basePipelineHandle;
		0,												// deInt32							basePipelineIndex;
	};

	return createComputePipeline(context.getDeviceInterface(),
								 context.getDevice(), DE_NULL, &pipelineCreateInfo);
}

Move<VkCommandPool> makeCommandPool(Context& context)
{
	const VkCommandPoolCreateInfo commandPoolParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // VkStructureType sType;
		DE_NULL,									// const void*        pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // VkCommandPoolCreateFlags
		// flags;
		context.getUniversalQueueFamilyIndex(), // deUint32 queueFamilyIndex;
	};

	return createCommandPool(
			   context.getDeviceInterface(), context.getDevice(), &commandPoolParams);
}

Move<VkCommandBuffer> makeCommandBuffer(
	Context& context, const VkCommandPool commandPool)
{
	const VkCommandBufferAllocateInfo bufferAllocateParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType		sType;
		DE_NULL,										// const void*			pNext;
		commandPool,									// VkCommandPool		commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel	level;
		1u,												// deUint32				bufferCount;
	};
	return allocateCommandBuffer(context.getDeviceInterface(),
								 context.getDevice(), &bufferAllocateParams);
}

struct Buffer;
struct Image;

struct BufferOrImage
{
	bool isImage() const
	{
		return m_isImage;
	}

	Buffer* getAsBuffer()
	{
		if (m_isImage) DE_FATAL("Trying to get a buffer as an image!");
		return reinterpret_cast<Buffer* >(this);
	}

	Image* getAsImage()
	{
		if (!m_isImage) DE_FATAL("Trying to get an image as a buffer!");
		return reinterpret_cast<Image*>(this);
	}

	virtual VkDescriptorType getType() const
	{
		if (m_isImage)
		{
			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		}
		else
		{
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		}
	}

	Allocation& getAllocation() const
	{
		return *m_allocation;
	}

	virtual ~BufferOrImage() {}

protected:
	explicit BufferOrImage(bool image) : m_isImage(image) {}

	bool m_isImage;
	de::details::MovePtr<Allocation> m_allocation;
};

struct Buffer : public BufferOrImage
{
	explicit Buffer(
		Context& context, VkDeviceSize sizeInBytes, VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		: BufferOrImage		(false)
		, m_sizeInBytes		(sizeInBytes)
		, m_usage			(usage)
	{
		const DeviceInterface&			vkd					= context.getDeviceInterface();
		const VkDevice					device				= context.getDevice();

		const vk::VkBufferCreateInfo	bufferCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			DE_NULL,
			0u,
			m_sizeInBytes,
			m_usage,
			VK_SHARING_MODE_EXCLUSIVE,
			0u,
			DE_NULL,
		};
		m_buffer		= createBuffer(vkd, device, &bufferCreateInfo);

		VkMemoryRequirements			req					= getBufferMemoryRequirements(vkd, device, *m_buffer);

		m_allocation	= context.getDefaultAllocator().allocate(req, MemoryRequirement::HostVisible);
		VK_CHECK(vkd.bindBufferMemory(device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
	}

	virtual VkDescriptorType getType() const
	{
		if (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT == m_usage)
		{
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		}
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	}

	VkBuffer getBuffer () const
	{
		return *m_buffer;
	}

	const VkBuffer* getBufferPtr () const
	{
		return &(*m_buffer);
	}

	VkDeviceSize getSize () const
	{
		return m_sizeInBytes;
	}

private:
	Move<VkBuffer>				m_buffer;
	VkDeviceSize				m_sizeInBytes;
	const VkBufferUsageFlags	m_usage;
};

struct Image : public BufferOrImage
{
	explicit Image(Context& context, deUint32 width, deUint32 height,
				   VkFormat format, VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT)
		: BufferOrImage(true)
	{
		const DeviceInterface&			vkd					= context.getDeviceInterface();
		const VkDevice					device				= context.getDevice();

		const VkImageCreateInfo			imageCreateInfo		=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, DE_NULL, 0, VK_IMAGE_TYPE_2D,
			format, {width, height, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_TILING_OPTIMAL, usage,
			VK_SHARING_MODE_EXCLUSIVE, 0u, DE_NULL,
			VK_IMAGE_LAYOUT_UNDEFINED
		};

		const VkComponentMapping		componentMapping	=
		{
			VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
		};

		const VkImageSubresourceRange	subresourceRange	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	//VkImageAspectFlags	aspectMask
			0u,							//deUint32				baseMipLevel
			1u,							//deUint32				levelCount
			0u,							//deUint32				baseArrayLayer
			1u							//deUint32				layerCount
		};

		const VkSamplerCreateInfo		samplerCreateInfo	=
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			DE_NULL,
			0u,
			VK_FILTER_NEAREST,
			VK_FILTER_NEAREST,
			VK_SAMPLER_MIPMAP_MODE_NEAREST,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			0.0f,
			VK_FALSE,
			1.0f,
			DE_FALSE,
			VK_COMPARE_OP_ALWAYS,
			0.0f,
			0.0f,
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			VK_FALSE,
		};

		m_image			= createImage(vkd, device, &imageCreateInfo);

		VkMemoryRequirements			req					= getImageMemoryRequirements(vkd, device, *m_image);

		req.size		*= 2;
		m_allocation	= context.getDefaultAllocator().allocate(req, MemoryRequirement::Any);

		VK_CHECK(vkd.bindImageMemory(device, *m_image, m_allocation->getMemory(), m_allocation->getOffset()));

		const VkImageViewCreateInfo		imageViewCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, DE_NULL, 0, *m_image,
			VK_IMAGE_VIEW_TYPE_2D, imageCreateInfo.format, componentMapping,
			subresourceRange
		};

		m_imageView		= createImageView(vkd, device, &imageViewCreateInfo);
		m_sampler		= createSampler(vkd, device, &samplerCreateInfo);

		// Transition input image layouts
		{
			const Unique<VkCommandPool>		cmdPool				(makeCommandPool(context));
			const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(context, *cmdPool));

			beginCommandBuffer(vkd, *cmdBuffer);

			const VkImageMemoryBarrier		imageBarrier		= makeImageMemoryBarrier((VkAccessFlags)0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																	VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, *m_image, subresourceRange);

			vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				(VkDependencyFlags)0, 0u, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &imageBarrier);

			endCommandBuffer(vkd, *cmdBuffer);
			submitCommandsAndWait(vkd, device, context.getUniversalQueue(), *cmdBuffer);
		}
	}

	VkImage getImage () const
	{
		return *m_image;
	}

	VkImageView getImageView () const
	{
		return *m_imageView;
	}

	VkSampler getSampler () const
	{
		return *m_sampler;
	}

private:
	Move<VkImage> m_image;
	Move<VkImageView> m_imageView;
	Move<VkSampler> m_sampler;
};
}

std::string vkt::subgroups::getSharedMemoryBallotHelper()
{
	return	"shared uvec4 superSecretComputeShaderHelper[gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z];\n"
			"uvec4 sharedMemoryBallot(bool vote)\n"
			"{\n"
			"  uint groupOffset = gl_SubgroupID;\n"
			"  // One invocation in the group 0's the whole group's data\n"
			"  if (subgroupElect())\n"
			"  {\n"
			"    superSecretComputeShaderHelper[groupOffset] = uvec4(0);\n"
			"  }\n"
			"  subgroupMemoryBarrierShared();\n"
			"  if (vote)\n"
			"  {\n"
			"    const highp uint invocationId = gl_SubgroupInvocationID % 32;\n"
			"    const highp uint bitToSet = 1u << invocationId;\n"
			"    switch (gl_SubgroupInvocationID / 32)\n"
			"    {\n"
			"    case 0: atomicOr(superSecretComputeShaderHelper[groupOffset].x, bitToSet); break;\n"
			"    case 1: atomicOr(superSecretComputeShaderHelper[groupOffset].y, bitToSet); break;\n"
			"    case 2: atomicOr(superSecretComputeShaderHelper[groupOffset].z, bitToSet); break;\n"
			"    case 3: atomicOr(superSecretComputeShaderHelper[groupOffset].w, bitToSet); break;\n"
			"    }\n"
			"  }\n"
			"  subgroupMemoryBarrierShared();\n"
			"  return superSecretComputeShaderHelper[groupOffset];\n"
			"}\n";
}

deUint32 vkt::subgroups::getSubgroupSize(Context& context)
{
	VkPhysicalDeviceSubgroupProperties subgroupProperties;
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
	subgroupProperties.pNext = DE_NULL;

	VkPhysicalDeviceProperties2 properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &subgroupProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

	return subgroupProperties.subgroupSize;
}

VkDeviceSize vkt::subgroups::maxSupportedSubgroupSize() {
	return 128u;
}

std::string vkt::subgroups::getShaderStageName(VkShaderStageFlags stage)
{
	switch (stage)
	{
		default:
			DE_FATAL("Unhandled stage!");
			return "";
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return "compute";
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return "fragment";
		case VK_SHADER_STAGE_VERTEX_BIT:
			return "vertex";
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return "geometry";
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return "tess_control";
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return "tess_eval";
	}
}

std::string vkt::subgroups::getSubgroupFeatureName(vk::VkSubgroupFeatureFlagBits bit)
{
	switch (bit)
	{
		default:
			DE_FATAL("Unknown subgroup feature category!");
			return "";
		case VK_SUBGROUP_FEATURE_BASIC_BIT:
			return "VK_SUBGROUP_FEATURE_BASIC_BIT";
		case VK_SUBGROUP_FEATURE_VOTE_BIT:
			return "VK_SUBGROUP_FEATURE_VOTE_BIT";
		case VK_SUBGROUP_FEATURE_ARITHMETIC_BIT:
			return "VK_SUBGROUP_FEATURE_ARITHMETIC_BIT";
		case VK_SUBGROUP_FEATURE_BALLOT_BIT:
			return "VK_SUBGROUP_FEATURE_BALLOT_BIT";
		case VK_SUBGROUP_FEATURE_SHUFFLE_BIT:
			return "VK_SUBGROUP_FEATURE_SHUFFLE_BIT";
		case VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT:
			return "VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT";
		case VK_SUBGROUP_FEATURE_CLUSTERED_BIT:
			return "VK_SUBGROUP_FEATURE_CLUSTERED_BIT";
		case VK_SUBGROUP_FEATURE_QUAD_BIT:
			return "VK_SUBGROUP_FEATURE_QUAD_BIT";
	}
}

void vkt::subgroups::addNoSubgroupShader (SourceCollections& programCollection)
{
	{
	/*
		"#version 450\n"
		"void main (void)\n"
		"{\n"
		"  float pixelSize = 2.0f/1024.0f;\n"
		"   float pixelPosition = pixelSize/2.0f - 1.0f;\n"
		"  gl_Position = vec4(float(gl_VertexIndex) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
		"  gl_PointSize = 1.0f;\n"
		"}\n"
	*/
		const std::string vertNoSubgroup =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 1\n"
			"; Bound: 37\n"
			"; Schema: 0\n"
			"OpCapability Shader\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Vertex %4 \"main\" %22 %26\n"
			"OpMemberDecorate %20 0 BuiltIn Position\n"
			"OpMemberDecorate %20 1 BuiltIn PointSize\n"
			"OpMemberDecorate %20 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %20 3 BuiltIn CullDistance\n"
			"OpDecorate %20 Block\n"
			"OpDecorate %26 BuiltIn VertexIndex\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeFloat 32\n"
			"%7 = OpTypePointer Function %6\n"
			"%9 = OpConstant %6 0.00195313\n"
			"%12 = OpConstant %6 2\n"
			"%14 = OpConstant %6 1\n"
			"%16 = OpTypeVector %6 4\n"
			"%17 = OpTypeInt 32 0\n"
			"%18 = OpConstant %17 1\n"
			"%19 = OpTypeArray %6 %18\n"
			"%20 = OpTypeStruct %16 %6 %19 %19\n"
			"%21 = OpTypePointer Output %20\n"
			"%22 = OpVariable %21 Output\n"
			"%23 = OpTypeInt 32 1\n"
			"%24 = OpConstant %23 0\n"
			"%25 = OpTypePointer Input %23\n"
			"%26 = OpVariable %25 Input\n"
			"%33 = OpConstant %6 0\n"
			"%35 = OpTypePointer Output %16\n"
			"%37 = OpConstant %23 1\n"
			"%38 = OpTypePointer Output %6\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%8 = OpVariable %7 Function\n"
			"%10 = OpVariable %7 Function\n"
			"OpStore %8 %9\n"
			"%11 = OpLoad %6 %8\n"
			"%13 = OpFDiv %6 %11 %12\n"
			"%15 = OpFSub %6 %13 %14\n"
			"OpStore %10 %15\n"
			"%27 = OpLoad %23 %26\n"
			"%28 = OpConvertSToF %6 %27\n"
			"%29 = OpLoad %6 %8\n"
			"%30 = OpFMul %6 %28 %29\n"
			"%31 = OpLoad %6 %10\n"
			"%32 = OpFAdd %6 %30 %31\n"
			"%34 = OpCompositeConstruct %16 %32 %33 %33 %14\n"
			"%36 = OpAccessChain %35 %22 %24\n"
			"OpStore %36 %34\n"
			"%39 = OpAccessChain %38 %22 %37\n"
			"OpStore %39 %14\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("vert_noSubgroup") << vertNoSubgroup;
	}

	{
	/*
		"#version 450\n"
		"layout(vertices=1) out;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"  if (gl_InvocationID == 0)\n"
		"  {\n"
		"    gl_TessLevelOuter[0] = 1.0f;\n"
		"    gl_TessLevelOuter[1] = 1.0f;\n"
		"  }\n"
		"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		"}\n"
	*/
		const std::string tescNoSubgroup =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 1\n"
			"; Bound: 45\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationControl %4 \"main\" %8 %20 %32 %38\n"
			"OpExecutionMode %4 OutputVertices 1\n"
			"OpDecorate %8 BuiltIn InvocationId\n"
			"OpDecorate %20 Patch\n"
			"OpDecorate %20 BuiltIn TessLevelOuter\n"
			"OpMemberDecorate %29 0 BuiltIn Position\n"
			"OpMemberDecorate %29 1 BuiltIn PointSize\n"
			"OpMemberDecorate %29 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %29 3 BuiltIn CullDistance\n"
			"OpDecorate %29 Block\n"
			"OpMemberDecorate %34 0 BuiltIn Position\n"
			"OpMemberDecorate %34 1 BuiltIn PointSize\n"
			"OpMemberDecorate %34 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %34 3 BuiltIn CullDistance\n"
			"OpDecorate %34 Block\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 1\n"
			"%7 = OpTypePointer Input %6\n"
			"%8 = OpVariable %7 Input\n"
			"%10 = OpConstant %6 0\n"
			"%11 = OpTypeBool\n"
			"%15 = OpTypeFloat 32\n"
			"%16 = OpTypeInt 32 0\n"
			"%17 = OpConstant %16 4\n"
			"%18 = OpTypeArray %15 %17\n"
			"%19 = OpTypePointer Output %18\n"
			"%20 = OpVariable %19 Output\n"
			"%21 = OpConstant %15 1\n"
			"%22 = OpTypePointer Output %15\n"
			"%24 = OpConstant %6 1\n"
			"%26 = OpTypeVector %15 4\n"
			"%27 = OpConstant %16 1\n"
			"%28 = OpTypeArray %15 %27\n"
			"%29 = OpTypeStruct %26 %15 %28 %28\n"
			"%30 = OpTypeArray %29 %27\n"
			"%31 = OpTypePointer Output %30\n"
			"%32 = OpVariable %31 Output\n"
			"%34 = OpTypeStruct %26 %15 %28 %28\n"
			"%35 = OpConstant %16 32\n"
			"%36 = OpTypeArray %34 %35\n"
			"%37 = OpTypePointer Input %36\n"
			"%38 = OpVariable %37 Input\n"
			"%40 = OpTypePointer Input %26\n"
			"%43 = OpTypePointer Output %26\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%9 = OpLoad %6 %8\n"
			"%12 = OpIEqual %11 %9 %10\n"
			"OpSelectionMerge %14 None\n"
			"OpBranchConditional %12 %13 %14\n"
			"%13 = OpLabel\n"
			"%23 = OpAccessChain %22 %20 %10\n"
			"OpStore %23 %21\n"
			"%25 = OpAccessChain %22 %20 %24\n"
			"OpStore %25 %21\n"
			"OpBranch %14\n"
			"%14 = OpLabel\n"
			"%33 = OpLoad %6 %8\n"
			"%39 = OpLoad %6 %8\n"
			"%41 = OpAccessChain %40 %38 %39 %10\n"
			"%42 = OpLoad %26 %41\n"
			"%44 = OpAccessChain %43 %32 %33 %10\n"
			"OpStore %44 %42\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tesc_noSubgroup") << tescNoSubgroup;
	}

	{
	/*
		"#version 450\n"
		"layout(isolines) in;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"  float pixelSize = 2.0f/1024.0f;\n"
		"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
		"}\n";
	*/
		const std::string teseNoSubgroup =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 42\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationEvaluation %4 \"main\" %16 %23 %29\n"
			"OpExecutionMode %4 Isolines\n"
			"OpExecutionMode %4 SpacingEqual\n"
			"OpExecutionMode %4 VertexOrderCcw\n"
			"OpMemberDecorate %14 0 BuiltIn Position\n"
			"OpMemberDecorate %14 1 BuiltIn PointSize\n"
			"OpMemberDecorate %14 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %14 3 BuiltIn CullDistance\n"
			"OpDecorate %14 Block\n"
			"OpMemberDecorate %19 0 BuiltIn Position\n"
			"OpMemberDecorate %19 1 BuiltIn PointSize\n"
			"OpMemberDecorate %19 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %19 3 BuiltIn CullDistance\n"
			"OpDecorate %19 Block\n"
			"OpDecorate %29 BuiltIn TessCoord\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeFloat 32\n"
			"%7 = OpTypePointer Function %6\n"
			"%9 = OpConstant %6 0.00195313\n"
			"%10 = OpTypeVector %6 4\n"
			"%11 = OpTypeInt 32 0\n"
			"%12 = OpConstant %11 1\n"
			"%13 = OpTypeArray %6 %12\n"
			"%14 = OpTypeStruct %10 %6 %13 %13\n"
			"%15 = OpTypePointer Output %14\n"
			"%16 = OpVariable %15 Output\n"
			"%17 = OpTypeInt 32 1\n"
			"%18 = OpConstant %17 0\n"
			"%19 = OpTypeStruct %10 %6 %13 %13\n"
			"%20 = OpConstant %11 32\n"
			"%21 = OpTypeArray %19 %20\n"
			"%22 = OpTypePointer Input %21\n"
			"%23 = OpVariable %22 Input\n"
			"%24 = OpTypePointer Input %10\n"
			"%27 = OpTypeVector %6 3\n"
			"%28 = OpTypePointer Input %27\n"
			"%29 = OpVariable %28 Input\n"
			"%30 = OpConstant %11 0\n"
			"%31 = OpTypePointer Input %6\n"
			"%36 = OpConstant %6 2\n"
			"%40 = OpTypePointer Output %10\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%8 = OpVariable %7 Function\n"
			"OpStore %8 %9\n"
			"%25 = OpAccessChain %24 %23 %18 %18\n"
			"%26 = OpLoad %10 %25\n"
			"%32 = OpAccessChain %31 %29 %30\n"
			"%33 = OpLoad %6 %32\n"
			"%34 = OpLoad %6 %8\n"
			"%35 = OpFMul %6 %33 %34\n"
			"%37 = OpFDiv %6 %35 %36\n"
			"%38 = OpCompositeConstruct %10 %37 %37 %37 %37\n"
			"%39 = OpFAdd %10 %26 %38\n"
			"%41 = OpAccessChain %40 %16 %18\n"
			"OpStore %41 %39\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tese_noSubgroup") << teseNoSubgroup;
	}

}


std::string vkt::subgroups::getVertShaderForStage(vk::VkShaderStageFlags stage)
{
	switch (stage)
	{
		default:
			DE_FATAL("Unhandled stage!");
			return "";
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return
				"#version 450\n"
				"void main (void)\n"
				"{\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"   float pixelPosition = pixelSize/2.0f - 1.0f;\n"
				"  gl_Position = vec4(float(gl_VertexIndex) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
				"}\n";
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return
				"#version 450\n"
				"void main (void)\n"
				"{\n"
				"}\n";
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return
				"#version 450\n"
				"void main (void)\n"
				"{\n"
				"}\n";
	}
}

bool vkt::subgroups::isSubgroupSupported(Context& context)
{
	return context.contextSupports(vk::ApiVersion(1, 1, 0));
}

bool vkt::subgroups::areSubgroupOperationsSupportedForStage(
	Context& context, const VkShaderStageFlags stage)
{
	VkPhysicalDeviceSubgroupProperties subgroupProperties;
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
	subgroupProperties.pNext = DE_NULL;

	VkPhysicalDeviceProperties2 properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &subgroupProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

	return (stage & subgroupProperties.supportedStages) ? true : false;
}

bool vkt::subgroups::areSubgroupOperationsRequiredForStage(
	VkShaderStageFlags stage)
{
	switch (stage)
	{
		default:
			return false;
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return true;
	}
}

bool vkt::subgroups::isSubgroupFeatureSupportedForDevice(
	Context& context,
	VkSubgroupFeatureFlagBits bit) {
	VkPhysicalDeviceSubgroupProperties subgroupProperties;
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
	subgroupProperties.pNext = DE_NULL;

	VkPhysicalDeviceProperties2 properties;
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &subgroupProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

	return (bit & subgroupProperties.supportedOperations) ? true : false;
}

bool vkt::subgroups::isFragmentSSBOSupportedForDevice(Context& context)
{
	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(
				context.getInstanceInterface(), context.getPhysicalDevice());
	return features.fragmentStoresAndAtomics ? true : false;
}

bool vkt::subgroups::isVertexSSBOSupportedForDevice(Context& context)
{
	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(
				context.getInstanceInterface(), context.getPhysicalDevice());
	return features.vertexPipelineStoresAndAtomics ? true : false;
}

bool vkt::subgroups::isDoubleSupportedForDevice(Context& context)
{
	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(
				context.getInstanceInterface(), context.getPhysicalDevice());
	return features.shaderFloat64 ? true : false;
}

bool vkt::subgroups::isTessellationAndGeometryPointSizeSupported (Context& context)
{
	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(
		context.getInstanceInterface(), context.getPhysicalDevice());
	return features.shaderTessellationAndGeometryPointSize ? true : false;
}

bool vkt::subgroups::isDoubleFormat(VkFormat format)
{
	switch (format)
	{
		default:
			return false;
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return true;
	}
}

std::string vkt::subgroups::getFormatNameForGLSL (VkFormat format)
{
	switch (format)
	{
		default:
			DE_FATAL("Unhandled format!");
			return "";
		case VK_FORMAT_R32_SINT:
			return "int";
		case VK_FORMAT_R32G32_SINT:
			return "ivec2";
		case VK_FORMAT_R32G32B32_SINT:
			return "ivec3";
		case VK_FORMAT_R32G32B32A32_SINT:
			return "ivec4";
		case VK_FORMAT_R32_UINT:
			return "uint";
		case VK_FORMAT_R32G32_UINT:
			return "uvec2";
		case VK_FORMAT_R32G32B32_UINT:
			return "uvec3";
		case VK_FORMAT_R32G32B32A32_UINT:
			return "uvec4";
		case VK_FORMAT_R32_SFLOAT:
			return "float";
		case VK_FORMAT_R32G32_SFLOAT:
			return "vec2";
		case VK_FORMAT_R32G32B32_SFLOAT:
			return "vec3";
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			return "vec4";
		case VK_FORMAT_R64_SFLOAT:
			return "double";
		case VK_FORMAT_R64G64_SFLOAT:
			return "dvec2";
		case VK_FORMAT_R64G64B64_SFLOAT:
			return "dvec3";
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return "dvec4";
		case VK_FORMAT_R8_USCALED:
			return "bool";
		case VK_FORMAT_R8G8_USCALED:
			return "bvec2";
		case VK_FORMAT_R8G8B8_USCALED:
			return "bvec3";
		case VK_FORMAT_R8G8B8A8_USCALED:
			return "bvec4";
	}
}

void vkt::subgroups::setVertexShaderFrameBuffer (SourceCollections& programCollection)
{
	/*
		"layout(location = 0) in highp vec4 in_position;\n"
		"void main (void)\n"
		"{\n"
		"  gl_Position = in_position;\n"
		"  gl_PointSize = 1.0f;\n"
		"}\n";
	*/
	programCollection.spirvAsmSources.add("vert") <<
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 7\n"
		"; Bound: 25\n"
		"; Schema: 0\n"
		"OpCapability Shader\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Vertex %4 \"main\" %13 %17\n"
		"OpMemberDecorate %11 0 BuiltIn Position\n"
		"OpMemberDecorate %11 1 BuiltIn PointSize\n"
		"OpMemberDecorate %11 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %11 3 BuiltIn CullDistance\n"
		"OpDecorate %11 Block\n"
		"OpDecorate %17 Location 0\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypeVector %6 4\n"
		"%8 = OpTypeInt 32 0\n"
		"%9 = OpConstant %8 1\n"
		"%10 = OpTypeArray %6 %9\n"
		"%11 = OpTypeStruct %7 %6 %10 %10\n"
		"%12 = OpTypePointer Output %11\n"
		"%13 = OpVariable %12 Output\n"
		"%14 = OpTypeInt 32 1\n"
		"%15 = OpConstant %14 0\n"
		"%16 = OpTypePointer Input %7\n"
		"%17 = OpVariable %16 Input\n"
		"%19 = OpTypePointer Output %7\n"
		"%21 = OpConstant %14 1\n"
		"%22 = OpConstant %6 1\n"
		"%23 = OpTypePointer Output %6\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%18 = OpLoad %7 %17\n"
		"%20 = OpAccessChain %19 %13 %15\n"
		"OpStore %20 %18\n"
		"%24 = OpAccessChain %23 %13 %21\n"
		"OpStore %24 %22\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
}

void vkt::subgroups::setFragmentShaderFrameBuffer (vk::SourceCollections& programCollection)
{
	/*
		"layout(location = 0) in float in_color;\n"
		"layout(location = 0) out uint out_color;\n"
		"void main()\n"
		{\n"
		"	out_color = uint(in_color);\n"
		"}\n";
	*/
	programCollection.spirvAsmSources.add("fragment") <<
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 14\n"
		"; Schema: 0\n"
		"OpCapability Shader\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Fragment %4 \"main\" %8 %11\n"
		"OpExecutionMode %4 OriginUpperLeft\n"
		"OpDecorate %8 Location 0\n"
		"OpDecorate %11 Location 0\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeInt 32 0\n"
		"%7 = OpTypePointer Output %6\n"
		"%8 = OpVariable %7 Output\n"
		"%9 = OpTypeFloat 32\n"
		"%10 = OpTypePointer Input %9\n"
		"%11 = OpVariable %10 Input\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%12 = OpLoad %9 %11\n"
		"%13 = OpConvertFToU %6 %12\n"
		"OpStore %8 %13\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
}

void vkt::subgroups::setTesCtrlShaderFrameBuffer (vk::SourceCollections& programCollection)
{
	/*
		"#extension GL_KHR_shader_subgroup_basic: enable\n"
		"#extension GL_EXT_tessellation_shader : require\n"
		"layout(vertices = 2) out;\n"
		"void main (void)\n"
		"{\n"
		"  if (gl_InvocationID == 0)\n"
		  {\n"
		"    gl_TessLevelOuter[0] = 1.0f;\n"
		"    gl_TessLevelOuter[1] = 1.0f;\n"
		"  }\n"
		"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		"}\n";
	*/
	programCollection.spirvAsmSources.add("tesc") <<
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 46\n"
		"; Schema: 0\n"
		"OpCapability Tessellation\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationControl %4 \"main\" %8 %20 %33 %39\n"
		"OpExecutionMode %4 OutputVertices 2\n"
		"OpDecorate %8 BuiltIn InvocationId\n"
		"OpDecorate %20 Patch\n"
		"OpDecorate %20 BuiltIn TessLevelOuter\n"
		"OpMemberDecorate %29 0 BuiltIn Position\n"
		"OpMemberDecorate %29 1 BuiltIn PointSize\n"
		"OpMemberDecorate %29 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %29 3 BuiltIn CullDistance\n"
		"OpDecorate %29 Block\n"
		"OpMemberDecorate %35 0 BuiltIn Position\n"
		"OpMemberDecorate %35 1 BuiltIn PointSize\n"
		"OpMemberDecorate %35 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %35 3 BuiltIn CullDistance\n"
		"OpDecorate %35 Block\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeInt 32 1\n"
		"%7 = OpTypePointer Input %6\n"
		"%8 = OpVariable %7 Input\n"
		"%10 = OpConstant %6 0\n"
		"%11 = OpTypeBool\n"
		"%15 = OpTypeFloat 32\n"
		"%16 = OpTypeInt 32 0\n"
		"%17 = OpConstant %16 4\n"
		"%18 = OpTypeArray %15 %17\n"
		"%19 = OpTypePointer Output %18\n"
		"%20 = OpVariable %19 Output\n"
		"%21 = OpConstant %15 1\n"
		"%22 = OpTypePointer Output %15\n"
		"%24 = OpConstant %6 1\n"
		"%26 = OpTypeVector %15 4\n"
		"%27 = OpConstant %16 1\n"
		"%28 = OpTypeArray %15 %27\n"
		"%29 = OpTypeStruct %26 %15 %28 %28\n"
		"%30 = OpConstant %16 2\n"
		"%31 = OpTypeArray %29 %30\n"
		"%32 = OpTypePointer Output %31\n"
		"%33 = OpVariable %32 Output\n"
		"%35 = OpTypeStruct %26 %15 %28 %28\n"
		"%36 = OpConstant %16 32\n"
		"%37 = OpTypeArray %35 %36\n"
		"%38 = OpTypePointer Input %37\n"
		"%39 = OpVariable %38 Input\n"
		"%41 = OpTypePointer Input %26\n"
		"%44 = OpTypePointer Output %26\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%9 = OpLoad %6 %8\n"
		"%12 = OpIEqual %11 %9 %10\n"
		"OpSelectionMerge %14 None\n"
		"OpBranchConditional %12 %13 %14\n"
		"%13 = OpLabel\n"
		"%23 = OpAccessChain %22 %20 %10\n"
		"OpStore %23 %21\n"
		"%25 = OpAccessChain %22 %20 %24\n"
		"OpStore %25 %21\n"
		"OpBranch %14\n"
		"%14 = OpLabel\n"
		"%34 = OpLoad %6 %8\n"
		"%40 = OpLoad %6 %8\n"
		"%42 = OpAccessChain %41 %39 %40 %10\n"
		"%43 = OpLoad %26 %42\n"
		"%45 = OpAccessChain %44 %33 %34 %10\n"
		"OpStore %45 %43\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
}

void vkt::subgroups::setTesEvalShaderFrameBuffer (vk::SourceCollections& programCollection)
{
	/*
		"#extension GL_KHR_shader_subgroup_ballot: enable\n"
		"#extension GL_EXT_tessellation_shader : require\n"
		"layout(isolines, equal_spacing, ccw ) in;\n"
		"layout(location = 0) in float in_color[];\n"
		"layout(location = 0) out float out_color;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
		"  out_color = in_color[0];\n"
		"}\n";
	*/
	programCollection.spirvAsmSources.add("tese") <<
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 45\n"
		"; Schema: 0\n"
		"OpCapability Tessellation\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationEvaluation %4 \"main\" %13 %20 %29 %39 %42\n"
		"OpExecutionMode %4 Isolines\n"
		"OpExecutionMode %4 SpacingEqual\n"
		"OpExecutionMode %4 VertexOrderCcw\n"
		"OpMemberDecorate %11 0 BuiltIn Position\n"
		"OpMemberDecorate %11 1 BuiltIn PointSize\n"
		"OpMemberDecorate %11 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %11 3 BuiltIn CullDistance\n"
		"OpDecorate %11 Block\n"
		"OpMemberDecorate %16 0 BuiltIn Position\n"
		"OpMemberDecorate %16 1 BuiltIn PointSize\n"
		"OpMemberDecorate %16 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %16 3 BuiltIn CullDistance\n"
		"OpDecorate %16 Block\n"
		"OpDecorate %29 BuiltIn TessCoord\n"
		"OpDecorate %39 Location 0\n"
		"OpDecorate %42 Location 0\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypeVector %6 4\n"
		"%8 = OpTypeInt 32 0\n"
		"%9 = OpConstant %8 1\n"
		"%10 = OpTypeArray %6 %9\n"
		"%11 = OpTypeStruct %7 %6 %10 %10\n"
		"%12 = OpTypePointer Output %11\n"
		"%13 = OpVariable %12 Output\n"
		"%14 = OpTypeInt 32 1\n"
		"%15 = OpConstant %14 0\n"
		"%16 = OpTypeStruct %7 %6 %10 %10\n"
		"%17 = OpConstant %8 32\n"
		"%18 = OpTypeArray %16 %17\n"
		"%19 = OpTypePointer Input %18\n"
		"%20 = OpVariable %19 Input\n"
		"%21 = OpTypePointer Input %7\n"
		"%24 = OpConstant %14 1\n"
		"%27 = OpTypeVector %6 3\n"
		"%28 = OpTypePointer Input %27\n"
		"%29 = OpVariable %28 Input\n"
		"%30 = OpConstant %8 0\n"
		"%31 = OpTypePointer Input %6\n"
		"%36 = OpTypePointer Output %7\n"
		"%38 = OpTypePointer Output %6\n"
		"%39 = OpVariable %38 Output\n"
		"%40 = OpTypeArray %6 %17\n"
		"%41 = OpTypePointer Input %40\n"
		"%42 = OpVariable %41 Input\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%22 = OpAccessChain %21 %20 %15 %15\n"
		"%23 = OpLoad %7 %22\n"
		"%25 = OpAccessChain %21 %20 %24 %15\n"
		"%26 = OpLoad %7 %25\n"
		"%32 = OpAccessChain %31 %29 %30\n"
		"%33 = OpLoad %6 %32\n"
		"%34 = OpCompositeConstruct %7 %33 %33 %33 %33\n"
		"%35 = OpExtInst %7 %1 FMix %23 %26 %34\n"
		"%37 = OpAccessChain %36 %13 %15\n"
		"OpStore %37 %35\n"
		"%43 = OpAccessChain %31 %42 %15\n"
		"%44 = OpLoad %6 %43\n"
		"OpStore %39 %44\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
}

void vkt::subgroups::addGeometryShadersFromTemplate (const std::string& glslTemplate, const vk::ShaderBuildOptions& options,  vk::GlslSourceCollection& collection)
{
	tcu::StringTemplate geometryTemplate(glslTemplate);

	map<string, string>		linesParams;
	linesParams.insert(pair<string, string>("TOPOLOGY", "lines"));

	map<string, string>		pointsParams;
	pointsParams.insert(pair<string, string>("TOPOLOGY", "points"));

	collection.add("geometry_lines")	<< glu::GeometrySource(geometryTemplate.specialize(linesParams))	<< options;
	collection.add("geometry_points")	<< glu::GeometrySource(geometryTemplate.specialize(pointsParams))	<< options;
}

void vkt::subgroups::addGeometryShadersFromTemplate (const std::string& spirvTemplate, const vk::SpirVAsmBuildOptions& options, vk::SpirVAsmCollection& collection)
{
	tcu::StringTemplate geometryTemplate(spirvTemplate);

	map<string, string>		linesParams;
	linesParams.insert(pair<string, string>("TOPOLOGY", "InputLines"));

	map<string, string>		pointsParams;
	pointsParams.insert(pair<string, string>("TOPOLOGY", "InputPoints"));

	collection.add("geometry_lines")	<< geometryTemplate.specialize(linesParams)		<< options;
	collection.add("geometry_points")	<< geometryTemplate.specialize(pointsParams)	<< options;
}

void initializeMemory(Context& context, const Allocation& alloc, subgroups::SSBOData& data)
{
	const vk::VkFormat format = data.format;
	const vk::VkDeviceSize size = data.numElements *
		(data.isImage ? getFormatSizeInBytes(format) : getElementSizeInBytes(format, data.layout));
	if (subgroups::SSBOData::InitializeNonZero == data.initializeType)
	{
		de::Random rnd(context.getTestContext().getCommandLine().getBaseSeed());

		switch (format)
		{
			default:
				DE_FATAL("Illegal buffer format");
				break;
			case VK_FORMAT_R8_USCALED:
			case VK_FORMAT_R8G8_USCALED:
			case VK_FORMAT_R8G8B8_USCALED:
			case VK_FORMAT_R8G8B8A8_USCALED:
			case VK_FORMAT_R32_SINT:
			case VK_FORMAT_R32G32_SINT:
			case VK_FORMAT_R32G32B32_SINT:
			case VK_FORMAT_R32G32B32A32_SINT:
			case VK_FORMAT_R32_UINT:
			case VK_FORMAT_R32G32_UINT:
			case VK_FORMAT_R32G32B32_UINT:
			case VK_FORMAT_R32G32B32A32_UINT:
			{
				deUint32* ptr = reinterpret_cast<deUint32*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / sizeof(deUint32)); k++)
				{
					ptr[k] = rnd.getUint32();
				}
			}
			break;
			case VK_FORMAT_R32_SFLOAT:
			case VK_FORMAT_R32G32_SFLOAT:
			case VK_FORMAT_R32G32B32_SFLOAT:
			case VK_FORMAT_R32G32B32A32_SFLOAT:
			{
				float* ptr = reinterpret_cast<float*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / sizeof(float)); k++)
				{
					ptr[k] = rnd.getFloat();
				}
			}
			break;
			case VK_FORMAT_R64_SFLOAT:
			case VK_FORMAT_R64G64_SFLOAT:
			case VK_FORMAT_R64G64B64_SFLOAT:
			case VK_FORMAT_R64G64B64A64_SFLOAT:
			{
				double* ptr = reinterpret_cast<double*>(alloc.getHostPtr());

				for (vk::VkDeviceSize k = 0; k < (size / sizeof(double)); k++)
				{
					ptr[k] = rnd.getDouble();
				}
			}
			break;
		}
	}
	else if (subgroups::SSBOData::InitializeZero == data.initializeType)
	{
		deUint32* ptr = reinterpret_cast<deUint32*>(alloc.getHostPtr());

		for (vk::VkDeviceSize k = 0; k < size / 4; k++)
		{
			ptr[k] = 0;
		}
	}

	if (subgroups::SSBOData::InitializeNone != data.initializeType)
	{
		flushAlloc(context.getDeviceInterface(), context.getDevice(), alloc);
	}
}

deUint32 getResultBinding (const VkShaderStageFlagBits shaderStage)
{
	switch(shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
			return 0u;
			break;
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return 1u;
			break;
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return 2u;
			break;
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return 3u;
			break;
		default:
			DE_ASSERT(0);
			return -1;
	}
	DE_ASSERT(0);
	return -1;
}

tcu::TestStatus vkt::subgroups::makeTessellationEvaluationFrameBufferTest (
	Context& context, VkFormat format, SSBOData* extraData,
	deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize),
	const VkShaderStageFlags shaderStage)
{
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkDevice							device					= context.getDevice();
	const deUint32							maxWidth				= 1024u;
	vector<de::SharedPtr<BufferOrImage> >	inputBuffers			(extraDataCount);
	DescriptorSetLayoutBuilder				layoutBuilder;
	DescriptorPoolBuilder					poolBuilder;
	DescriptorSetUpdateBuilder				updateBuilder;
	Move <VkDescriptorPool>					descriptorPool;
	Move <VkDescriptorSet>					descriptorSet;

	const Unique<VkShaderModule>			vertexShaderModule		(createShaderModule(vk, device,
																		context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>			teCtrlShaderModule		(createShaderModule(vk, device,
																		context.getBinaryCollection().get("tesc"), 0u));
	const Unique<VkShaderModule>			teEvalShaderModule		(createShaderModule(vk, device,
																		context.getBinaryCollection().get("tese"), 0u));
	const Unique<VkShaderModule>			fragmentShaderModule	(createShaderModule(vk, device,
																	context.getBinaryCollection().get("fragment"), 0u));
	const Unique<VkRenderPass>				renderPass				(makeRenderPass(context, format));

	const VkVertexInputBindingDescription	vertexInputBinding		=
	{
		0u,											// binding;
		static_cast<deUint32>(sizeof(tcu::Vec4)),	// stride;
		VK_VERTEX_INPUT_RATE_VERTEX					// inputRate
	};

	const VkVertexInputAttributeDescription	vertexInputAttribute	=
	{
		0u,
		0u,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		0u
	};

	for (deUint32 i = 0u; i < extraDataCount; i++)
	{
		if (extraData[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraData[i].numElements), 1u, extraData[i].format));
		}
		else
		{
			vk::VkDeviceSize size = getElementSizeInBytes(extraData[i].format, extraData[i].layout) * extraData[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
		}
		const Allocation& alloc = inputBuffers[i]->getAllocation();
		initializeMemory(context, alloc, extraData[i]);
	}

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
		layoutBuilder.addBinding(inputBuffers[ndx]->getType(), 1u, shaderStage, DE_NULL);

	const Unique<VkDescriptorSetLayout>		descriptorSetLayout		(layoutBuilder.build(vk, device));

	const Unique<VkPipelineLayout>			pipelineLayout			(makePipelineLayout(context, *descriptorSetLayout));

	const Unique<VkPipeline>				pipeline				(makeGraphicsPipeline(context, *pipelineLayout,
																	VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
																	VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
																	*vertexShaderModule, *fragmentShaderModule, DE_NULL, *teCtrlShaderModule, *teEvalShaderModule,
																	*renderPass, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, &vertexInputBinding, &vertexInputAttribute, true, format));

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
		poolBuilder.addType(inputBuffers[ndx]->getType());

	if (extraDataCount > 0)
	{
		descriptorPool = poolBuilder.build(vk, device,
							VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	}

	for (deUint32 buffersNdx = 0u; buffersNdx < inputBuffers.size(); buffersNdx++)
	{
		if (inputBuffers[buffersNdx]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[buffersNdx]->getAsImage()->getSampler(),
										inputBuffers[buffersNdx]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
										DescriptorSetUpdateBuilder::Location::binding(buffersNdx),
										inputBuffers[buffersNdx]->getType(), &info);
		}
		else
		{
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[buffersNdx]->getAsBuffer()->getBuffer(),
										0ull, inputBuffers[buffersNdx]->getAsBuffer()->getSize());

			updateBuilder.writeSingle(*descriptorSet,
										DescriptorSetUpdateBuilder::Location::binding(buffersNdx),
										inputBuffers[buffersNdx]->getType(), &info);
		}
	}

	updateBuilder.update(vk, device);

	const VkQueue							queue					= context.getUniversalQueue();
	const Unique<VkCommandPool>				cmdPool					(makeCommandPool(context));
	const deUint32							subgroupSize			= getSubgroupSize(context);
	const Unique<VkCommandBuffer>			cmdBuffer				(makeCommandBuffer(context, *cmdPool));
	const vk::VkDeviceSize					vertexBufferSize		= 2ull * maxWidth * sizeof(tcu::Vec4);
	Buffer									vertexBuffer			(context, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	unsigned								totalIterations			= 0u;
	unsigned								failedIterations		= 0u;
	Image									discardableImage		(context, maxWidth, 1u, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	{
		const Allocation&		alloc				= vertexBuffer.getAllocation();
		std::vector<tcu::Vec4>	data				(2u * maxWidth, Vec4(1.0f, 0.0f, 1.0f, 1.0f));
		const float				pixelSize			= 2.0f / static_cast<float>(maxWidth);
		float					leftHandPosition	= -1.0f;

		for(deUint32 ndx = 0u; ndx < data.size(); ndx+=2u)
		{
			data[ndx][0] = leftHandPosition;
			leftHandPosition += pixelSize;
			data[ndx+1][0] = leftHandPosition;
		}

		deMemcpy(alloc.getHostPtr(), &data[0], data.size() * sizeof(tcu::Vec4));
		flushAlloc(vk, device, alloc);
	}

	for (deUint32 width = 1u; width < maxWidth; ++width)
	{
		const Unique<VkFramebuffer>	framebuffer			(makeFramebuffer(context, *renderPass, discardableImage.getImageView(), maxWidth, 1));
		const VkViewport			viewport			= makeViewport(maxWidth, 1u);
		const VkRect2D				scissor				= makeRect2D(maxWidth, 1u);
		const vk::VkDeviceSize		imageResultSize		= tcu::getPixelSize(vk::mapVkFormat(format)) * maxWidth;
		Buffer						imageBufferResult	(context, imageResultSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const VkDeviceSize			vertexBufferOffset	= 0u;

		totalIterations++;

		beginCommandBuffer(vk, *cmdBuffer);
		{

			vk.cmdSetViewport(*cmdBuffer, 0, 1, &viewport);
			vk.cmdSetScissor(*cmdBuffer, 0, 1, &scissor);

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, maxWidth, 1u), tcu::Vec4(0.0f));

			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			if (extraDataCount > 0)
			{
				vk.cmdBindDescriptorSets(*cmdBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
					&descriptorSet.get(), 0u, DE_NULL);
			}

			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, vertexBuffer.getBufferPtr(), &vertexBufferOffset);
			vk.cmdDraw(*cmdBuffer, 2 * width, 1, 0, 0);

			endRenderPass(vk, *cmdBuffer);

			copyImageToBuffer(vk, *cmdBuffer, discardableImage.getImage(), imageBufferResult.getBuffer(), tcu::IVec2(maxWidth, 1), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			endCommandBuffer(vk, *cmdBuffer);

			submitCommandsAndWait(vk, device, queue, *cmdBuffer);
		}

		{
			const Allocation& allocResult = imageBufferResult.getAllocation();
			invalidateAlloc(vk, device, allocResult);

			std::vector<const void*> datas;
			datas.push_back(allocResult.getHostPtr());
			if (!checkResult(datas, width/2u, subgroupSize))
				failedIterations++;
		}
	}

	if (0 < failedIterations)
	{
		context.getTestContext().getLog()
				<< TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}

bool vkt::subgroups::check(std::vector<const void*> datas,
	deUint32 width, deUint32 ref)
{
	const deUint32* data = reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 n = 0; n < width; ++n)
	{
		if (data[n] != ref)
		{
			return false;
		}
	}

	return true;
}

bool vkt::subgroups::checkCompute(std::vector<const void*> datas,
	const deUint32 numWorkgroups[3], const deUint32 localSize[3],
	deUint32 ref)
{
	const deUint32 globalSizeX = numWorkgroups[0] * localSize[0];
	const deUint32 globalSizeY = numWorkgroups[1] * localSize[1];
	const deUint32 globalSizeZ = numWorkgroups[2] * localSize[2];

	return check(datas, globalSizeX * globalSizeY * globalSizeZ, ref);
}

tcu::TestStatus vkt::subgroups::makeGeometryFrameBufferTest(
	Context& context, VkFormat format, SSBOData* extraData,
	deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize))
{
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkDevice							device					= context.getDevice();
	const deUint32							maxWidth				= 1024u;
	vector<de::SharedPtr<BufferOrImage> >	inputBuffers			(extraDataCount);
	DescriptorSetLayoutBuilder				layoutBuilder;
	DescriptorPoolBuilder					poolBuilder;
	DescriptorSetUpdateBuilder				updateBuilder;
	Move <VkDescriptorPool>					descriptorPool;
	Move <VkDescriptorSet>					descriptorSet;

	const Unique<VkShaderModule>			vertexShaderModule		(createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>			geometryShaderModule	(createShaderModule(vk, device, context.getBinaryCollection().get("geometry"), 0u));
	const Unique<VkShaderModule>			fragmentShaderModule	(createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0u));
	const Unique<VkRenderPass>				renderPass				(makeRenderPass(context, format));
	const VkVertexInputBindingDescription	vertexInputBinding		=
	{
		0u,											// binding;
		static_cast<deUint32>(sizeof(tcu::Vec4)),	// stride;
		VK_VERTEX_INPUT_RATE_VERTEX					// inputRate
	};

	const VkVertexInputAttributeDescription	vertexInputAttribute	=
	{
		0u,
		0u,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		0u
	};

	for (deUint32 i = 0u; i < extraDataCount; i++)
	{
		if (extraData[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraData[i].numElements), 1u, extraData[i].format));
		}
		else
		{
			vk::VkDeviceSize size = getElementSizeInBytes(extraData[i].format, extraData[i].layout) * extraData[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
		}
		const Allocation& alloc = inputBuffers[i]->getAllocation();
		initializeMemory(context, alloc, extraData[i]);
	}

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
		layoutBuilder.addBinding(inputBuffers[ndx]->getType(), 1u, VK_SHADER_STAGE_GEOMETRY_BIT, DE_NULL);

	const Unique<VkDescriptorSetLayout>		descriptorSetLayout		(layoutBuilder.build(vk, device));

	const Unique<VkPipelineLayout>			pipelineLayout			(makePipelineLayout(context, *descriptorSetLayout));

	const Unique<VkPipeline>				pipeline				(makeGraphicsPipeline(context, *pipelineLayout,
																	VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
																	*vertexShaderModule, *fragmentShaderModule, *geometryShaderModule, DE_NULL, DE_NULL,
																	*renderPass, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, &vertexInputBinding, &vertexInputAttribute, true, format));

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
		poolBuilder.addType(inputBuffers[ndx]->getType());

	if (extraDataCount > 0)
	{
		descriptorPool = poolBuilder.build(vk, device,
							VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	}

	for (deUint32 buffersNdx = 0u; buffersNdx < inputBuffers.size(); buffersNdx++)
	{
		if (inputBuffers[buffersNdx]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[buffersNdx]->getAsImage()->getSampler(),
										inputBuffers[buffersNdx]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
										DescriptorSetUpdateBuilder::Location::binding(buffersNdx),
										inputBuffers[buffersNdx]->getType(), &info);
		}
		else
		{
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[buffersNdx]->getAsBuffer()->getBuffer(),
										0ull, inputBuffers[buffersNdx]->getAsBuffer()->getSize());

			updateBuilder.writeSingle(*descriptorSet,
										DescriptorSetUpdateBuilder::Location::binding(buffersNdx),
										inputBuffers[buffersNdx]->getType(), &info);
		}
	}

	updateBuilder.update(vk, device);

	const VkQueue							queue					= context.getUniversalQueue();
	const Unique<VkCommandPool>				cmdPool					(makeCommandPool(context));
	const deUint32							subgroupSize			= getSubgroupSize(context);
	const Unique<VkCommandBuffer>			cmdBuffer				(makeCommandBuffer(context, *cmdPool));
	const vk::VkDeviceSize					vertexBufferSize		= maxWidth * sizeof(tcu::Vec4);
	Buffer									vertexBuffer			(context, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	unsigned								totalIterations			= 0u;
	unsigned								failedIterations		= 0u;
	Image									discardableImage		(context, maxWidth, 1u, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	{
		const Allocation&		alloc				= vertexBuffer.getAllocation();
		std::vector<tcu::Vec4>	data				(maxWidth, Vec4(1.0f, 1.0f, 1.0f, 1.0f));
		const float				pixelSize			= 2.0f / static_cast<float>(maxWidth);
		float					leftHandPosition	= -1.0f;

		for(deUint32 ndx = 0u; ndx < maxWidth; ++ndx)
		{
			data[ndx][0] = leftHandPosition + pixelSize / 2.0f;
			leftHandPosition += pixelSize;
		}

		deMemcpy(alloc.getHostPtr(), &data[0], maxWidth * sizeof(tcu::Vec4));
		flushAlloc(vk, device, alloc);
	}

	for (deUint32 width = 1u; width < maxWidth; width++)
	{
		totalIterations++;
		const Unique<VkFramebuffer>	framebuffer			(makeFramebuffer(context, *renderPass, discardableImage.getImageView(), maxWidth, 1));
		const VkViewport			viewport			= makeViewport(maxWidth, 1u);
		const VkRect2D				scissor				= makeRect2D(maxWidth, 1u);
		const vk::VkDeviceSize		imageResultSize		= tcu::getPixelSize(vk::mapVkFormat(format)) * maxWidth;
		Buffer						imageBufferResult	(context, imageResultSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const VkDeviceSize			vertexBufferOffset	= 0u;

		for (deUint32 ndx = 0u; ndx < inputBuffers.size(); ndx++)
		{
			const Allocation& alloc = inputBuffers[ndx]->getAllocation();
			initializeMemory(context, alloc, extraData[ndx]);
		}

		beginCommandBuffer(vk, *cmdBuffer);
		{
			vk.cmdSetViewport(*cmdBuffer, 0, 1, &viewport);

			vk.cmdSetScissor(*cmdBuffer, 0, 1, &scissor);

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, maxWidth, 1u), tcu::Vec4(0.0f));

			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			if (extraDataCount > 0)
			{
				vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
					&descriptorSet.get(), 0u, DE_NULL);
			}

			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, vertexBuffer.getBufferPtr(), &vertexBufferOffset);

			vk.cmdDraw(*cmdBuffer, width, 1u, 0u, 0u);

			endRenderPass(vk, *cmdBuffer);

			copyImageToBuffer(vk, *cmdBuffer, discardableImage.getImage(), imageBufferResult.getBuffer(), tcu::IVec2(maxWidth, 1), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			endCommandBuffer(vk, *cmdBuffer);

			submitCommandsAndWait(vk, device, queue, *cmdBuffer);
		}

		{
			const Allocation& allocResult = imageBufferResult.getAllocation();
			invalidateAlloc(vk, device, allocResult);

			std::vector<const void*> datas;
			datas.push_back(allocResult.getHostPtr());
			if (!checkResult(datas, width, subgroupSize))
				failedIterations++;
		}
	}

	if (0 < failedIterations)
	{
		context.getTestContext().getLog()
				<< TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}


tcu::TestStatus vkt::subgroups::allStages(
	Context& context, VkFormat format, SSBOData* extraDatas,
	deUint32 extraDatasCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize),
	const VkShaderStageFlags shaderStageTested)
{
	const DeviceInterface&			vk					= context.getDeviceInterface();
	const VkDevice					device				= context.getDevice();
	const deUint32					maxWidth			= 1024u;
	vector<VkShaderStageFlagBits>	stagesVector;
	VkShaderStageFlags				shaderStageRequired	= (VkShaderStageFlags)0ull;

	Move<VkShaderModule>			vertexShaderModule;
	Move<VkShaderModule>			teCtrlShaderModule;
	Move<VkShaderModule>			teEvalShaderModule;
	Move<VkShaderModule>			geometryShaderModule;
	Move<VkShaderModule>			fragmentShaderModule;

	if (shaderStageTested & VK_SHADER_STAGE_VERTEX_BIT)
	{
		stagesVector.push_back(VK_SHADER_STAGE_VERTEX_BIT);
	}
	if (shaderStageTested & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
	{
		stagesVector.push_back(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
		shaderStageRequired |= (shaderStageTested & (VkShaderStageFlags)VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) ? (VkShaderStageFlags) 0u : (VkShaderStageFlags)VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		shaderStageRequired |= (shaderStageTested & (VkShaderStageFlags)VK_SHADER_STAGE_VERTEX_BIT) ? (VkShaderStageFlags) 0u : (VkShaderStageFlags)VK_SHADER_STAGE_VERTEX_BIT;
	}
	if (shaderStageTested & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
	{
		stagesVector.push_back(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
		shaderStageRequired |= (shaderStageTested & (VkShaderStageFlags)VK_SHADER_STAGE_VERTEX_BIT) ? (VkShaderStageFlags) 0u : (VkShaderStageFlags)VK_SHADER_STAGE_VERTEX_BIT;
		shaderStageRequired |= (shaderStageTested & (VkShaderStageFlags)VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ? (VkShaderStageFlags) 0u : (VkShaderStageFlags)VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	}
	if (shaderStageTested & VK_SHADER_STAGE_GEOMETRY_BIT)
	{
		stagesVector.push_back(VK_SHADER_STAGE_GEOMETRY_BIT);
		const VkShaderStageFlags required = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStageRequired |=  (shaderStageTested & required) ? (VkShaderStageFlags) 0 : required;
	}
	if (shaderStageTested & VK_SHADER_STAGE_FRAGMENT_BIT)
	{
		const VkShaderStageFlags required = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStageRequired |=  (shaderStageTested & required) ? (VkShaderStageFlags) 0 : required;
	}

	const deUint32	stagesCount	= static_cast<deUint32>(stagesVector.size());
	const string	vert		= (shaderStageRequired & VK_SHADER_STAGE_VERTEX_BIT)					? "vert_noSubgroup"		: "vert";
	const string	tesc		= (shaderStageRequired & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)		? "tesc_noSubgroup"		: "tesc";
	const string	tese		= (shaderStageRequired & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)	? "tese_noSubgroup"		: "tese";

	shaderStageRequired = shaderStageTested | shaderStageRequired;

	vertexShaderModule = createShaderModule(vk, device, context.getBinaryCollection().get(vert), 0u);
	if (shaderStageRequired & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
	{
		teCtrlShaderModule = createShaderModule(vk, device, context.getBinaryCollection().get(tesc), 0u);
		teEvalShaderModule = createShaderModule(vk, device, context.getBinaryCollection().get(tese), 0u);
	}
	if (shaderStageRequired & VK_SHADER_STAGE_GEOMETRY_BIT)
	{
		if (shaderStageRequired & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		{
			// tessellation shaders output line primitives
			geometryShaderModule = createShaderModule(vk, device, context.getBinaryCollection().get("geometry_lines"), 0u);
		}
		else
		{
			// otherwise points are processed by geometry shader
			geometryShaderModule = createShaderModule(vk, device, context.getBinaryCollection().get("geometry_points"), 0u);
		}
	}
	if (shaderStageRequired & VK_SHADER_STAGE_FRAGMENT_BIT)
		fragmentShaderModule = createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0u);

	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers(stagesCount + extraDatasCount);

	DescriptorSetLayoutBuilder layoutBuilder;
	// The implicit result SSBO we use to store our outputs from the shader
	for (deUint32 ndx = 0u; ndx < stagesCount; ++ndx)
	{
		const VkDeviceSize shaderSize = (stagesVector[ndx] == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) ? maxWidth * 2 : maxWidth;
		const VkDeviceSize size = getElementSizeInBytes(format, SSBOData::LayoutStd430) * shaderSize;
		inputBuffers[ndx] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));

		layoutBuilder.addIndexedBinding(inputBuffers[ndx]->getType(), 1, stagesVector[ndx], getResultBinding(stagesVector[ndx]), DE_NULL);
	}

	for (deUint32 ndx = stagesCount; ndx < stagesCount + extraDatasCount; ++ndx)
	{
		const deUint32 datasNdx = ndx - stagesCount;
		if (extraDatas[datasNdx].isImage)
		{
			inputBuffers[ndx] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraDatas[datasNdx].numElements), 1, extraDatas[datasNdx].format));
		}
		else
		{
			const vk::VkDeviceSize size = getElementSizeInBytes(extraDatas[datasNdx].format, extraDatas[datasNdx].layout) * extraDatas[datasNdx].numElements;
			inputBuffers[ndx] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
		}

		const Allocation& alloc = inputBuffers[ndx]->getAllocation();
		initializeMemory(context, alloc, extraDatas[datasNdx]);

		layoutBuilder.addIndexedBinding(inputBuffers[ndx]->getType(), 1,
								extraDatas[datasNdx].stages, extraDatas[datasNdx].binding, DE_NULL);
	}

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));

	const Unique<VkPipelineLayout> pipelineLayout(
		makePipelineLayout(context, *descriptorSetLayout));

	const Unique<VkRenderPass> renderPass(makeRenderPass(context, format));
	const Unique<VkPipeline> pipeline(makeGraphicsPipeline(context, *pipelineLayout,
										shaderStageRequired,
										*vertexShaderModule, *fragmentShaderModule, *geometryShaderModule, *teCtrlShaderModule, *teEvalShaderModule,
										*renderPass,
										(shaderStageRequired & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_POINT_LIST));

	Move <VkDescriptorPool>	descriptorPool;
	Move <VkDescriptorSet>	descriptorSet;

	if (inputBuffers.size() > 0)
	{
		DescriptorPoolBuilder poolBuilder;

		for (deUint32 ndx = 0u; ndx < static_cast<deUint32>(inputBuffers.size()); ndx++)
		{
			poolBuilder.addType(inputBuffers[ndx]->getType());
		}

		descriptorPool = poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		// Create descriptor set
		descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

		DescriptorSetUpdateBuilder updateBuilder;

		for (deUint32 ndx = 0u; ndx < stagesCount + extraDatasCount; ndx++)
		{
			deUint32 binding;
			if (ndx < stagesCount) binding = getResultBinding(stagesVector[ndx]);
			else binding = extraDatas[ndx -stagesCount].binding;

			if (inputBuffers[ndx]->isImage())
			{
				VkDescriptorImageInfo info =
					makeDescriptorImageInfo(inputBuffers[ndx]->getAsImage()->getSampler(),
											inputBuffers[ndx]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

				updateBuilder.writeSingle(	*descriptorSet,
											DescriptorSetUpdateBuilder::Location::binding(binding),
											inputBuffers[ndx]->getType(), &info);
			}
			else
			{
				VkDescriptorBufferInfo info =
					makeDescriptorBufferInfo(inputBuffers[ndx]->getAsBuffer()->getBuffer(),
							0ull, inputBuffers[ndx]->getAsBuffer()->getSize());

				updateBuilder.writeSingle(	*descriptorSet,
													DescriptorSetUpdateBuilder::Location::binding(binding),
													inputBuffers[ndx]->getType(), &info);
			}
		}

		updateBuilder.update(vk, device);
	}

	{
		const VkQueue					queue					= context.getUniversalQueue();
		const Unique<VkCommandPool>		cmdPool					(makeCommandPool(context));
		const deUint32					subgroupSize			= getSubgroupSize(context);
		const Unique<VkCommandBuffer>	cmdBuffer				(makeCommandBuffer(context, *cmdPool));
		unsigned						totalIterations			= 0u;
		unsigned						failedIterations		= 0u;
		Image							resultImage				(context, maxWidth, 1, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		const Unique<VkFramebuffer>		framebuffer				(makeFramebuffer(context, *renderPass, resultImage.getImageView(), maxWidth, 1));
		const VkViewport				viewport				= makeViewport(maxWidth, 1u);
		const VkRect2D					scissor					= makeRect2D(maxWidth, 1u);
		const vk::VkDeviceSize			imageResultSize			= tcu::getPixelSize(vk::mapVkFormat(format)) * maxWidth;
		Buffer							imageBufferResult		(context, imageResultSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const VkImageSubresourceRange	subresourceRange		=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,											//VkImageAspectFlags	aspectMask
			0u,																	//deUint32				baseMipLevel
			1u,																	//deUint32				levelCount
			0u,																	//deUint32				baseArrayLayer
			1u																	//deUint32				layerCount
		};

		const VkImageMemoryBarrier		colorAttachmentBarrier	= makeImageMemoryBarrier(
			(VkAccessFlags)0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			resultImage.getImage(), subresourceRange);

		for (deUint32 width = 1u; width < maxWidth; width++)
		{
			for (deUint32 ndx = stagesCount; ndx < stagesCount + extraDatasCount; ++ndx)
			{
				// re-init the data
				const Allocation& alloc = inputBuffers[ndx]->getAllocation();
				initializeMemory(context, alloc, extraDatas[ndx - stagesCount]);
			}

			totalIterations++;

			beginCommandBuffer(vk, *cmdBuffer);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0u, (const VkMemoryBarrier*)DE_NULL, 0u, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &colorAttachmentBarrier);

			vk.cmdSetViewport(*cmdBuffer, 0, 1, &viewport);

			vk.cmdSetScissor(*cmdBuffer, 0, 1, &scissor);

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, maxWidth, 1u), tcu::Vec4(0.0f));

			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			if (stagesCount + extraDatasCount > 0)
				vk.cmdBindDescriptorSets(*cmdBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
						&descriptorSet.get(), 0u, DE_NULL);

			vk.cmdDraw(*cmdBuffer, width, 1, 0, 0);

			endRenderPass(vk, *cmdBuffer);

			copyImageToBuffer(vk, *cmdBuffer, resultImage.getImage(), imageBufferResult.getBuffer(), tcu::IVec2(width, 1), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			endCommandBuffer(vk, *cmdBuffer);

			submitCommandsAndWait(vk, device, queue, *cmdBuffer);

			for (deUint32 ndx = 0u; ndx < stagesCount; ++ndx)
			{
				std::vector<const void*> datas;
				if (!inputBuffers[ndx]->isImage())
				{
					const Allocation& resultAlloc = inputBuffers[ndx]->getAllocation();
					invalidateAlloc(vk, device, resultAlloc);
					// we always have our result data first
					datas.push_back(resultAlloc.getHostPtr());
				}

				for (deUint32 index = stagesCount; index < stagesCount + extraDatasCount; ++index)
				{
					const deUint32 datasNdx = index - stagesCount;
					if ((stagesVector[ndx] & extraDatas[datasNdx].stages) && (!inputBuffers[index]->isImage()))
					{
						const Allocation& resultAlloc = inputBuffers[index]->getAllocation();
						invalidateAlloc(vk, device, resultAlloc);
						// we always have our result data first
						datas.push_back(resultAlloc.getHostPtr());
					}
				}

				if (!checkResult(datas, (stagesVector[ndx] == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) ? width * 2 : width , subgroupSize))
					failedIterations++;
			}
			if (shaderStageTested & VK_SHADER_STAGE_FRAGMENT_BIT)
			{
				std::vector<const void*> datas;
				const Allocation& resultAlloc = imageBufferResult.getAllocation();
				invalidateAlloc(vk, device, resultAlloc);

				// we always have our result data first
				datas.push_back(resultAlloc.getHostPtr());

				for (deUint32 index = stagesCount; index < stagesCount + extraDatasCount; ++index)
				{
					const deUint32 datasNdx = index - stagesCount;
					if (VK_SHADER_STAGE_FRAGMENT_BIT & extraDatas[datasNdx].stages && (!inputBuffers[index]->isImage()))
					{
						const Allocation& alloc = inputBuffers[index]->getAllocation();
						invalidateAlloc(vk, device, alloc);
						// we always have our result data first
						datas.push_back(alloc.getHostPtr());
					}
				}

				if (!checkResult(datas, width , subgroupSize))
					failedIterations++;
			}

			vk.resetCommandBuffer(*cmdBuffer, 0);
		}

		if (0 < failedIterations)
		{
			context.getTestContext().getLog()
					<< TestLog::Message << (totalIterations - failedIterations) << " / "
					<< totalIterations << " values passed" << TestLog::EndMessage;
			return tcu::TestStatus::fail("Failed!");
		}
	}

	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus vkt::subgroups::makeVertexFrameBufferTest(Context& context, vk::VkFormat format,
	SSBOData* extraData, deUint32 extraDataCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width, deUint32 subgroupSize))
{
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkDevice							device					= context.getDevice();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							maxWidth				= 1024u;
	vector<de::SharedPtr<BufferOrImage> >	inputBuffers			(extraDataCount);
	DescriptorSetLayoutBuilder				layoutBuilder;
	const Unique<VkShaderModule>			vertexShaderModule		(createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>			fragmentShaderModule	(createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0u));
	const Unique<VkRenderPass>				renderPass				(makeRenderPass(context, format));

	const VkVertexInputBindingDescription	vertexInputBinding		=
	{
		0u,											// binding;
		static_cast<deUint32>(sizeof(tcu::Vec4)),	// stride;
		VK_VERTEX_INPUT_RATE_VERTEX					// inputRate
	};

	const VkVertexInputAttributeDescription	vertexInputAttribute	=
	{
		0u,
		0u,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		0u
	};

	for (deUint32 i = 0u; i < extraDataCount; i++)
	{
		if (extraData[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context, static_cast<deUint32>(extraData[i].numElements), 1u, extraData[i].format));
		}
		else
		{
			vk::VkDeviceSize size = getElementSizeInBytes(extraData[i].format, extraData[i].layout) * extraData[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
		}
		const Allocation& alloc = inputBuffers[i]->getAllocation();
		initializeMemory(context, alloc, extraData[i]);
	}

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
		layoutBuilder.addBinding(inputBuffers[ndx]->getType(), 1u, VK_SHADER_STAGE_VERTEX_BIT, DE_NULL);

	const Unique<VkDescriptorSetLayout>		descriptorSetLayout		(layoutBuilder.build(vk, device));

	const Unique<VkPipelineLayout>			pipelineLayout			(makePipelineLayout(context, *descriptorSetLayout));

	const Unique<VkPipeline>				pipeline				(makeGraphicsPipeline(context, *pipelineLayout,
																		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
																		*vertexShaderModule, *fragmentShaderModule,
																		DE_NULL, DE_NULL, DE_NULL,
																		*renderPass, VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
																		&vertexInputBinding, &vertexInputAttribute, true, format));
	DescriptorPoolBuilder					poolBuilder;
	DescriptorSetUpdateBuilder				updateBuilder;


	for (deUint32 ndx = 0u; ndx < inputBuffers.size(); ndx++)
		poolBuilder.addType(inputBuffers[ndx]->getType());

	Move <VkDescriptorPool>					descriptorPool;
	Move <VkDescriptorSet>					descriptorSet;

	if (extraDataCount > 0)
	{
		descriptorPool = poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	}

	for (deUint32 ndx = 0u; ndx < extraDataCount; ndx++)
	{
		const Allocation& alloc = inputBuffers[ndx]->getAllocation();
		initializeMemory(context, alloc, extraData[ndx]);
	}

	for (deUint32 buffersNdx = 0u; buffersNdx < inputBuffers.size(); buffersNdx++)
	{
		if (inputBuffers[buffersNdx]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[buffersNdx]->getAsImage()->getSampler(),
										inputBuffers[buffersNdx]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
										DescriptorSetUpdateBuilder::Location::binding(buffersNdx),
										inputBuffers[buffersNdx]->getType(), &info);
		}
		else
		{
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[buffersNdx]->getAsBuffer()->getBuffer(),
										0ull, inputBuffers[buffersNdx]->getAsBuffer()->getSize());

			updateBuilder.writeSingle(*descriptorSet,
										DescriptorSetUpdateBuilder::Location::binding(buffersNdx),
										inputBuffers[buffersNdx]->getType(), &info);
		}
	}
	updateBuilder.update(vk, device);

	const Unique<VkCommandPool>				cmdPool					(makeCommandPool(context));

	const deUint32							subgroupSize			= getSubgroupSize(context);

	const Unique<VkCommandBuffer>			cmdBuffer				(makeCommandBuffer(context, *cmdPool));

	const vk::VkDeviceSize					vertexBufferSize		= maxWidth * sizeof(tcu::Vec4);
	Buffer									vertexBuffer			(context, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	unsigned								totalIterations			= 0u;
	unsigned								failedIterations		= 0u;

	Image									discardableImage		(context, maxWidth, 1u, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	{
		const Allocation&		alloc				= vertexBuffer.getAllocation();
		std::vector<tcu::Vec4>	data				(maxWidth, Vec4(1.0f, 1.0f, 1.0f, 1.0f));
		const float				pixelSize			= 2.0f / static_cast<float>(maxWidth);
		float					leftHandPosition	= -1.0f;

		for(deUint32 ndx = 0u; ndx < maxWidth; ++ndx)
		{
			data[ndx][0] = leftHandPosition + pixelSize / 2.0f;
			leftHandPosition += pixelSize;
		}

		deMemcpy(alloc.getHostPtr(), &data[0], maxWidth * sizeof(tcu::Vec4));
		flushAlloc(vk, device, alloc);
	}

	for (deUint32 width = 1u; width < maxWidth; width++)
	{
		totalIterations++;
		const Unique<VkFramebuffer>	framebuffer			(makeFramebuffer(context, *renderPass, discardableImage.getImageView(), maxWidth, 1));
		const VkViewport			viewport			= makeViewport(maxWidth, 1u);
		const VkRect2D				scissor				= makeRect2D(maxWidth, 1u);
		const vk::VkDeviceSize		imageResultSize		= tcu::getPixelSize(vk::mapVkFormat(format)) * maxWidth;
		Buffer						imageBufferResult	(context, imageResultSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const VkDeviceSize			vertexBufferOffset	= 0u;

		for (deUint32 ndx = 0u; ndx < inputBuffers.size(); ndx++)
		{
			const Allocation& alloc = inputBuffers[ndx]->getAllocation();
			initializeMemory(context, alloc, extraData[ndx]);
		}

		beginCommandBuffer(vk, *cmdBuffer);
		{
			vk.cmdSetViewport(*cmdBuffer, 0, 1, &viewport);

			vk.cmdSetScissor(*cmdBuffer, 0, 1, &scissor);

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, maxWidth, 1u), tcu::Vec4(0.0f));

			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			if (extraDataCount > 0)
			{
				vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
					&descriptorSet.get(), 0u, DE_NULL);
			}

			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, vertexBuffer.getBufferPtr(), &vertexBufferOffset);

			vk.cmdDraw(*cmdBuffer, width, 1u, 0u, 0u);

			endRenderPass(vk, *cmdBuffer);

			copyImageToBuffer(vk, *cmdBuffer, discardableImage.getImage(), imageBufferResult.getBuffer(), tcu::IVec2(maxWidth, 1), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			endCommandBuffer(vk, *cmdBuffer);

			submitCommandsAndWait(vk, device, queue, *cmdBuffer);
		}

		{
			const Allocation& allocResult = imageBufferResult.getAllocation();
			invalidateAlloc(vk, device, allocResult);

			std::vector<const void*> datas;
			datas.push_back(allocResult.getHostPtr());
			if (!checkResult(datas, width, subgroupSize))
				failedIterations++;
		}
	}

	if (0 < failedIterations)
	{
		context.getTestContext().getLog()
				<< TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}


tcu::TestStatus vkt::subgroups::makeFragmentFrameBufferTest	(Context& context, VkFormat format, SSBOData* extraDatas,
	deUint32 extraDatasCount,
	bool (*checkResult)(std::vector<const void*> datas, deUint32 width,
						deUint32 height, deUint32 subgroupSize))
{
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkDevice							device					= context.getDevice();
	const VkQueue							queue					= context.getUniversalQueue();
	const Unique<VkShaderModule>			vertexShaderModule		(createShaderModule
																		(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>			fragmentShaderModule	(createShaderModule
																		(vk, device, context.getBinaryCollection().get("fragment"), 0u));

	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers(extraDatasCount);

	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		if (extraDatas[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context,
										static_cast<deUint32>(extraDatas[i].numElements), 1, extraDatas[i].format));
		}
		else
		{
			vk::VkDeviceSize size =
				getElementSizeInBytes(extraDatas[i].format, extraDatas[i].layout) * extraDatas[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
		}

		const Allocation& alloc = inputBuffers[i]->getAllocation();
		initializeMemory(context, alloc, extraDatas[i]);
	}

	DescriptorSetLayoutBuilder layoutBuilder;

	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		layoutBuilder.addBinding(inputBuffers[i]->getType(), 1,
								 VK_SHADER_STAGE_FRAGMENT_BIT, DE_NULL);
	}

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		layoutBuilder.build(vk, device));

	const Unique<VkPipelineLayout> pipelineLayout(
		makePipelineLayout(context, *descriptorSetLayout));

	const Unique<VkRenderPass> renderPass(makeRenderPass(context, format));
	const Unique<VkPipeline> pipeline(makeGraphicsPipeline(context, *pipelineLayout,
									  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
									  *vertexShaderModule, *fragmentShaderModule, DE_NULL, DE_NULL, DE_NULL, *renderPass, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
									  DE_NULL, DE_NULL, true));

	DescriptorPoolBuilder poolBuilder;

	// To stop validation complaining, always add at least one type to pool.
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		poolBuilder.addType(inputBuffers[i]->getType());
	}

	Move<VkDescriptorPool> descriptorPool;
	// Create descriptor set
	Move<VkDescriptorSet> descriptorSet;

	if (extraDatasCount > 0)
	{
		descriptorPool = poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		descriptorSet	= makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	}

	DescriptorSetUpdateBuilder updateBuilder;

	for (deUint32 i = 0; i < extraDatasCount; i++)
	{
		if (inputBuffers[i]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[i]->getAsImage()->getSampler(),
										inputBuffers[i]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i),
									  inputBuffers[i]->getType(), &info);
		}
		else
		{
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[i]->getAsBuffer()->getBuffer(),
										 0ull, inputBuffers[i]->getAsBuffer()->getSize());

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i),
									  inputBuffers[i]->getType(), &info);
		}
	}

	if (extraDatasCount > 0)
		updateBuilder.update(vk, device);

	const Unique<VkCommandPool>		cmdPool				(makeCommandPool(context));

	const deUint32					subgroupSize		= getSubgroupSize(context);

	const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(context, *cmdPool));

	unsigned totalIterations = 0;
	unsigned failedIterations = 0;

	for (deUint32 width = 8; width <= subgroupSize; width *= 2)
	{
		for (deUint32 height = 8; height <= subgroupSize; height *= 2)
		{
			totalIterations++;

			// re-init the data
			for (deUint32 i = 0; i < extraDatasCount; i++)
			{
				const Allocation& alloc = inputBuffers[i]->getAllocation();
				initializeMemory(context, alloc, extraDatas[i]);
			}

			VkDeviceSize formatSize = getFormatSizeInBytes(format);
			const VkDeviceSize resultImageSizeInBytes =
				width * height * formatSize;

			Image resultImage(context, width, height, format,
							  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
							  VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

			Buffer resultBuffer(context, resultImageSizeInBytes,
								VK_IMAGE_USAGE_TRANSFER_DST_BIT);

			const Unique<VkFramebuffer> framebuffer(makeFramebuffer(context,
													*renderPass, resultImage.getImageView(), width, height));

			beginCommandBuffer(vk, *cmdBuffer);

			VkViewport viewport = makeViewport(width, height);

			vk.cmdSetViewport(
				*cmdBuffer, 0, 1, &viewport);

			VkRect2D scissor = {{0, 0}, {width, height}};

			vk.cmdSetScissor(
				*cmdBuffer, 0, 1, &scissor);

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, width, height), tcu::Vec4(0.0f));

			vk.cmdBindPipeline(
				*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			if (extraDatasCount > 0)
			{
				vk.cmdBindDescriptorSets(*cmdBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u,
						&descriptorSet.get(), 0u, DE_NULL);
			}

			vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);

			endRenderPass(vk, *cmdBuffer);

			copyImageToBuffer(vk, *cmdBuffer, resultImage.getImage(), resultBuffer.getBuffer(), tcu::IVec2(width, height), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			endCommandBuffer(vk, *cmdBuffer);

			submitCommandsAndWait(vk, device, queue, *cmdBuffer);

			std::vector<const void*> datas;
			{
				const Allocation& resultAlloc = resultBuffer.getAllocation();
				invalidateAlloc(vk, device, resultAlloc);

				// we always have our result data first
				datas.push_back(resultAlloc.getHostPtr());
			}

			if (!checkResult(datas, width, height, subgroupSize))
			{
				failedIterations++;
			}

			vk.resetCommandBuffer(*cmdBuffer, 0);
		}
	}

	if (0 < failedIterations)
	{
		context.getTestContext().getLog()
				<< TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus vkt::subgroups::makeComputeTest(
	Context& context, VkFormat format, SSBOData* inputs, deUint32 inputsCount,
	bool (*checkResult)(std::vector<const void*> datas,
						const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						deUint32 subgroupSize))
{
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkDevice							device					= context.getDevice();
	const VkQueue							queue					= context.getUniversalQueue();
	VkDeviceSize							elementSize				= getFormatSizeInBytes(format);

	const VkDeviceSize resultBufferSize = maxSupportedSubgroupSize() *
										  maxSupportedSubgroupSize() *
										  maxSupportedSubgroupSize();
	const VkDeviceSize resultBufferSizeInBytes = resultBufferSize * elementSize;

	Buffer resultBuffer(
		context, resultBufferSizeInBytes);

	std::vector< de::SharedPtr<BufferOrImage> > inputBuffers(inputsCount);

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		if (inputs[i].isImage)
		{
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Image(context,
										static_cast<deUint32>(inputs[i].numElements), 1, inputs[i].format));
		}
		else
		{
			vk::VkDeviceSize size =
				getElementSizeInBytes(inputs[i].format, inputs[i].layout) * inputs[i].numElements;
			inputBuffers[i] = de::SharedPtr<BufferOrImage>(new Buffer(context, size));
		}

		const Allocation& alloc = inputBuffers[i]->getAllocation();
		initializeMemory(context, alloc, inputs[i]);
	}

	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addBinding(
		resultBuffer.getType(), 1, VK_SHADER_STAGE_COMPUTE_BIT, DE_NULL);

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		layoutBuilder.addBinding(
			inputBuffers[i]->getType(), 1, VK_SHADER_STAGE_COMPUTE_BIT, DE_NULL);
	}

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		layoutBuilder.build(vk, device));

	const Unique<VkShaderModule> shaderModule(
		createShaderModule(vk, device,
						   context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(
		makePipelineLayout(context, *descriptorSetLayout));

	DescriptorPoolBuilder poolBuilder;

	poolBuilder.addType(resultBuffer.getType());

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		poolBuilder.addType(inputBuffers[i]->getType());
	}

	const Unique<VkDescriptorPool> descriptorPool(
		poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	// Create descriptor set
	const Unique<VkDescriptorSet> descriptorSet(
		makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	DescriptorSetUpdateBuilder updateBuilder;

	const VkDescriptorBufferInfo resultDescriptorInfo =
		makeDescriptorBufferInfo(
			resultBuffer.getBuffer(), 0ull, resultBufferSizeInBytes);

	updateBuilder.writeSingle(*descriptorSet,
							  DescriptorSetUpdateBuilder::Location::binding(0u),
							  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultDescriptorInfo);

	for (deUint32 i = 0; i < inputsCount; i++)
	{
		if (inputBuffers[i]->isImage())
		{
			VkDescriptorImageInfo info =
				makeDescriptorImageInfo(inputBuffers[i]->getAsImage()->getSampler(),
										inputBuffers[i]->getAsImage()->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i + 1),
									  inputBuffers[i]->getType(), &info);
		}
		else
		{
			vk::VkDeviceSize size =
				getElementSizeInBytes(inputs[i].format, inputs[i].layout) * inputs[i].numElements;
			VkDescriptorBufferInfo info =
				makeDescriptorBufferInfo(inputBuffers[i]->getAsBuffer()->getBuffer(), 0ull, size);

			updateBuilder.writeSingle(*descriptorSet,
									  DescriptorSetUpdateBuilder::Location::binding(i + 1),
									  inputBuffers[i]->getType(), &info);
		}
	}

	updateBuilder.update(vk, device);

	const Unique<VkCommandPool>		cmdPool				(makeCommandPool(context));

	unsigned totalIterations = 0;
	unsigned failedIterations = 0;

	const deUint32 subgroupSize = getSubgroupSize(context);

	const Unique<VkCommandBuffer> cmdBuffer(
		makeCommandBuffer(context, *cmdPool));

	const deUint32 numWorkgroups[3] = {4, 2, 2};

	const deUint32 localSizesToTestCount = 15;
	deUint32 localSizesToTest[localSizesToTestCount][3] =
	{
		{1, 1, 1},
		{32, 4, 1},
		{32, 1, 4},
		{1, 32, 4},
		{1, 4, 32},
		{4, 1, 32},
		{4, 32, 1},
		{subgroupSize, 1, 1},
		{1, subgroupSize, 1},
		{1, 1, subgroupSize},
		{3, 5, 7},
		{128, 1, 1},
		{1, 128, 1},
		{1, 1, 64},
		{1, 1, 1} // Isn't used, just here to make double buffering checks easier
	};

	Move<VkPipeline> lastPipeline(
		makeComputePipeline(context, *pipelineLayout, *shaderModule,
							localSizesToTest[0][0], localSizesToTest[0][1], localSizesToTest[0][2]));

	for (deUint32 index = 0; index < (localSizesToTestCount - 1); index++)
	{
		const deUint32 nextX = localSizesToTest[index + 1][0];
		const deUint32 nextY = localSizesToTest[index + 1][1];
		const deUint32 nextZ = localSizesToTest[index + 1][2];

		// we are running one test
		totalIterations++;

		beginCommandBuffer(vk, *cmdBuffer);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *lastPipeline);

		vk.cmdBindDescriptorSets(*cmdBuffer,
				VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
				&descriptorSet.get(), 0u, DE_NULL);

		vk.cmdDispatch(*cmdBuffer,numWorkgroups[0], numWorkgroups[1], numWorkgroups[2]);

		endCommandBuffer(vk, *cmdBuffer);

		Move<VkPipeline> nextPipeline(
			makeComputePipeline(context, *pipelineLayout, *shaderModule,
								nextX, nextY, nextZ));

		submitCommandsAndWait(vk, device, queue, *cmdBuffer);

		std::vector<const void*> datas;

		{
			const Allocation& resultAlloc = resultBuffer.getAllocation();
			invalidateAlloc(vk, device, resultAlloc);

			// we always have our result data first
			datas.push_back(resultAlloc.getHostPtr());
		}

		for (deUint32 i = 0; i < inputsCount; i++)
		{
			if (!inputBuffers[i]->isImage())
			{
				const Allocation& resultAlloc = inputBuffers[i]->getAllocation();
				invalidateAlloc(vk, device, resultAlloc);

				// we always have our result data first
				datas.push_back(resultAlloc.getHostPtr());
			}
		}

		if (!checkResult(datas, numWorkgroups, localSizesToTest[index], subgroupSize))
		{
			failedIterations++;
		}

		vk.resetCommandBuffer(*cmdBuffer, 0);

		lastPipeline = nextPipeline;
	}

	if (0 < failedIterations)
	{
		context.getTestContext().getLog()
				<< TestLog::Message << (totalIterations - failedIterations) << " / "
				<< totalIterations << " values passed" << TestLog::EndMessage;
		return tcu::TestStatus::fail("Failed!");
	}

	return tcu::TestStatus::pass("OK");
}
