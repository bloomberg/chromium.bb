/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright 2014 The Android Open Source Project
 * Copyright (c) 2015 The Khronos Group Inc.
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
 * \brief Early fragment tests
 *//*--------------------------------------------------------------------*/

#include "vktFragmentOperationsEarlyFragmentTests.hpp"
#include "vktFragmentOperationsMakeUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <string>

namespace vkt
{
namespace FragmentOperations
{
namespace
{
using namespace vk;
using de::UniquePtr;

//! Basic 2D image.
inline VkImageCreateInfo makeImageCreateInfo (const tcu::IVec2& size, const VkFormat format, const VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType;
		DE_NULL,												// const void*				pNext;
		(VkImageCreateFlags)0,									// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,										// VkImageType				imageType;
		format,													// VkFormat					format;
		makeExtent3D(size.x(), size.y(), 1),					// VkExtent3D				extent;
		1u,														// deUint32					mipLevels;
		1u,														// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling;
		usage,													// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
		0u,														// deUint32					queueFamilyIndexCount;
		DE_NULL,												// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout			initialLayout;
	};
	return imageParams;
}

Move<VkRenderPass> makeRenderPass (const DeviceInterface&	vk,
								   const VkDevice			device,
								   const VkFormat			colorFormat,
								   const bool				useDepthStencilAttachment,
								   const VkFormat			depthStencilFormat)
{
	return makeRenderPass(vk, device, colorFormat, useDepthStencilAttachment ? depthStencilFormat : VK_FORMAT_UNDEFINED);
}

Move<VkFramebuffer> makeFramebuffer (const DeviceInterface&		vk,
									 const VkDevice				device,
									 const VkRenderPass			renderPass,
									 const deUint32				attachmentCount,
									 const VkImageView*			pAttachments,
									 const tcu::IVec2			size)
{
	const VkFramebufferCreateInfo framebufferInfo = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		// VkStructureType                             sType;
		DE_NULL,										// const void*                                 pNext;
		(VkFramebufferCreateFlags)0,					// VkFramebufferCreateFlags                    flags;
		renderPass,										// VkRenderPass                                renderPass;
		attachmentCount,								// uint32_t                                    attachmentCount;
		pAttachments,									// const VkImageView*                          pAttachments;
		static_cast<deUint32>(size.x()),				// uint32_t                                    width;
		static_cast<deUint32>(size.y()),				// uint32_t                                    height;
		1u,												// uint32_t                                    layers;
	};

	return createFramebuffer(vk, device, &framebufferInfo);
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&	vk,
									   const VkDevice			device,
									   const VkPipelineLayout	pipelineLayout,
									   const VkRenderPass		renderPass,
									   const VkShaderModule		vertexModule,
									   const VkShaderModule		fragmentModule,
									   const tcu::IVec2&		renderSize,
									   const bool				enableDepthTest,
									   const bool				enableStencilTest)
{
	const std::vector<VkViewport>			viewports					(1, makeViewport(renderSize));
	const std::vector<VkRect2D>				scissors					(1, makeRect2D(renderSize));

	const VkStencilOpState					stencilOpState				= makeStencilOpState(
		VK_STENCIL_OP_KEEP,		// stencil fail
		VK_STENCIL_OP_KEEP,		// depth & stencil pass
		VK_STENCIL_OP_KEEP,		// depth only fail
		VK_COMPARE_OP_EQUAL,	// compare op
		1u,						// compare mask
		1u,						// write mask
		1u);					// reference

	VkPipelineDepthStencilStateCreateInfo	depthStencilStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType                          sType
		DE_NULL,													// const void*                              pNext
		0u,															// VkPipelineDepthStencilStateCreateFlags   flags
		enableDepthTest ? VK_TRUE : VK_FALSE,						// VkBool32                                 depthTestEnable
		enableDepthTest ? VK_TRUE : VK_FALSE,						// VkBool32                                 depthWriteEnable
		VK_COMPARE_OP_LESS,											// VkCompareOp                              depthCompareOp
		VK_FALSE,													// VkBool32                                 depthBoundsTestEnable
		enableStencilTest ? VK_TRUE : VK_FALSE,						// VkBool32                                 stencilTestEnable
		stencilOpState,												// VkStencilOpState                         front
		stencilOpState,												// VkStencilOpState                         back
		0.0f,														// float                                    minDepthBounds
		1.0f														// float                                    maxDepthBounds
	};

