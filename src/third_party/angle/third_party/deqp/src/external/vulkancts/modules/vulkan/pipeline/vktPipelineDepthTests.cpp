/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 *//*!
 * \file
 * \brief Depth Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineDepthTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"

#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

bool isSupportedDepthStencilFormat (const InstanceInterface& instanceInterface, VkPhysicalDevice device, VkFormat format)
{
	VkFormatProperties formatProps;

	instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);

	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u;
}

tcu::TestStatus testSupportsDepthStencilFormat (Context& context, VkFormat format)
{
	DE_ASSERT(vk::isDepthStencilFormat(format));

	if (isSupportedDepthStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice(), format))
		return tcu::TestStatus::pass("Format can be used in depth/stencil attachment");
	else
		return tcu::TestStatus::fail("Unsupported depth/stencil attachment format");
}

tcu::TestStatus testSupportsAtLeastOneDepthStencilFormat (Context& context, const std::vector<VkFormat> formats)
{
	std::ostringstream	supportedFormatsMsg;
	bool				pass					= false;

	DE_ASSERT(!formats.empty());

	for (size_t formatNdx = 0; formatNdx < formats.size(); formatNdx++)
	{
		const VkFormat format = formats[formatNdx];

		DE_ASSERT(vk::isDepthStencilFormat(format));

		if (isSupportedDepthStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice(), format))
		{
			pass = true;
			supportedFormatsMsg << vk::getFormatName(format);

			if (formatNdx < formats.size() - 1)
				supportedFormatsMsg << ", ";
		}
	}

	if (pass)
		return tcu::TestStatus::pass(std::string("Supported depth/stencil formats: ") + supportedFormatsMsg.str());
	else
		return tcu::TestStatus::fail("All depth/stencil formats are unsupported");
}

class DepthTest : public vkt::TestCase
{
public:
	enum
	{
		QUAD_COUNT = 4
	};

	static const float					quadDepths[QUAD_COUNT];

										DepthTest				(tcu::TestContext&		testContext,
																 const std::string&		name,
																 const std::string&		description,
																 const VkFormat			depthFormat,
																 const VkCompareOp		depthCompareOps[QUAD_COUNT],
																 const bool				depthBoundsTestEnable			= false,
																 const float			depthBoundsMin					= 0.0f,
																 const float			depthBoundsMax					= 1.0f,
																 const bool				depthTestEnable					= true,
																 const bool				stencilTestEnable				= false);
	virtual								~DepthTest				(void);
	virtual void						initPrograms			(SourceCollections& programCollection) const;
	virtual TestInstance*				createInstance			(Context& context) const;

private:
	const VkFormat						m_depthFormat;
	const bool							m_depthBoundsTestEnable;
	const float							m_depthBoundsMin;
	const float							m_depthBoundsMax;
	const bool							m_depthTestEnable;
	const bool							m_stencilTestEnable;
	VkCompareOp							m_depthCompareOps[QUAD_COUNT];
};

class DepthTestInstance : public vkt::TestInstance
{
public:
										DepthTestInstance		(Context&			context,
																 const VkFormat		depthFormat,
																 const VkCompareOp	depthCompareOps[DepthTest::QUAD_COUNT],
																 const bool			depthBoundsTestEnable,
																 const float		depthBoundsMin,
																 const float		depthBoundsMax,
																 const bool			depthTestEnable,
																 const bool			stencilTestEnable);
	virtual								~DepthTestInstance		(void);
	virtual tcu::TestStatus				iterate					(void);

private:
	tcu::TestStatus						verifyImage				(void);

private:
	VkCompareOp							m_depthCompareOps[DepthTest::QUAD_COUNT];
	const tcu::UVec2					m_renderSize;
	const VkFormat						m_colorFormat;
	const VkFormat						m_depthFormat;
	const bool							m_depthBoundsTestEnable;
	const float							m_depthBoundsMin;
	const float							m_depthBoundsMax;
	const bool							m_depthTestEnable;
	const bool							m_stencilTestEnable;
	VkImageSubresourceRange				m_depthImageSubresourceRange;

