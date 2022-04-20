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
 * \brief Pipeline Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineTests.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineStencilTests.hpp"
#include "vktPipelineBlendTests.hpp"
#include "vktPipelineDepthTests.hpp"
#include "vktPipelineDynamicOffsetTests.hpp"
#include "vktPipelineEarlyDestroyTests.hpp"
#include "vktPipelineLogicOpTests.hpp"
#include "vktPipelineImageTests.hpp"
#include "vktPipelineInputAssemblyTests.hpp"
#include "vktPipelineInterfaceMatchingTests.hpp"
#include "vktPipelineSamplerTests.hpp"
#include "vktPipelineImageViewTests.hpp"
#include "vktPipelinePushConstantTests.hpp"
#include "vktPipelinePushDescriptorTests.hpp"
#include "vktPipelineSpecConstantTests.hpp"
#include "vktPipelineMatchedAttachmentsTests.hpp"
#include "vktPipelineMultisampleTests.hpp"
#include "vktPipelineMultisampleInterpolationTests.hpp"
#include "vktPipelineMultisampleShaderBuiltInTests.hpp"
#include "vktPipelineVertexInputTests.hpp"
#include "vktPipelineTimestampTests.hpp"
#include "vktPipelineCacheTests.hpp"
#include "vktPipelineRenderToImageTests.hpp"
#include "vktPipelineFramebufferAttachmentTests.hpp"
#include "vktPipelineStencilExportTests.hpp"
#include "vktPipelineDerivativeTests.hpp"
#include "vktPipelineCreationFeedbackTests.hpp"
#include "vktPipelineDepthRangeUnrestrictedTests.hpp"
#include "vktPipelineExecutablePropertiesTests.hpp"
#include "vktPipelineMiscTests.hpp"
#include "vktPipelineMaxVaryingsTests.hpp"
#include "vktPipelineBlendOperationAdvancedTests.hpp"
#include "vktPipelineExtendedDynamicStateTests.hpp"
#include "vktPipelineCreationCacheControlTests.hpp"
#include "vktPipelineNoPositionTests.hpp"
#include "vktPipelineBindPointTests.hpp"
#include "vktPipelineColorWriteEnableTests.hpp"
#include "vktPipelineLibraryTests.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

void createChildren (tcu::TestCaseGroup* group, PipelineConstructionType pipelineConstructionType)
{
	tcu::TestContext& testCtx = group->getTestContext();

	group->addChild(createStencilTests					(testCtx, pipelineConstructionType));
	group->addChild(createBlendTests					(testCtx, pipelineConstructionType));
	group->addChild(createDepthTests					(testCtx, pipelineConstructionType));
	group->addChild(createDynamicOffsetTests			(testCtx, pipelineConstructionType));
	group->addChild(createEarlyDestroyTests				(testCtx, pipelineConstructionType));
	group->addChild(createImageTests					(testCtx, pipelineConstructionType));
	group->addChild(createSamplerTests					(testCtx, pipelineConstructionType));
	group->addChild(createImageViewTests				(testCtx, pipelineConstructionType));
	group->addChild(createLogicOpTests					(testCtx, pipelineConstructionType));
	group->addChild(createPushConstantTests				(testCtx, pipelineConstructionType));
	group->addChild(createPushDescriptorTests			(testCtx, pipelineConstructionType));
	group->addChild(createSpecConstantTests				(testCtx, pipelineConstructionType));
	group->addChild(createMatchedAttachmentsTests		(testCtx, pipelineConstructionType));
	group->addChild(createMultisampleTests				(testCtx, pipelineConstructionType, false));
	group->addChild(createMultisampleTests				(testCtx, pipelineConstructionType, true));
	group->addChild(createMultisampleInterpolationTests	(testCtx, pipelineConstructionType));
	group->addChild(createMultisampleShaderBuiltInTests	(testCtx, pipelineConstructionType));
	group->addChild(createTestGroup						(testCtx, "vertex_input", "", createVertexInputTests, pipelineConstructionType));
	group->addChild(createInputAssemblyTests			(testCtx, pipelineConstructionType));
	group->addChild(createInterfaceMatchingTests		(testCtx, pipelineConstructionType));
	group->addChild(createTimestampTests				(testCtx, pipelineConstructionType));
	group->addChild(createCacheTests					(testCtx, pipelineConstructionType));
	group->addChild(createRenderToImageTests			(testCtx, pipelineConstructionType));
	group->addChild(createFramebufferAttachmentTests	(testCtx, pipelineConstructionType));
	group->addChild(createStencilExportTests			(testCtx, pipelineConstructionType));
	group->addChild(createCreationFeedbackTests			(testCtx, pipelineConstructionType));
	group->addChild(createDepthRangeUnrestrictedTests	(testCtx, pipelineConstructionType));
	group->addChild(createExecutablePropertiesTests		(testCtx, pipelineConstructionType));
	group->addChild(createMaxVaryingsTests				(testCtx, pipelineConstructionType));
	group->addChild(createBlendOperationAdvancedTests	(testCtx, pipelineConstructionType));
	group->addChild(createExtendedDynamicStateTests		(testCtx, pipelineConstructionType));
	group->addChild(createNoPositionTests				(testCtx, pipelineConstructionType));
	group->addChild(createBindPointTests				(testCtx, pipelineConstructionType));
	group->addChild(createColorWriteEnableTests			(testCtx, pipelineConstructionType));

	// NOTE: all new pipeline tests should use GraphicsPipelineWrapper for pipeline creation

	if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
		// there is no support for pipelineConstructionType in amber
		group->addChild(createMiscTests					(testCtx));

		// compute pipeline tests should not be repeated basing on pipelineConstructionType
		group->addChild(createDerivativeTests			(testCtx));

		// dont repeat tests requiring timing execution of vkCreate*Pipelines
		group->addChild(createCacheControlTests			(testCtx));
	}
	else if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
	{
		// execute pipeline library specific tests only once
		group->addChild(createPipelineLibraryTests		(testCtx));
	}
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> monolithicGroup			(createTestGroup(testCtx, "monolithic",				"Monolithic pipeline tests",					createChildren, PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC));
	de::MovePtr<tcu::TestCaseGroup> pipelineLibraryGroup	(createTestGroup(testCtx, "pipeline_library",		"Graphics pipeline library tests",				createChildren, PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY));
	de::MovePtr<tcu::TestCaseGroup> fastLinkedLibraryGroup	(createTestGroup(testCtx, "fast_linked_library",	"Fast linked graphics pipeline library tests",	createChildren, PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY));

	de::MovePtr<tcu::TestCaseGroup> mainGroup(new tcu::TestCaseGroup(testCtx, "pipeline", "Pipeline Tests"));
	mainGroup->addChild(monolithicGroup.release());
	mainGroup->addChild(pipelineLibraryGroup.release());
	mainGroup->addChild(fastLinkedLibraryGroup.release());
	return mainGroup.release();
}

} // pipeline
} // vkt