	return vk::makeGraphicsPipeline(vk,										// const DeviceInterface&                        vk
									device,									// const VkDevice                                device
									pipelineLayout,							// const VkPipelineLayout                        pipelineLayout
									vertexModule,							// const VkShaderModule                          vertexShaderModule
									DE_NULL,								// const VkShaderModule                          tessellationControlModule
									DE_NULL,								// const VkShaderModule                          tessellationEvalModule
									DE_NULL,								// const VkShaderModule                          geometryShaderModule
									fragmentModule,							// const VkShaderModule                          fragmentShaderModule
									renderPass,								// const VkRenderPass                            renderPass
									viewports,								// const std::vector<VkViewport>&                viewports
									scissors,								// const std::vector<VkRect2D>&                  scissors
									VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
									0u,										// const deUint32                                subpass
									0u,										// const deUint32                                patchControlPoints
									DE_NULL,								// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
									DE_NULL,								// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
									DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
									&depthStencilStateCreateInfo);			// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
}

void commandClearStencilAttachment (const DeviceInterface&	vk,
									const VkCommandBuffer	commandBuffer,
									const VkOffset2D&		offset,
									const VkExtent2D&		extent,
									const deUint32			clearValue)
{
	const VkClearAttachment stencilAttachment =
	{
		VK_IMAGE_ASPECT_STENCIL_BIT,					// VkImageAspectFlags    aspectMask;
		0u,												// uint32_t              colorAttachment;
		makeClearValueDepthStencil(0.0f, clearValue),	// VkClearValue          clearValue;
	};

	const VkClearRect rect =
	{
		{ offset, extent },		// VkRect2D    rect;
		0u,						// uint32_t    baseArrayLayer;
		1u,						// uint32_t    layerCount;
	};

	vk.cmdClearAttachments(commandBuffer, 1u, &stencilAttachment, 1u, &rect);
}

VkImageAspectFlags getImageAspectFlags (const VkFormat format)
{
	const tcu::TextureFormat tcuFormat = mapVkFormat(format);

	if      (tcuFormat.order == tcu::TextureFormat::DS)		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::D)		return VK_IMAGE_ASPECT_DEPTH_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::S)		return VK_IMAGE_ASPECT_STENCIL_BIT;

	DE_ASSERT(false);
	return 0u;
}

bool isSupportedDepthStencilFormat (const InstanceInterface& instanceInterface, const VkPhysicalDevice device, const VkFormat format)
{
	VkFormatProperties formatProps;
	instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);
	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

VkFormat pickSupportedDepthStencilFormat (const InstanceInterface&	instanceInterface,
										  const VkPhysicalDevice	device,
										  const deUint32			numFormats,
										  const VkFormat*			pFormats)
{
	for (deUint32 i = 0; i < numFormats; ++i)
		if (isSupportedDepthStencilFormat(instanceInterface, device, pFormats[i]))
			return pFormats[i];
	return VK_FORMAT_UNDEFINED;
}

enum Flags
{
	FLAG_TEST_DEPTH							= 1u << 0,
	FLAG_TEST_STENCIL						= 1u << 1,
	FLAG_DONT_USE_TEST_ATTACHMENT			= 1u << 2,
	FLAG_DONT_USE_EARLY_FRAGMENT_TESTS		= 1u << 3,
};

class EarlyFragmentTest : public TestCase
{
public:
						EarlyFragmentTest	(tcu::TestContext&		testCtx,
											 const std::string		name,
											 const deUint32			flags);

	void				initPrograms		(SourceCollections&		programCollection) const;
	TestInstance*		createInstance		(Context&				context) const;

private:
	const deUint32		m_flags;
};