	Move<VkImage>						m_colorImage;
	de::MovePtr<Allocation>				m_colorImageAlloc;
	Move<VkImage>						m_depthImage;
	de::MovePtr<Allocation>				m_depthImageAlloc;
	Move<VkImageView>					m_colorAttachmentView;
	Move<VkImageView>					m_depthAttachmentView;
	Move<VkRenderPass>					m_renderPass;
	Move<VkFramebuffer>					m_framebuffer;

	Move<VkShaderModule>				m_vertexShaderModule;
	Move<VkShaderModule>				m_fragmentShaderModule;

	Move<VkBuffer>						m_vertexBuffer;
	std::vector<Vertex4RGBA>			m_vertices;
	de::MovePtr<Allocation>				m_vertexBufferAlloc;

	Move<VkPipelineLayout>				m_pipelineLayout;
	Move<VkPipeline>					m_graphicsPipelines[DepthTest::QUAD_COUNT];

	Move<VkCommandPool>					m_cmdPool;
	Move<VkCommandBuffer>				m_cmdBuffer;
};

const float DepthTest::quadDepths[QUAD_COUNT] =
{
	0.1f,
	0.0f,
	0.3f,
	0.2f
};

DepthTest::DepthTest (tcu::TestContext&		testContext,
					  const std::string&	name,
					  const std::string&	description,
					  const VkFormat		depthFormat,
					  const VkCompareOp		depthCompareOps[QUAD_COUNT],
					  const bool			depthBoundsTestEnable,
					  const float			depthBoundsMin,
					  const float			depthBoundsMax,
					  const bool			depthTestEnable,
					  const bool			stencilTestEnable)
	: vkt::TestCase	(testContext, name, description)
	, m_depthFormat				(depthFormat)
	, m_depthBoundsTestEnable	(depthBoundsTestEnable)
	, m_depthBoundsMin			(depthBoundsMin)
	, m_depthBoundsMax			(depthBoundsMax)
	, m_depthTestEnable			(depthTestEnable)
	, m_stencilTestEnable		(stencilTestEnable)
{
	deMemcpy(m_depthCompareOps, depthCompareOps, sizeof(VkCompareOp) * QUAD_COUNT);
}

DepthTest::~DepthTest (void)
{
}

TestInstance* DepthTest::createInstance (Context& context) const
{
	return new DepthTestInstance(context, m_depthFormat, m_depthCompareOps, m_depthBoundsTestEnable, m_depthBoundsMin, m_depthBoundsMax, m_depthTestEnable, m_stencilTestEnable);
}

void DepthTest::initPrograms (SourceCollections& programCollection) const
{
	programCollection.glslSources.add("color_vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout(location = 0) in vec4 position;\n"
		"layout(location = 1) in vec4 color;\n"
		"layout(location = 0) out highp vec4 vtxColor;\n"
		"void main (void)\n"
		"{\n"
		"	gl_Position = position;\n"
		"	vtxColor = color;\n"
		"}\n");

	programCollection.glslSources.add("color_frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 vtxColor;\n"
		"layout(location = 0) out highp vec4 fragColor;\n"
		"void main (void)\n"
		"{\n"
		"	fragColor = vtxColor;\n"
		"}\n");
}

