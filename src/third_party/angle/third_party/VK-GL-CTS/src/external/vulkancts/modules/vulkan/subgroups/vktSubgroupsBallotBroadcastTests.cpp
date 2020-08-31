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
 * \brief Subgroups Tests
 */ /*--------------------------------------------------------------------*/

#include "vktSubgroupsBallotBroadcastTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{
enum OpType
{
	OPTYPE_BROADCAST = 0,
	OPTYPE_BROADCAST_NONCONST,
	OPTYPE_BROADCAST_FIRST,
	OPTYPE_LAST
};

static bool checkVertexPipelineStages(const void* internalData, std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	DE_UNREF(internalData);
	return vkt::subgroups::check(datas, width, 3);
}

static bool checkCompute(const void* internalData, std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	DE_UNREF(internalData);
	return vkt::subgroups::checkCompute(datas, numWorkgroups, localSize, 3);
}

std::string getOpTypeCaseName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_BROADCAST:
			return "subgroupbroadcast";
		case OPTYPE_BROADCAST_NONCONST:
			return "subgroupbroadcast_nonconst";
		case OPTYPE_BROADCAST_FIRST:
			return "subgroupbroadcastfirst";
	}
}


struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
	deBool				extShaderSubGroupBallotTests;
	deBool				requiredSubgroupSize;
};

std::string getExtHeader(CaseDefinition caseDef)
{
	return (caseDef.extShaderSubGroupBallotTests ?	"#extension GL_ARB_shader_ballot: enable\n"
													"#extension GL_KHR_shader_subgroup_basic: enable\n"
													"#extension GL_ARB_gpu_shader_int64: enable\n"
												:	"#extension GL_KHR_shader_subgroup_ballot: enable\n")
				+ subgroups::getAdditionalExtensionForFormat(caseDef.format);
}

std::string getTestSrc(const CaseDefinition &caseDef)
{
	std::ostringstream bdy;

	std::string broadcast;
	std::string broadcastFirst;
	std::string mask;
	int max;
	if (caseDef.extShaderSubGroupBallotTests)
	{
		broadcast		= "readInvocationARB";
		broadcastFirst	= "readFirstInvocationARB";
		mask			= "mask = ballotARB(true);\n";
		max = 64;

		bdy << "  uint64_t mask;\n"
			<< mask
			<< "  uint sgSize = gl_SubGroupSizeARB;\n"
			<< "  uint sgInvocation = gl_SubGroupInvocationARB;\n";
	}
	else
	{
		broadcast		= "subgroupBroadcast";
		broadcastFirst	= "subgroupBroadcastFirst";
		mask			= "mask = subgroupBallot(true);\n";
		max = (int)subgroups::maxSupportedSubgroupSize();

		bdy << "  uvec4 mask = subgroupBallot(true);\n"
			<< "  uint sgSize = gl_SubgroupSize;\n"
			<< "  uint sgInvocation = gl_SubgroupInvocationID;\n";
	}

	const std::string fmt = subgroups::getFormatNameForGLSL(caseDef.format);

	if (caseDef.opType == OPTYPE_BROADCAST)
	{
		bdy	<< "  tempRes = 0x3;\n";
		for (int i = 0; i < max; i++)
		{
			bdy << "  {\n"
			<< "    const uint id = "<< i << ";\n"
			<< "    " << fmt << " op = " << broadcast << "(data[sgInvocation], id);\n"
			<< "    if ((id < sgSize) && subgroupBallotBitExtract(mask, id))\n"
			<< "    {\n"
			<< "      if (op != data[id])\n"
			<< "      {\n"
			<< "        tempRes = 0;\n"
			<< "      }\n"
			<< "    }\n"
			<< "  }\n";
		}
	}
	else if (caseDef.opType == OPTYPE_BROADCAST_NONCONST)
	{
		const std::string validate =	"    if (subgroupBallotBitExtract(mask, id) && op != data[id])\n"
										"        tempRes = 0;\n";

		bdy	<< "  tempRes= 0x3;\n"
			<< "  for (uint id = 0; id < sgSize; id++)\n"
			<< "  {\n"
			<< "    " << fmt << " op = " << broadcast << "(data[sgInvocation], id);\n"
			<< validate
			<< "  }\n"
			<< "  // Test lane id that is only uniform across active lanes\n"
			<< "  if (sgInvocation >= sgSize / 2)\n"
			<< "  {\n"
			<< "    uint id = sgInvocation & ~((sgSize / 2) - 1);\n"
			<< "    " << fmt << " op = " << broadcast << "(data[sgInvocation], id);\n"
			<< validate
			<< "  }\n";
	}
	else
	{
		bdy << "  tempRes = 0;\n"
			<< "  uint firstActive = 0;\n"
			<< "  for (uint i = 0; i < sgSize; i++)\n"
			<< "  {\n"
			<< "    if (subgroupBallotBitExtract(mask, i))\n"
			<< "    {\n"
			<< "      firstActive = i;\n"
			<< "      break;\n"
			<< "    }\n"
			<< "  }\n"
			<< "  tempRes |= (" << broadcastFirst << "(data[sgInvocation]) == data[firstActive]) ? 0x1 : 0;\n"
			<< "  // make the firstActive invocation inactive now\n"
			<< "  if (firstActive != sgInvocation)\n"
			<< "  {\n"
			<< mask
			<< "    for (uint i = 0; i < sgSize; i++)\n"
			<< "    {\n"
			<< "      if (subgroupBallotBitExtract(mask, i))\n"
			<< "      {\n"
			<< "        firstActive = i;\n"
			<< "        break;\n"
			<< "      }\n"
			<< "    }\n"
			<< "    tempRes |= (" << broadcastFirst << "(data[sgInvocation]) == data[firstActive]) ? 0x2 : 0;\n"
			<< "  }\n"
			<< "  else\n"
			<< "  {\n"
			<< "    // the firstActive invocation didn't partake in the second result so set it to true\n"
			<< "    tempRes |= 0x2;\n"
			<< "  }\n";
	}
	return bdy.str();
}