EarlyFragmentTest::EarlyFragmentTest (tcu::TestContext& testCtx, const std::string name, const deUint32 flags)
	: TestCase	(testCtx, name, "")
	, m_flags	(flags)
{
}

void EarlyFragmentTest::initPrograms (SourceCollections& programCollection) const
{
	// Vertex
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(location = 0) in highp vec4 position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "   vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment
	{
		const bool useEarlyTests = (m_flags & FLAG_DONT_USE_EARLY_FRAGMENT_TESTS) == 0;
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< (useEarlyTests ? "layout(early_fragment_tests) in;\n" : "")
			<< "layout(location = 0) out highp vec4 fragColor;\n"
			<< "\n"
			<< "layout(binding = 0) coherent buffer Output {\n"
			<< "    uint result;\n"
			<< "} sb_out;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    atomicAdd(sb_out.result, 1u);\n"
			<< "	fragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

class EarlyFragmentTestInstance : public TestInstance
{
public:
							EarlyFragmentTestInstance (Context& context, const deUint32 flags);

	tcu::TestStatus			iterate					  (void);

private:
	enum TestMode
	{
		MODE_INVALID,
		MODE_DEPTH,
		MODE_STENCIL,
	};

	const TestMode			m_testMode;
	const bool				m_useTestAttachment;
	const bool				m_useEarlyTests;
};

EarlyFragmentTestInstance::EarlyFragmentTestInstance (Context& context, const deUint32 flags)
	: TestInstance			(context)
	, m_testMode			(flags & FLAG_TEST_DEPTH   ? MODE_DEPTH :
							 flags & FLAG_TEST_STENCIL ? MODE_STENCIL : MODE_INVALID)
	, m_useTestAttachment	((flags & FLAG_DONT_USE_TEST_ATTACHMENT) == 0)
	, m_useEarlyTests		((flags & FLAG_DONT_USE_EARLY_FRAGMENT_TESTS) == 0)
{
	DE_ASSERT(m_testMode != MODE_INVALID);
}

tcu::TestStatus EarlyFragmentTestInstance::iterate (void)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const VkDevice				device				= m_context.getDevice();
	const VkPhysicalDevice		physDevice			= m_context.getPhysicalDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&					allocator			= m_context.getDefaultAllocator();

	// Color attachment

	const tcu::IVec2				renderSize			= tcu::IVec2(32, 32);
	const VkFormat					colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageSubresourceRange	colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const Unique<VkImage>			colorImage			(makeImage(vk, device, makeImageCreateInfo(renderSize, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
	const UniquePtr<Allocation>		colorImageAlloc		(bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorImageView		(makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

	// Test attachment (depth or stencil)
	static const VkFormat stencilFormats[] =
	{
		// One of the following formats must be supported, as per spec requirement.
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	const VkFormat testFormat = (m_testMode == MODE_STENCIL ? pickSupportedDepthStencilFormat(vki, physDevice, DE_LENGTH_OF_ARRAY(stencilFormats), stencilFormats)
															: VK_FORMAT_D16_UNORM);		// spec requires this format to be supported
	if (testFormat == VK_FORMAT_UNDEFINED)
		return tcu::TestStatus::fail("Required depth/stencil format not supported");

	if (m_useTestAttachment)
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Using depth/stencil format " << getFormatName(testFormat) << tcu::TestLog::EndMessage;

	const VkImageSubresourceRange	testSubresourceRange	= makeImageSubresourceRange(getImageAspectFlags(testFormat), 0u, 1u, 0u, 1u);
	const Unique<VkImage>			testImage				(makeImage(vk, device, makeImageCreateInfo(renderSize, testFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)));
	const UniquePtr<Allocation>		testImageAlloc			(bindImage(vk, device, allocator, *testImage, MemoryRequirement::Any));
	const Unique<VkImageView>		testImageView			(makeImageView(vk, device, *testImage, VK_IMAGE_VIEW_TYPE_2D, testFormat, testSubresourceRange));
	const VkImageView				attachmentImages[]		= { *colorImageView, *testImageView };
	const deUint32					numUsedAttachmentImages = (m_useTestAttachment ? 2u : 1u);

	// Vertex buffer

	const deUint32					numVertices				= 6;
	const VkDeviceSize				vertexBufferSizeBytes	= sizeof(tcu::Vec4) * numVertices;
	const Unique<VkBuffer>			vertexBuffer			(makeBuffer(vk, device, makeBufferCreateInfo(vertexBufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)));
	const UniquePtr<Allocation>		vertexBufferAlloc		(bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

	{
		tcu::Vec4* const pVertices = reinterpret_cast<tcu::Vec4*>(vertexBufferAlloc->getHostPtr());

		pVertices[0] = tcu::Vec4( 1.0f, -1.0f,  0.5f,  1.0f);
		pVertices[1] = tcu::Vec4(-1.0f, -1.0f,  0.0f,  1.0f);
		pVertices[2] = tcu::Vec4(-1.0f,  1.0f,  0.5f,  1.0f);

		pVertices[3] = tcu::Vec4(-1.0f,  1.0f,  0.5f,  1.0f);
		pVertices[4] = tcu::Vec4( 1.0f,  1.0f,  1.0f,  1.0f);
		pVertices[5] = tcu::Vec4( 1.0f, -1.0f,  0.5f,  1.0f);

		flushAlloc(vk, device, *vertexBufferAlloc);
		// No barrier needed, flushed memory is automatically visible
	}

	// Result buffer

	const VkDeviceSize				resultBufferSizeBytes	= sizeof(deUint32);
	const Unique<VkBuffer>			resultBuffer			(makeBuffer(vk, device, makeBufferCreateInfo(resultBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)));
	const UniquePtr<Allocation>		resultBufferAlloc		(bindBuffer(vk, device, allocator, *resultBuffer, MemoryRequirement::HostVisible));

	{
		deUint32* const pData = static_cast<deUint32*>(resultBufferAlloc->getHostPtr());

		*pData = 0;
		flushAlloc(vk, device, *resultBufferAlloc);
	}

	// Render result buffer (to retrieve color attachment contents)

	const VkDeviceSize				colorBufferSizeBytes	= tcu::getPixelSize(mapVkFormat(colorFormat)) * renderSize.x() * renderSize.y();
	const Unique<VkBuffer>			colorBuffer				(makeBuffer(vk, device, makeBufferCreateInfo(colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT)));
	const UniquePtr<Allocation>		colorBufferAlloc		(bindBuffer(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	// Descriptors

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet				 (makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const VkDescriptorBufferInfo  resultBufferDescriptorInfo = makeDescriptorBufferInfo(resultBuffer.get(), 0ull, resultBufferSizeBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferDescriptorInfo)
		.update(vk, device);

	// Pipeline

	const Unique<VkShaderModule>	vertexModule  (createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragmentModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkRenderPass>		renderPass	  (makeRenderPass(vk, device, colorFormat, m_useTestAttachment, testFormat));
	const Unique<VkFramebuffer>		framebuffer	  (makeFramebuffer(vk, device, *renderPass, numUsedAttachmentImages, attachmentImages, renderSize));
	const Unique<VkPipelineLayout>	pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>		pipeline	  (makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModule, *fragmentModule, renderSize,
												  (m_testMode == MODE_DEPTH), (m_testMode == MODE_STENCIL)));
	const Unique<VkCommandPool>		cmdPool		  (createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer	  (allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Draw commands

	{
		const VkRect2D renderArea = {
			makeOffset2D(0, 0),
			makeExtent2D(renderSize.x(), renderSize.y()),
		};
		const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
		const VkDeviceSize vertexBufferOffset = 0ull;

		beginCommandBuffer(vk, *cmdBuffer);

		{
			const VkImageMemoryBarrier barriers[] = {
				makeImageMemoryBarrier(
					0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					*colorImage, colorSubresourceRange),
				makeImageMemoryBarrier(
					0u, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					*testImage, testSubresourceRange),
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(barriers), barriers);
		}

		// Will clear the attachments with specified depth and stencil values.
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor, 0.5f, 0u);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

		// Mask half of the attachment image with value that will pass the stencil test.
		if (m_useTestAttachment && m_testMode == MODE_STENCIL)
			commandClearStencilAttachment(vk, *cmdBuffer, makeOffset2D(0, 0), makeExtent2D(renderSize.x()/2, renderSize.y()), 1u);

		vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		copyImageToBuffer(vk, *cmdBuffer, *colorImage, *colorBuffer, renderSize, VK_ACCESS_SHADER_WRITE_BIT);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	// Log result image
	{
		invalidateAlloc(vk, device, *colorBufferAlloc);

		const tcu::ConstPixelBufferAccess imagePixelAccess(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1, colorBufferAlloc->getHostPtr());

		tcu::TestLog& log = m_context.getTestContext().getLog();
		log << tcu::TestLog::Image("color0", "Rendered image", imagePixelAccess);
	}

	// Verify results
	{
		invalidateAlloc(vk, device, *resultBufferAlloc);

		const int  actualCounter	   = *static_cast<deInt32*>(resultBufferAlloc->getHostPtr());
		const bool expectPartialResult = (m_useEarlyTests && m_useTestAttachment);
		const int  expectedCounter	   = expectPartialResult ? renderSize.x() * renderSize.y() / 2 : renderSize.x() * renderSize.y();
		const int  tolerance		   = expectPartialResult ? de::max(renderSize.x(), renderSize.y()) * 3	: 0;
		const int  expectedMin         = de::max(0, expectedCounter - tolerance);
		const int  expectedMax		   = expectedCounter + tolerance;

		tcu::TestLog& log = m_context.getTestContext().getLog();
		log << tcu::TestLog::Message << "Expected value"
			<< (expectPartialResult ? " in range: [" + de::toString(expectedMin) + ", " + de::toString(expectedMax) + "]" : ": " + de::toString(expectedCounter))
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Result value: " << de::toString(actualCounter) << tcu::TestLog::EndMessage;

		if (expectedMin <= actualCounter && actualCounter <= expectedMax)
			return tcu::TestStatus::pass("Success");
		else
			return tcu::TestStatus::fail("Value out of range");
	}
}

TestInstance* EarlyFragmentTest::createInstance (Context& context) const
{
	// Check required features
	{
		VkPhysicalDeviceFeatures features;
		context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), &features);

		// SSBO writes in fragment shader
		if (!features.fragmentStoresAndAtomics)
			throw tcu::NotSupportedError("Missing required feature: fragmentStoresAndAtomics");
	}

	return new EarlyFragmentTestInstance(context, m_flags);
}

} // anonymous ns

tcu::TestCaseGroup* createEarlyFragmentTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "early_fragment", "early fragment test cases"));

	static const struct
	{
		std::string caseName;
		deUint32	flags;
	} cases[] =
	{
		{ "no_early_fragment_tests_depth",					FLAG_TEST_DEPTH   | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS									},
		{ "no_early_fragment_tests_stencil",				FLAG_TEST_STENCIL | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS									},
		{ "early_fragment_tests_depth",						FLAG_TEST_DEPTH																			},
		{ "early_fragment_tests_stencil",					FLAG_TEST_STENCIL																		},
		{ "no_early_fragment_tests_depth_no_attachment",	FLAG_TEST_DEPTH   | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS | FLAG_DONT_USE_TEST_ATTACHMENT	},
		{ "no_early_fragment_tests_stencil_no_attachment",	FLAG_TEST_STENCIL | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS | FLAG_DONT_USE_TEST_ATTACHMENT	},
		{ "early_fragment_tests_depth_no_attachment",		FLAG_TEST_DEPTH   |										 FLAG_DONT_USE_TEST_ATTACHMENT	},
		{ "early_fragment_tests_stencil_no_attachment",		FLAG_TEST_STENCIL |										 FLAG_DONT_USE_TEST_ATTACHMENT	},
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
		testGroup->addChild(new EarlyFragmentTest(testCtx, cases[i].caseName, cases[i].flags));

	return testGroup.release();
}

} // FragmentOperations
} // vkt