DepthTestInstance::DepthTestInstance (Context&				context,
									  const VkFormat		depthFormat,
									  const VkCompareOp		depthCompareOps[DepthTest::QUAD_COUNT],
									  const bool			depthBoundsTestEnable,
									  const float			depthBoundsMin,
									  const float			depthBoundsMax,
									  const bool			depthTestEnable,
									  const bool			stencilTestEnable)
	: vkt::TestInstance			(context)
	, m_renderSize				(32, 32)
	, m_colorFormat				(VK_FORMAT_R8G8B8A8_UNORM)
	, m_depthFormat				(depthFormat)
	, m_depthBoundsTestEnable	(depthBoundsTestEnable)
	, m_depthBoundsMin			(depthBoundsMin)
	, m_depthBoundsMax			(depthBoundsMax)
	, m_depthTestEnable			(depthTestEnable)
	, m_stencilTestEnable		(stencilTestEnable)
{
	const DeviceInterface&		vk						= context.getDeviceInterface();
	const VkDevice				vkDevice				= context.getDevice();
	const deUint32				queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const VkComponentMapping	componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	// Check depthBounds support
	if (m_depthBoundsTestEnable && !context.getDeviceFeatures().depthBounds)
		TCU_THROW(NotSupportedError, "depthBounds feature is not supported");

	// Copy depth operators
	deMemcpy(m_depthCompareOps, depthCompareOps, sizeof(VkCompareOp) * DepthTest::QUAD_COUNT);

	// Create color image
	{
		const VkImageCreateInfo colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			0u,																			// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
			m_colorFormat,																// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },									// VkExtent3D				extent;
			1u,																			// deUint32					mipLevels;
			1u,																			// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode;
			1u,																			// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,															// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout			initialLayout;
		};

		m_colorImage			= createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory
		m_colorImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));
	}

	// Create depth image
	{
		// Check format support
		if (!isSupportedDepthStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice(), m_depthFormat))
			throw tcu::NotSupportedError(std::string("Unsupported depth/stencil format: ") + getFormatName(m_depthFormat));

		const VkImageCreateInfo depthImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
			m_depthFormat,									// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },		// VkExtent3D				extent;
			1u,												// deUint32					mipLevels;
			1u,												// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT,				// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
			1u,												// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,								// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
		};

		m_depthImage = createImage(vk, vkDevice, &depthImageParams);

		// Allocate and bind depth image memory
		m_depthImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_depthImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_depthImage, m_depthImageAlloc->getMemory(), m_depthImageAlloc->getOffset()));

		const VkImageAspectFlags aspect = (mapVkFormat(m_depthFormat).order == tcu::TextureFormat::DS ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
																									  : VK_IMAGE_ASPECT_DEPTH_BIT);
		m_depthImageSubresourceRange    = makeImageSubresourceRange(aspect, 0u, depthImageParams.mipLevels, 0u, depthImageParams.arrayLayers);
	}

	// Create color attachment view
	{
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_colorImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_colorFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkComponentMapping		components;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	// Create depth attachment view
	{
		const VkImageViewCreateInfo depthAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_depthImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			m_depthFormat,									// VkFormat					format;
			componentMappingRGBA,							// VkComponentMapping		components;
			m_depthImageSubresourceRange,					// VkImageSubresourceRange	subresourceRange;
		};

		m_depthAttachmentView = createImageView(vk, vkDevice, &depthAttachmentViewParams);
	}

	// Create render pass
	m_renderPass = makeRenderPass(vk, vkDevice, m_colorFormat, m_depthFormat, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	// Create framebuffer
	{
		const VkImageView attachmentBindInfos[2] =
		{
			*m_colorAttachmentView,
			*m_depthAttachmentView,
		};

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkFramebufferCreateFlags		flags;
			*m_renderPass,										// VkRenderPass					renderPass;
			2u,													// deUint32						attachmentCount;
			attachmentBindInfos,								// const VkImageView*			pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32						width;
			(deUint32)m_renderSize.y(),							// deUint32						height;
			1u													// deUint32						layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkPipelineLayoutCreateFlags		flags;
			0u,													// deUint32							setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*		pSetLayouts;
			0u,													// deUint32							pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Shader modules
	m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
	m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

	// Create pipeline
	{
		const std::vector<VkViewport>				viewports							(1, makeViewport(m_renderSize));
		const std::vector<VkRect2D>					scissors							(1, makeRect2D(m_renderSize));;

		const VkVertexInputBindingDescription		vertexInputBindingDescription		=
		{
			0u,							// deUint32					binding;
			sizeof(Vertex4RGBA),		// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputStepRate	inputRate;
		};

		const VkVertexInputAttributeDescription		vertexInputAttributeDescriptions[2]	=
		{
			{
				0u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				0u									// deUint32	offset;
			},
			{
				1u,									// deUint32	location;
				0u,									// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat	format;
				DE_OFFSET_OF(Vertex4RGBA, color),	// deUint32	offset;
			}
		};

		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineVertexInputStateCreateFlags	flags;
			1u,																// deUint32									vertexBindingDescriptionCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,																// deUint32									vertexAttributeDescriptionCount;
			vertexInputAttributeDescriptions								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		VkPipelineDepthStencilStateCreateInfo		depthStencilStateParams				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// VkPipelineDepthStencilStateCreateFlags	flags;
			m_depthTestEnable,											// VkBool32									depthTestEnable;
			true,														// VkBool32									depthWriteEnable;
			VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp;
			m_depthBoundsTestEnable,									// VkBool32									depthBoundsTestEnable;
			m_stencilTestEnable,										// VkBool32									stencilTestEnable;
			// VkStencilOpState	front;
			{
				VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
				0u,						// deUint32		compareMask;
				0u,						// deUint32		writeMask;
				0u,						// deUint32		reference;
			},
			// VkStencilOpState	back;
			{
				VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
				VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
				0u,						// deUint32		compareMask;
				0u,						// deUint32		writeMask;
				0u,						// deUint32		reference;
			},
			m_depthBoundsMin,			// float			minDepthBounds;
			m_depthBoundsMax,			// float			maxDepthBounds;
		};

		for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
		{
			depthStencilStateParams.depthCompareOp	= depthCompareOps[quadNdx];
			m_graphicsPipelines[quadNdx]			= makeGraphicsPipeline(vk,									// const DeviceInterface&                        vk
																		   vkDevice,							// const VkDevice                                device
																		   *m_pipelineLayout,					// const VkPipelineLayout                        pipelineLayout
																		   *m_vertexShaderModule,				// const VkShaderModule                          vertexShaderModule
																		   DE_NULL,								// const VkShaderModule                          tessellationControlModule
																		   DE_NULL,								// const VkShaderModule                          tessellationEvalModule
																		   DE_NULL,								// const VkShaderModule                          geometryShaderModule
																		   *m_fragmentShaderModule,				// const VkShaderModule                          fragmentShaderModule
																		   *m_renderPass,						// const VkRenderPass                            renderPass
																		   viewports,							// const std::vector<VkViewport>&                viewports
																		   scissors,							// const std::vector<VkRect2D>&                  scissors
																		   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
																		   0u,									// const deUint32                                subpass
																		   0u,									// const deUint32                                patchControlPoints
																		   &vertexInputStateParams,				// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
																		   DE_NULL,								// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
																		   DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
																		   &depthStencilStateParams);			// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
		}
	}

	// Create vertex buffer
	{
		const VkBufferCreateInfo vertexBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			1024u,										// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		m_vertices			= createOverlappingQuads();
		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Adjust depths
		for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
			for (int vertexNdx = 0; vertexNdx < 6; vertexNdx++)
				m_vertices[quadNdx * 6 + vertexNdx].position.z() = DepthTest::quadDepths[quadNdx];

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), m_vertices.data(), m_vertices.size() * sizeof(Vertex4RGBA));
		flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	{
		const VkClearValue attachmentClearValues[2] =
		{
			defaultClearValue(m_colorFormat),
			defaultClearValue(m_depthFormat),
		};
		const VkImageMemoryBarrier imageLayoutBarriers[] =
		{
			// color image layout transition
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,									// VkStructureType            sType;
				DE_NULL,																// const void*                pNext;
				(VkAccessFlags)0,														// VkAccessFlags              srcAccessMask;
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,									// VkAccessFlags              dstAccessMask;
				VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout              oldLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,								// VkImageLayout              newLayout;
				VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   dstQueueFamilyIndex;
				*m_colorImage,															// VkImage                    image;
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }							// VkImageSubresourceRange    subresourceRange;
			},
			// depth image layout transition
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,									// VkStructureType            sType;
				DE_NULL,																// const void*                pNext;
				(VkAccessFlags)0,														// VkAccessFlags              srcAccessMask;
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,							// VkAccessFlags              dstAccessMask;
				VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout              oldLayout;
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,						// VkImageLayout              newLayout;
				VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   dstQueueFamilyIndex;
				*m_depthImage,															// VkImage                    image;
				m_depthImageSubresourceRange,											// VkImageSubresourceRange    subresourceRange;
			},
		};

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *m_cmdBuffer, 0u);

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, (VkDependencyFlags)0,
			0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(imageLayoutBarriers), imageLayoutBarriers);

		beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), 2u, attachmentClearValues);

		const VkDeviceSize		quadOffset		= (m_vertices.size() / DepthTest::QUAD_COUNT) * sizeof(Vertex4RGBA);

		for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
		{
			VkDeviceSize vertexBufferOffset = quadOffset * quadNdx;

			vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelines[quadNdx]);
			vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
			vk.cmdDraw(*m_cmdBuffer, (deUint32)(m_vertices.size() / DepthTest::QUAD_COUNT), 1, 0, 0);
		}

		endRenderPass(vk, *m_cmdBuffer);
		endCommandBuffer(vk, *m_cmdBuffer);
	}
}