std::string getHelperFunctionARB(const CaseDefinition &caseDef)
{
	std::ostringstream bdy;

	if (caseDef.extShaderSubGroupBallotTests == DE_FALSE)
		return "";

	bdy << "bool subgroupBallotBitExtract(uint64_t value, uint index)\n";
	bdy << "{\n";
	bdy << "    if (index > 63)\n";
	bdy << "        return false;\n";
	bdy << "    uint64_t mask = 1ul << index;\n";
	bdy << "    if (bool((value & mask)) == true)\n";
	bdy << "        return true;\n";
	bdy << "    return false;\n";
	bdy << "}\n";
   return bdy.str();
}

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::SpirvVersion			spirvVersion = (caseDef.opType == OPTYPE_BROADCAST_NONCONST) ? vk::SPIRV_VERSION_1_5 : vk::SPIRV_VERSION_1_3;
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u);

	std::string extHeader = getExtHeader(caseDef);
	std::string testSrc = getTestSrc(caseDef);
	std::string helperStr = getHelperFunctionARB(caseDef);

   subgroups::initStdFrameBufferPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, *caseDef.geometryPointSizeSupported, extHeader, testSrc, helperStr);
}

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::SpirvVersion			spirvVersion = (caseDef.opType == OPTYPE_BROADCAST_NONCONST) ? vk::SPIRV_VERSION_1_5 : vk::SPIRV_VERSION_1_3;
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, spirvVersion, 0u);

	std::string extHeader = getExtHeader(caseDef);
	std::string testSrc = getTestSrc(caseDef);
	std::string helperStr = getHelperFunctionARB(caseDef);

	subgroups::initStdPrograms(programCollection, buildOptions, caseDef.shaderStage, caseDef.format, extHeader, testSrc, helperStr);
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");

	if (!subgroups::isFormatSupportedForDevice(context, caseDef.format))
		TCU_THROW(NotSupportedError, "Device does not support the specified format in subgroup operations");

	if (caseDef.extShaderSubGroupBallotTests && !context.requireDeviceFunctionality("VK_EXT_shader_subgroup_ballot"))
		TCU_THROW(NotSupportedError, "Device does not support VK_EXT_shader_subgroup_ballot extension");

	if (caseDef.extShaderSubGroupBallotTests && !subgroups::isInt64SupportedForDevice(context))
		TCU_THROW(NotSupportedError, "Device does not support int64 data types");

	if ((caseDef.opType == OPTYPE_BROADCAST_NONCONST) && !subgroups::isSubgroupBroadcastDynamicIdSupported(context))
		TCU_THROW(NotSupportedError, "Device does not support SubgroupBroadcastDynamicId");

	if (caseDef.requiredSubgroupSize)
	{
		if (!context.requireDeviceFunctionality("VK_EXT_subgroup_size_control"))
			TCU_THROW(NotSupportedError, "Device does not support VK_EXT_subgroup_size_control extension");
		VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControlFeatures;
		subgroupSizeControlFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT;
		subgroupSizeControlFeatures.pNext = DE_NULL;

		VkPhysicalDeviceFeatures2 features;
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &subgroupSizeControlFeatures;

		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features);

		if (subgroupSizeControlFeatures.subgroupSizeControl == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support varying subgroup sizes nor required subgroup size");

		if (subgroupSizeControlFeatures.computeFullSubgroups == DE_FALSE)
			TCU_THROW(NotSupportedError, "Device does not support full subgroups in compute shaders");
	}

	*caseDef.geometryPointSizeSupported = subgroups::isTessellationAndGeometryPointSizeSupported(context);


}