DepthTestInstance::~DepthTestInstance (void)
{
}

tcu::TestStatus DepthTestInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	return verifyImage();
}

tcu::TestStatus DepthTestInstance::verifyImage (void)
{
	const tcu::TextureFormat	tcuColorFormat	= mapVkFormat(m_colorFormat);
	const tcu::TextureFormat	tcuDepthFormat	= mapVkFormat(m_depthFormat);
	const ColorVertexShader		vertexShader;
	const ColorFragmentShader	fragmentShader	(tcuColorFormat, tcuDepthFormat);
	const rr::Program			program			(&vertexShader, &fragmentShader);
	ReferenceRenderer			refRenderer		(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
	bool						compareOk		= false;

	// Render reference image
	{
		for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
		{
			// Set depth state
			rr::RenderState renderState(refRenderer.getViewportState(), m_context.getDeviceProperties().limits.subPixelPrecisionBits);
			renderState.fragOps.depthTestEnabled = m_depthTestEnable;
			renderState.fragOps.depthFunc = mapVkCompareOp(m_depthCompareOps[quadNdx]);
			if (m_depthBoundsTestEnable)
			{
				renderState.fragOps.depthBoundsTestEnabled = true;
				renderState.fragOps.minDepthBound = m_depthBoundsMin;
				renderState.fragOps.maxDepthBound = m_depthBoundsMax;
			}

			refRenderer.draw(renderState,
							 rr::PRIMITIVETYPE_TRIANGLES,
							 std::vector<Vertex4RGBA>(m_vertices.begin() + quadNdx * 6,
													  m_vertices.begin() + (quadNdx + 1) * 6));
		}
	}

	// Compare color result with reference image
	{
		const DeviceInterface&			vk					= m_context.getDeviceInterface();
		const VkDevice					vkDevice			= m_context.getDevice();
		const VkQueue					queue				= m_context.getUniversalQueue();
		const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
		SimpleAllocator					allocator			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
		de::MovePtr<tcu::TextureLevel>	result				= readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImage, m_colorFormat, m_renderSize);

		compareOk = tcu::intThresholdPositionDeviationCompare(m_context.getTestContext().getLog(),
															  "IntImageCompare",
															  "Image comparison",
															  refRenderer.getAccess(),
															  result->getAccess(),
															  tcu::UVec4(2, 2, 2, 2),
															  tcu::IVec3(1, 1, 0),
															  true,
															  tcu::COMPARE_LOG_RESULT);
	}

	// Compare depth result with reference image
	{
		const DeviceInterface&			vk					= m_context.getDeviceInterface();
		const VkDevice					vkDevice			= m_context.getDevice();
		const VkQueue					queue				= m_context.getUniversalQueue();
		const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
		SimpleAllocator					allocator			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
		de::MovePtr<tcu::TextureLevel>	result				= readDepthAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_depthImage, m_depthFormat, m_renderSize);


		{
			de::MovePtr<tcu::TextureLevel>	convertedReferenceLevel;
			tcu::Maybe<tcu::TextureFormat>	convertedFormat;

			if (refRenderer.getDepthAccess().getFormat().type == tcu::TextureFormat::UNSIGNED_INT_24_8_REV)
			{
				convertedFormat = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT24);
			}
			if (refRenderer.getDepthAccess().getFormat().type == tcu::TextureFormat::UNSIGNED_INT_16_8_8)
			{
				convertedFormat = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::UNORM_INT16);
			}
			else if (refRenderer.getDepthAccess().getFormat().type == tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV)
			{
				convertedFormat = tcu::TextureFormat(tcu::TextureFormat::D, tcu::TextureFormat::FLOAT);
			}

			if (convertedFormat)
			{
				convertedReferenceLevel = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(*convertedFormat, refRenderer.getDepthAccess().getSize().x(), refRenderer.getDepthAccess().getSize().y()));
				tcu::copy(convertedReferenceLevel->getAccess(), refRenderer.getDepthAccess());
			}

			float depthThreshold = 0.0f;

			if (tcu::getTextureChannelClass(result->getFormat().type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
			{
				const tcu::IVec4	formatBits = tcu::getTextureFormatBitDepth(result->getFormat());
				depthThreshold = 1.0f / static_cast<float>((1 << formatBits[0]) - 1);
			}
			else if (tcu::getTextureChannelClass(result->getFormat().type) == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
			{

				depthThreshold = 0.0000001f;
			}
			else
				TCU_FAIL("unrecognized format type class");

			compareOk = tcu::floatThresholdCompare(m_context.getTestContext().getLog(),
												   "DepthImageCompare",
												   "Depth image comparison",
												   convertedReferenceLevel ? convertedReferenceLevel->getAccess() : refRenderer.getDepthAccess(),
												   result->getAccess(),
												   tcu::Vec4(depthThreshold, 0.0f, 0.0f, 0.0f),
												   tcu::COMPARE_LOG_RESULT);
		}
	}

	if (compareOk)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

std::string getFormatCaseName (const VkFormat format)
{
	const std::string	fullName	= getFormatName(format);

	DE_ASSERT(de::beginsWith(fullName, "VK_FORMAT_"));

	return de::toLower(fullName.substr(10));
}

std::string	getCompareOpsName (const VkCompareOp quadDepthOps[DepthTest::QUAD_COUNT])
{
	std::ostringstream name;

	for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
	{
		const std::string	fullOpName	= getCompareOpName(quadDepthOps[quadNdx]);

		DE_ASSERT(de::beginsWith(fullOpName, "VK_COMPARE_OP_"));

		name << de::toLower(fullOpName.substr(14));

		if (quadNdx < DepthTest::QUAD_COUNT - 1)
			name << "_";
	}

	return name.str();
}

std::string	getCompareOpsDescription (const VkCompareOp quadDepthOps[DepthTest::QUAD_COUNT])
{
	std::ostringstream desc;
	desc << "Draws " << DepthTest::QUAD_COUNT << " quads with depth compare ops: ";

	for (int quadNdx = 0; quadNdx < DepthTest::QUAD_COUNT; quadNdx++)
	{
		desc << getCompareOpName(quadDepthOps[quadNdx]) << " at depth " << DepthTest::quadDepths[quadNdx];

		if (quadNdx < DepthTest::QUAD_COUNT - 1)
			desc << ", ";
	}
	return desc.str();
}


} // anonymous

tcu::TestCaseGroup* createDepthTests (tcu::TestContext& testCtx)
{
	const VkFormat depthFormats[] =
	{
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT
	};

	// Each entry configures the depth compare operators of QUAD_COUNT quads.
	// All entries cover pair-wise combinations of compare operators.
	const VkCompareOp depthOps[][DepthTest::QUAD_COUNT] =
	{
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_LESS,				VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS,				VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_EQUAL,				VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_NOT_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_LESS_OR_EQUAL,		VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_ALWAYS,				VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_LESS_OR_EQUAL },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_LESS },
		{ VK_COMPARE_OP_GREATER_OR_EQUAL,	VK_COMPARE_OP_NEVER,			VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_NEVER },
		{ VK_COMPARE_OP_LESS,				VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_EQUAL,			VK_COMPARE_OP_EQUAL },
		{ VK_COMPARE_OP_NEVER,				VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_ALWAYS,			VK_COMPARE_OP_GREATER_OR_EQUAL },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER,			VK_COMPARE_OP_ALWAYS },
		{ VK_COMPARE_OP_NOT_EQUAL,			VK_COMPARE_OP_LESS_OR_EQUAL,	VK_COMPARE_OP_NOT_EQUAL,		VK_COMPARE_OP_GREATER }
	};

	de::MovePtr<tcu::TestCaseGroup> depthTests (new tcu::TestCaseGroup(testCtx, "depth", "Depth tests"));

	// Tests for format features
	{
		de::MovePtr<tcu::TestCaseGroup> formatFeaturesTests (new tcu::TestCaseGroup(testCtx, "format_features", "Checks depth format features"));

		// Formats that must be supported in all implementations
		addFunctionCase(formatFeaturesTests.get(),
						"support_d16_unorm",
						"Tests if VK_FORMAT_D16_UNORM is supported as depth/stencil attachment format",
						testSupportsDepthStencilFormat,
						VK_FORMAT_D16_UNORM);

		// Sets where at least one of the formats must be supported
		const VkFormat	depthOnlyFormats[]		= { VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D32_SFLOAT };
		const VkFormat	depthStencilFormats[]	= { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };

		addFunctionCase(formatFeaturesTests.get(),
						"support_d24_unorm_or_d32_sfloat",
						"Tests if any of VK_FORMAT_D24_UNORM_X8 or VK_FORMAT_D32_SFLOAT are supported as depth/stencil attachment format",
						testSupportsAtLeastOneDepthStencilFormat,
						std::vector<VkFormat>(depthOnlyFormats, depthOnlyFormats + DE_LENGTH_OF_ARRAY(depthOnlyFormats)));

		addFunctionCase(formatFeaturesTests.get(),
						"support_d24_unorm_s8_uint_or_d32_sfloat_s8_uint",
						"Tests if any of VK_FORMAT_D24_UNORM_S8_UINT or VK_FORMAT_D32_SFLOAT_S8_UINT are supported as depth/stencil attachment format",
						testSupportsAtLeastOneDepthStencilFormat,
						std::vector<VkFormat>(depthStencilFormats, depthStencilFormats + DE_LENGTH_OF_ARRAY(depthStencilFormats)));

		depthTests->addChild(formatFeaturesTests.release());
	}

	// Tests for format and compare operators
	{
		de::MovePtr<tcu::TestCaseGroup> formatTests (new tcu::TestCaseGroup(testCtx, "format", "Uses different depth formats"));

		for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(depthFormats); formatNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup>	formatTest		(new tcu::TestCaseGroup(testCtx,
																					getFormatCaseName(depthFormats[formatNdx]).c_str(),
																					(std::string("Uses format ") + getFormatName(depthFormats[formatNdx])).c_str()));
			de::MovePtr<tcu::TestCaseGroup>	compareOpsTests	(new tcu::TestCaseGroup(testCtx, "compare_ops", "Combines depth compare operators"));

			for (size_t opsNdx = 0; opsNdx < DE_LENGTH_OF_ARRAY(depthOps); opsNdx++)
			{
				compareOpsTests->addChild(new DepthTest(testCtx,
														getCompareOpsName(depthOps[opsNdx]),
														getCompareOpsDescription(depthOps[opsNdx]),
														depthFormats[formatNdx],
														depthOps[opsNdx]));

				compareOpsTests->addChild(new DepthTest(testCtx,
														getCompareOpsName(depthOps[opsNdx]) + "_depth_bounds_test",
														getCompareOpsDescription(depthOps[opsNdx]) + " with depth bounds test enabled",
														depthFormats[formatNdx],
														depthOps[opsNdx],
														true,
														0.1f,
														0.25f));
			}
			// Special VkPipelineDepthStencilStateCreateInfo known to have issues
			{
				const VkCompareOp depthOpsSpecial[DepthTest::QUAD_COUNT] = { VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NEVER, VK_COMPARE_OP_NEVER };

				compareOpsTests->addChild(new DepthTest(testCtx,
														"never_zerodepthbounds_depthdisabled_stencilenabled",
														"special VkPipelineDepthStencilStateCreateInfo",
														depthFormats[formatNdx],
														depthOpsSpecial,
														true,
														0.0f,
														0.0f,
														false,
														true));
			}
			formatTest->addChild(compareOpsTests.release());

			// Test case with depth test enabled, but depth write disabled
			de::MovePtr<tcu::TestCaseGroup>	depthTestDisabled(new tcu::TestCaseGroup(testCtx, "depth_test_disabled", "Test for disabled depth test"));
			{
				const VkCompareOp depthOpsDepthTestDisabled[DepthTest::QUAD_COUNT] = { VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_ALWAYS };
				depthTestDisabled->addChild(new DepthTest(testCtx,
														"depth_write_enabled",
														"Depth writes should not occur if depth test is disabled",
														depthFormats[formatNdx],
														depthOpsDepthTestDisabled,
														false,			/* depthBoundsTestEnable */
														0.0f,			/* depthBoundMin*/
														1.0f,			/* depthBoundMax*/
														false,			/* depthTestEnable */
														false			/* stencilTestEnable */));
			}
			formatTest->addChild(depthTestDisabled.release());
			formatTests->addChild(formatTest.release());
		}
		depthTests->addChild(formatTests.release());
	}

	return depthTests.release();
}

} // pipeline
} // vkt