tcu::TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
	{
		if (subgroups::areSubgroupOperationsRequiredForStage(caseDef.shaderStage))
		{
			return tcu::TestStatus::fail(
					   "Shader stage " +
					   subgroups::getShaderStageName(caseDef.shaderStage) +
					   " is required to support subgroup operations!");
		}
		else
		{
			TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
		}
	}

	subgroups::SSBOData inputData;
	inputData.format = caseDef.format;
	inputData.layout = subgroups::SSBOData::LayoutStd140;
	inputData.numElements = caseDef.extShaderSubGroupBallotTests ? 64u : subgroups::maxSupportedSubgroupSize();
	inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}


tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
			return tcu::TestStatus::fail(
					"Shader stage " +
					subgroups::getShaderStageName(caseDef.shaderStage) +
					" is required to support subgroup operations!");
		}
		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.layout = subgroups::SSBOData::LayoutStd430;
		inputData.numElements = caseDef.extShaderSubGroupBallotTests ? 64u : subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

		if (caseDef.requiredSubgroupSize == DE_FALSE)
			return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkCompute);

		tcu::TestLog& log	= context.getTestContext().getLog();
		VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties;
		subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
		subgroupSizeControlProperties.pNext = DE_NULL;
		VkPhysicalDeviceProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &subgroupSizeControlProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

		log << tcu::TestLog::Message << "Testing required subgroup size range [" <<  subgroupSizeControlProperties.minSubgroupSize << ", "
			<< subgroupSizeControlProperties.maxSubgroupSize << "]" << tcu::TestLog::EndMessage;

		// According to the spec, requiredSubgroupSize must be a power-of-two integer.
		for (deUint32 size = subgroupSizeControlProperties.minSubgroupSize; size <= subgroupSizeControlProperties.maxSubgroupSize; size *= 2)
		{
			tcu::TestStatus result = subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkCompute,
																size, VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT);
			if (result.getCode() != QP_TEST_RESULT_PASS)
			{
				log << tcu::TestLog::Message << "subgroupSize " << size << " failed" << tcu::TestLog::EndMessage;
				return result;
			}
		}

		return tcu::TestStatus::pass("OK");
	}
	else
	{
		VkPhysicalDeviceSubgroupProperties subgroupProperties;
		subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
		subgroupProperties.pNext = DE_NULL;

		VkPhysicalDeviceProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &subgroupProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

		VkShaderStageFlagBits stages = (VkShaderStageFlagBits)(caseDef.shaderStage  & subgroupProperties.supportedStages);

		if (VK_SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
		{
			if ( (stages & VK_SHADER_STAGE_FRAGMENT_BIT) == 0)
				TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
			else
				stages = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		if ((VkShaderStageFlagBits)0u == stages)
			TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

		subgroups::SSBOData inputData;
		inputData.format			= caseDef.format;
		inputData.layout			= subgroups::SSBOData::LayoutStd430;
		inputData.numElements		= caseDef.extShaderSubGroupBallotTests ? 64u : subgroups::maxSupportedSubgroupSize();
		inputData.initializeType	= subgroups::SSBOData::InitializeNonZero;
		inputData.binding			= 4u;
		inputData.stages			= stages;

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, &inputData, 1, DE_NULL, checkVertexPipelineStages, stages);
	}
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsBallotBroadcastTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup ballot broadcast category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup ballot broadcast category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup ballot broadcast category tests: framebuffer"));

	de::MovePtr<tcu::TestCaseGroup> graphicGroupARB(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup ballot broadcast category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroupARB(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup ballot broadcast category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroupARB(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup ballot broadcast category tests: framebuffer"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};

	const std::vector<VkFormat> formats = subgroups::getAllFormats();

	for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
	{
		const VkFormat format = formats[formatIndex];
		// Vector, boolean and double types are not supported by functions defined in VK_EXT_shader_subgroup_ballot.
		const bool formatTypeIsSupportedARB =
		    format == VK_FORMAT_R32_SINT || format == VK_FORMAT_R32_UINT || format == VK_FORMAT_R32_SFLOAT;

		for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
		{
			const std::string name = getOpTypeCaseName(opTypeIndex) + "_" + subgroups::getFormatNameForGLSL(format);

			{
				CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, format, de::SharedPtr<bool>(new bool), DE_FALSE, DE_FALSE};
				addFunctionCaseWithPrograms(computeGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
				caseDef.extShaderSubGroupBallotTests = DE_TRUE;
				if (formatTypeIsSupportedARB)
					addFunctionCaseWithPrograms(computeGroupARB.get(), name, "", supportedCheck, initPrograms, test, caseDef);

				caseDef.extShaderSubGroupBallotTests = DE_FALSE;
				caseDef.requiredSubgroupSize = DE_TRUE;
				addFunctionCaseWithPrograms(computeGroup.get(), name + "_requiredsubgroupsize", "", supportedCheck, initPrograms, test, caseDef);
				caseDef.extShaderSubGroupBallotTests = DE_TRUE;
				if (formatTypeIsSupportedARB)
					addFunctionCaseWithPrograms(computeGroupARB.get(), name + "_requiredsubgroupsize", "", supportedCheck, initPrograms, test, caseDef);
			}

			{
				CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_ALL_GRAPHICS, format, de::SharedPtr<bool>(new bool), DE_FALSE, DE_FALSE};
				addFunctionCaseWithPrograms(graphicGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
				caseDef.extShaderSubGroupBallotTests = DE_TRUE;
				if (formatTypeIsSupportedARB)
					addFunctionCaseWithPrograms(graphicGroupARB.get(), name, "", supportedCheck, initPrograms, test, caseDef);

			}

			for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format, de::SharedPtr<bool>(new bool), DE_FALSE, DE_FALSE};
				addFunctionCaseWithPrograms(framebufferGroup.get(), name + getShaderStageName(caseDef.shaderStage), "",
							supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
				caseDef.extShaderSubGroupBallotTests = DE_TRUE;
				if (formatTypeIsSupportedARB)
					addFunctionCaseWithPrograms(framebufferGroupARB.get(), name + getShaderStageName(caseDef.shaderStage), "",
								supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}
		}
	}

	de::MovePtr<tcu::TestCaseGroup> groupARB(new tcu::TestCaseGroup(
		testCtx, "ext_shader_subgroup_ballot", "VK_EXT_shader_subgroup_ballot category tests"));

	groupARB->addChild(graphicGroupARB.release());
	groupARB->addChild(computeGroupARB.release());
	groupARB->addChild(framebufferGroupARB.release());

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "ballot_broadcast", "Subgroup ballot broadcast category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());
	group->addChild(groupARB.release());

	return group.release();
}
} // subgroups
} // vkt
