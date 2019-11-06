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

#include "vktSubgroupsBallotTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{
static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	return vkt::subgroups::check(datas, width, 0x7);
}

static bool checkCompute(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	return vkt::subgroups::checkCompute(datas, numWorkgroups, localSize, 0x7);
}

struct CaseDefinition
{
	VkShaderStageFlags	shaderStage;
	de::SharedPtr<bool>	geometryPointSizeSupported;
};

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::SpirVAsmBuildOptions	buildOptionsSpr	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3);
	std::ostringstream				subgroupSizeStr;
	subgroupSizeStr << subgroups::maxSupportedSubgroupSize();

	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		/*
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(location = 0) in highp vec4 in_position;\n"
			"layout(location = 0) out float out_color;\n"
			"layout(set = 0, binding = 0) uniform Buffer1\n"
			"{\n"
			"  uint data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  out_color = float(tempResult);\n"
			"  gl_Position = in_position;\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";
		*/
		const string vertex =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 76\n"
			"; Schema: 0\n"
			"OpCapability Shader\n"
			"OpCapability GroupNonUniform\n"
			"OpCapability GroupNonUniformBallot\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Vertex %4 \"main\" %35 %62 %70 %72\n"
			"OpDecorate %30 ArrayStride 16\n"
			"OpMemberDecorate %31 0 Offset 0\n"
			"OpDecorate %31 Block\n"
			"OpDecorate %33 DescriptorSet 0\n"
			"OpDecorate %33 Binding 0\n"
			"OpDecorate %35 RelaxedPrecision\n"
			"OpDecorate %35 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %36 RelaxedPrecision\n"
			"OpDecorate %62 Location 0\n"
			"OpMemberDecorate %68 0 BuiltIn Position\n"
			"OpMemberDecorate %68 1 BuiltIn PointSize\n"
			"OpMemberDecorate %68 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %68 3 BuiltIn CullDistance\n"
			"OpDecorate %68 Block\n"
			"OpDecorate %72 Location 0\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 0\n"
			"%7 = OpTypePointer Function %6\n"
			"%9 = OpConstant %6 0\n"
			"%10 = OpTypeVector %6 4\n"
			"%11 = OpConstantComposite %10 %9 %9 %9 %9\n"
			"%12 = OpTypeBool\n"
			"%13 = OpConstantTrue %12\n"
			"%14 = OpConstant %6 3\n"
			"%16 = OpTypeVector %12 4\n"
			"%20 = OpTypeInt 32 1\n"
			"%21 = OpConstant %20 1\n"
			"%22 = OpConstant %20 0\n"
			"%27 = OpTypePointer Function %12\n"
			"%29 = OpConstant %6 " + subgroupSizeStr.str() + "\n"
			"%30 = OpTypeArray %6 %29\n"
			"%31 = OpTypeStruct %30\n"
			"%32 = OpTypePointer Uniform %31\n"
			"%33 = OpVariable %32 Uniform\n"
			"%34 = OpTypePointer Input %6\n"
			"%35 = OpVariable %34 Input\n"
			"%37 = OpTypePointer Uniform %6\n"
			"%46 = OpConstant %20 2\n"
			"%51 = OpConstantFalse %12\n"
			"%55 = OpConstant %20 4\n"
			"%60 = OpTypeFloat 32\n"
			"%61 = OpTypePointer Output %60\n"
			"%62 = OpVariable %61 Output\n"
			"%65 = OpTypeVector %60 4\n"
			"%66 = OpConstant %6 1\n"
			"%67 = OpTypeArray %60 %66\n"
			"%68 = OpTypeStruct %65 %60 %67 %67\n"
			"%69 = OpTypePointer Output %68\n"
			"%70 = OpVariable %69 Output\n"
			"%71 = OpTypePointer Input %65\n"
			"%72 = OpVariable %71 Input\n"
			"%74 = OpTypePointer Output %65\n"
			"%76 = OpConstant %60 1\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%8 = OpVariable %7 Function\n"
			"%28 = OpVariable %27 Function\n"
			"OpStore %8 %9\n"
			"%15 = OpGroupNonUniformBallot %10 %14 %13\n"
			"%17 = OpIEqual %16 %11 %15\n"
			"%18 = OpAll %12 %17\n"
			"%19 = OpLogicalNot %12 %18\n"
			"%23 = OpSelect %20 %19 %21 %22\n"
			"%24 = OpBitcast %6 %23\n"
			"%25 = OpLoad %6 %8\n"
			"%26 = OpBitwiseOr %6 %25 %24\n"
			"OpStore %8 %26\n"
			"%36 = OpLoad %6 %35\n"
			"%38 = OpAccessChain %37 %33 %22 %36\n"
			"%39 = OpLoad %6 %38\n"
			"%40 = OpINotEqual %12 %39 %9\n"
			"OpStore %28 %40\n"
			"%41 = OpLoad %12 %28\n"
			"%42 = OpGroupNonUniformBallot %10 %14 %41\n"
			"%43 = OpIEqual %16 %11 %42\n"
			"%44 = OpAll %12 %43\n"
			"%45 = OpLogicalNot %12 %44\n"
			"%47 = OpSelect %20 %45 %46 %22\n"
			"%48 = OpBitcast %6 %47\n"
			"%49 = OpLoad %6 %8\n"
			"%50 = OpBitwiseOr %6 %49 %48\n"
			"OpStore %8 %50\n"
			"%52 = OpGroupNonUniformBallot %10 %14 %51\n"
			"%53 = OpIEqual %16 %11 %52\n"
			"%54 = OpAll %12 %53\n"
			"%56 = OpSelect %20 %54 %55 %22\n"
			"%57 = OpBitcast %6 %56\n"
			"%58 = OpLoad %6 %8\n"
			"%59 = OpBitwiseOr %6 %58 %57\n"
			"OpStore %8 %59\n"
			"%63 = OpLoad %6 %8\n"
			"%64 = OpConvertUToF %60 %63\n"
			"OpStore %62 %64\n"
			"%73 = OpLoad %65 %72\n"
			"%75 = OpAccessChain %74 %70 %22\n"
			"OpStore %75 %73\n"
			"%77 = OpAccessChain %61 %70 %21\n"
			"OpStore %77 %76\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("vert") << vertex << buildOptionsSpr;
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		/*
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(points) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			"layout(location = 0) out float out_color;\n"
			"layout(set = 0, binding = 0) uniform Buffer1\n"
			"{\n"
			"  uint data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  out_color = float(tempResult);\n"
			"  gl_Position = gl_in[0].gl_Position;\n"
			"  gl_PointSize = gl_in[0].gl_PointSize;\n"
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n";
		*/
		std::ostringstream geometry;
		geometry
			<< "; SPIR-V\n"
			<< "; Version: 1.3\n"
			<< "; Generator: Khronos Glslang Reference Front End; 2\n"
			<< "; Bound: 80\n"
			<< "; Schema: 0\n"
			<< "OpCapability Geometry\n"
			<< (*caseDef.geometryPointSizeSupported ? "OpCapability GeometryPointSize\n" : "")
			<< "OpCapability GroupNonUniform\n"
			<< "OpCapability GroupNonUniformBallot\n"
			<< "%1 = OpExtInstImport \"GLSL.std.450\"\n"
			<< "OpMemoryModel Logical GLSL450\n"
			<< "OpEntryPoint Geometry %4 \"main\" %35 %62 %70 %74\n"
			<< "OpExecutionMode %4 InputPoints\n"
			<< "OpExecutionMode %4 Invocations 1\n"
			<< "OpExecutionMode %4 OutputPoints\n"
			<< "OpExecutionMode %4 OutputVertices 1\n"
			<< "OpDecorate %30 ArrayStride 16\n"
			<< "OpMemberDecorate %31 0 Offset 0\n"
			<< "OpDecorate %31 Block\n"
			<< "OpDecorate %33 DescriptorSet 0\n"
			<< "OpDecorate %33 Binding 0\n"
			<< "OpDecorate %35 RelaxedPrecision\n"
			<< "OpDecorate %35 BuiltIn SubgroupLocalInvocationId\n"
			<< "OpDecorate %36 RelaxedPrecision\n"
			<< "OpDecorate %62 Location 0\n"
			<< "OpMemberDecorate %68 0 BuiltIn Position\n"
			<< "OpMemberDecorate %68 1 BuiltIn PointSize\n"
			<< "OpMemberDecorate %68 2 BuiltIn ClipDistance\n"
			<< "OpMemberDecorate %68 3 BuiltIn CullDistance\n"
			<< "OpDecorate %68 Block\n"
			<< "OpMemberDecorate %71 0 BuiltIn Position\n"
			<< "OpMemberDecorate %71 1 BuiltIn PointSize\n"
			<< "OpMemberDecorate %71 2 BuiltIn ClipDistance\n"
			<< "OpMemberDecorate %71 3 BuiltIn CullDistance\n"
			<< "OpDecorate %71 Block\n"
			<< "%2 = OpTypeVoid\n"
			<< "%3 = OpTypeFunction %2\n"
			<< "%6 = OpTypeInt 32 0\n"
			<< "%7 = OpTypePointer Function %6\n"
			<< "%9 = OpConstant %6 0\n"
			<< "%10 = OpTypeVector %6 4\n"
			<< "%11 = OpConstantComposite %10 %9 %9 %9 %9\n"
			<< "%12 = OpTypeBool\n"
			<< "%13 = OpConstantTrue %12\n"
			<< "%14 = OpConstant %6 3\n"
			<< "%16 = OpTypeVector %12 4\n"
			<< "%20 = OpTypeInt 32 1\n"
			<< "%21 = OpConstant %20 1\n"
			<< "%22 = OpConstant %20 0\n"
			<< "%27 = OpTypePointer Function %12\n"
			<< "%29 = OpConstant %6 " << subgroupSizeStr.str() << "\n"
			<< "%30 = OpTypeArray %6 %29\n"
			<< "%31 = OpTypeStruct %30\n"
			<< "%32 = OpTypePointer Uniform %31\n"
			<< "%33 = OpVariable %32 Uniform\n"
			<< "%34 = OpTypePointer Input %6\n"
			<< "%35 = OpVariable %34 Input\n"
			<< "%37 = OpTypePointer Uniform %6\n"
			<< "%46 = OpConstant %20 2\n"
			<< "%51 = OpConstantFalse %12\n"
			<< "%55 = OpConstant %20 4\n"
			<< "%60 = OpTypeFloat 32\n"
			<< "%61 = OpTypePointer Output %60\n"
			<< "%62 = OpVariable %61 Output\n"
			<< "%65 = OpTypeVector %60 4\n"
			<< "%66 = OpConstant %6 1\n"
			<< "%67 = OpTypeArray %60 %66\n"
			<< "%68 = OpTypeStruct %65 %60 %67 %67\n"
			<< "%69 = OpTypePointer Output %68\n"
			<< "%70 = OpVariable %69 Output\n"
			<< "%71 = OpTypeStruct %65 %60 %67 %67\n"
			<< "%72 = OpTypeArray %71 %66\n"
			<< "%73 = OpTypePointer Input %72\n"
			<< "%74 = OpVariable %73 Input\n"
			<< "%75 = OpTypePointer Input %65\n"
			<< "%78 = OpTypePointer Output %65\n"
			<< (*caseDef.geometryPointSizeSupported ?
				"%80 = OpTypePointer Input %60\n"
				"%81 = OpTypePointer Output %60\n" : "")
			<< "%4 = OpFunction %2 None %3\n"
			<< "%5 = OpLabel\n"
			<< "%8 = OpVariable %7 Function\n"
			<< "%28 = OpVariable %27 Function\n"
			<< "OpStore %8 %9\n"
			<< "%15 = OpGroupNonUniformBallot %10 %14 %13\n"
			<< "%17 = OpIEqual %16 %11 %15\n"
			<< "%18 = OpAll %12 %17\n"
			<< "%19 = OpLogicalNot %12 %18\n"
			<< "%23 = OpSelect %20 %19 %21 %22\n"
			<< "%24 = OpBitcast %6 %23\n"
			<< "%25 = OpLoad %6 %8\n"
			<< "%26 = OpBitwiseOr %6 %25 %24\n"
			<< "OpStore %8 %26\n"
			<< "%36 = OpLoad %6 %35\n"
			<< "%38 = OpAccessChain %37 %33 %22 %36\n"
			<< "%39 = OpLoad %6 %38\n"
			<< "%40 = OpINotEqual %12 %39 %9\n"
			<< "OpStore %28 %40\n"
			<< "%41 = OpLoad %12 %28\n"
			<< "%42 = OpGroupNonUniformBallot %10 %14 %41\n"
			<< "%43 = OpIEqual %16 %11 %42\n"
			<< "%44 = OpAll %12 %43\n"
			<< "%45 = OpLogicalNot %12 %44\n"
			<< "%47 = OpSelect %20 %45 %46 %22\n"
			<< "%48 = OpBitcast %6 %47\n"
			<< "%49 = OpLoad %6 %8\n"
			<< "%50 = OpBitwiseOr %6 %49 %48\n"
			<< "OpStore %8 %50\n"
			<< "%52 = OpGroupNonUniformBallot %10 %14 %51\n"
			<< "%53 = OpIEqual %16 %11 %52\n"
			<< "%54 = OpAll %12 %53\n"
			<< "%56 = OpSelect %20 %54 %55 %22\n"
			<< "%57 = OpBitcast %6 %56\n"
			<< "%58 = OpLoad %6 %8\n"
			<< "%59 = OpBitwiseOr %6 %58 %57\n"
			<< "OpStore %8 %59\n"
			<< "%63 = OpLoad %6 %8\n"
			<< "%64 = OpConvertUToF %60 %63\n"
			<< "OpStore %62 %64\n"
			<< "%76 = OpAccessChain %75 %74 %22 %22\n"
			<< "%77 = OpLoad %65 %76\n"
			<< "%79 = OpAccessChain %78 %70 %22\n"
			<< "OpStore %79 %77\n"
			<< (*caseDef.geometryPointSizeSupported ?
				"%82 = OpAccessChain %80 %74 %22 %21\n"
				"%83 = OpLoad %60 %82\n"
				"%84 = OpAccessChain %81 %70 %21\n"
				"OpStore %84 %83\n" : "")
			<< "OpEmitVertex\n"
			<< "OpEndPrimitive\n"
			<< "OpReturn\n"
			<< "OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("geometry") << geometry.str() << buildOptionsSpr;
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		/*
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(vertices = 2) out;\n"
			"layout(location = 0) out float out_color[];\n"
			"layout(set = 0, binding = 0) uniform Buffer1\n"
			"{\n"
			"  uint data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  if (gl_InvocationID == 0)\n"
			  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  out_color[gl_InvocationID] = float(tempResult);\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";
		*/
		const string controlSource =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 102\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"OpCapability GroupNonUniform\n"
			"OpCapability GroupNonUniformBallot\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationControl %4 \"main\" %8 %20 %50 %78 %89 %95\n"
			"OpExecutionMode %4 OutputVertices 2\n"
			"OpDecorate %8 BuiltIn InvocationId\n"
			"OpDecorate %20 Patch\n"
			"OpDecorate %20 BuiltIn TessLevelOuter\n"
			"OpDecorate %45 ArrayStride 16\n"
			"OpMemberDecorate %46 0 Offset 0\n"
			"OpDecorate %46 Block\n"
			"OpDecorate %48 DescriptorSet 0\n"
			"OpDecorate %48 Binding 0\n"
			"OpDecorate %50 RelaxedPrecision\n"
			"OpDecorate %50 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %51 RelaxedPrecision\n"
			"OpDecorate %78 Location 0\n"
			"OpMemberDecorate %86 0 BuiltIn Position\n"
			"OpMemberDecorate %86 1 BuiltIn PointSize\n"
			"OpMemberDecorate %86 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %86 3 BuiltIn CullDistance\n"
			"OpDecorate %86 Block\n"
			"OpMemberDecorate %91 0 BuiltIn Position\n"
			"OpMemberDecorate %91 1 BuiltIn PointSize\n"
			"OpMemberDecorate %91 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %91 3 BuiltIn CullDistance\n"
			"OpDecorate %91 Block\n"
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
			"%26 = OpTypePointer Function %16\n"
			"%28 = OpConstant %16 0\n"
			"%29 = OpTypeVector %16 4\n"
			"%30 = OpConstantComposite %29 %28 %28 %28 %28\n"
			"%31 = OpConstantTrue %11\n"
			"%32 = OpConstant %16 3\n"
			"%34 = OpTypeVector %11 4\n"
			"%42 = OpTypePointer Function %11\n"
			"%44 = OpConstant %16 " + subgroupSizeStr.str() + "\n"
			"%45 = OpTypeArray %16 %44\n"
			"%46 = OpTypeStruct %45\n"
			"%47 = OpTypePointer Uniform %46\n"
			"%48 = OpVariable %47 Uniform\n"
			"%49 = OpTypePointer Input %16\n"
			"%50 = OpVariable %49 Input\n"
			"%52 = OpTypePointer Uniform %16\n"
			"%61 = OpConstant %6 2\n"
			"%66 = OpConstantFalse %11\n"
			"%70 = OpConstant %6 4\n"
			"%75 = OpConstant %16 2\n"
			"%76 = OpTypeArray %15 %75\n"
			"%77 = OpTypePointer Output %76\n"
			"%78 = OpVariable %77 Output\n"
			"%83 = OpTypeVector %15 4\n"
			"%84 = OpConstant %16 1\n"
			"%85 = OpTypeArray %15 %84\n"
			"%86 = OpTypeStruct %83 %15 %85 %85\n"
			"%87 = OpTypeArray %86 %75\n"
			"%88 = OpTypePointer Output %87\n"
			"%89 = OpVariable %88 Output\n"
			"%91 = OpTypeStruct %83 %15 %85 %85\n"
			"%92 = OpConstant %16 32\n"
			"%93 = OpTypeArray %91 %92\n"
			"%94 = OpTypePointer Input %93\n"
			"%95 = OpVariable %94 Input\n"
			"%97 = OpTypePointer Input %83\n"
			"%100 = OpTypePointer Output %83\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%27 = OpVariable %26 Function\n"
			"%43 = OpVariable %42 Function\n"
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
			"OpStore %27 %28\n"
			"%33 = OpGroupNonUniformBallot %29 %32 %31\n"
			"%35 = OpIEqual %34 %30 %33\n"
			"%36 = OpAll %11 %35\n"
			"%37 = OpLogicalNot %11 %36\n"
			"%38 = OpSelect %6 %37 %24 %10\n"
			"%39 = OpBitcast %16 %38\n"
			"%40 = OpLoad %16 %27\n"
			"%41 = OpBitwiseOr %16 %40 %39\n"
			"OpStore %27 %41\n"
			"%51 = OpLoad %16 %50\n"
			"%53 = OpAccessChain %52 %48 %10 %51\n"
			"%54 = OpLoad %16 %53\n"
			"%55 = OpINotEqual %11 %54 %28\n"
			"OpStore %43 %55\n"
			"%56 = OpLoad %11 %43\n"
			"%57 = OpGroupNonUniformBallot %29 %32 %56\n"
			"%58 = OpIEqual %34 %30 %57\n"
			"%59 = OpAll %11 %58\n"
			"%60 = OpLogicalNot %11 %59\n"
			"%62 = OpSelect %6 %60 %61 %10\n"
			"%63 = OpBitcast %16 %62\n"
			"%64 = OpLoad %16 %27\n"
			"%65 = OpBitwiseOr %16 %64 %63\n"
			"OpStore %27 %65\n"
			"%67 = OpGroupNonUniformBallot %29 %32 %66\n"
			"%68 = OpIEqual %34 %30 %67\n"
			"%69 = OpAll %11 %68\n"
			"%71 = OpSelect %6 %69 %70 %10\n"
			"%72 = OpBitcast %16 %71\n"
			"%73 = OpLoad %16 %27\n"
			"%74 = OpBitwiseOr %16 %73 %72\n"
			"OpStore %27 %74\n"
			"%79 = OpLoad %6 %8\n"
			"%80 = OpLoad %16 %27\n"
			"%81 = OpConvertUToF %15 %80\n"
			"%82 = OpAccessChain %22 %78 %79\n"
			"OpStore %82 %81\n"
			"%90 = OpLoad %6 %8\n"
			"%96 = OpLoad %6 %8\n"
			"%98 = OpAccessChain %97 %95 %96 %10\n"
			"%99 = OpLoad %83 %98\n"
			"%101 = OpAccessChain %100 %89 %90 %10\n"
			"OpStore %101 %99\n"
			"OpReturn\n"
			"OpFunctionEnd\n";

		programCollection.spirvAsmSources.add("tesc") << controlSource << buildOptionsSpr;
		subgroups::setTesEvalShaderFrameBuffer(programCollection);

	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		/*
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(isolines, equal_spacing, ccw ) in;\n"
			"layout(location = 0) out float out_color;\n"
			"layout(set = 0, binding = 0) uniform Buffer1\n"
			"{\n"
			"  uint data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  out_color = float(tempResult);\n"
			"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			"}\n";
		*/
		const string evaluationSource =
			"; SPIR-V\n"
			"; Version: 1.3\n"
			"; Generator: Khronos Glslang Reference Front End; 2\n"
			"; Bound: 91\n"
			"; Schema: 0\n"
			"OpCapability Tessellation\n"
			"OpCapability GroupNonUniform\n"
			"OpCapability GroupNonUniformBallot\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint TessellationEvaluation %4 \"main\" %35 %62 %70 %75 %83\n"
			"OpExecutionMode %4 Isolines\n"
			"OpExecutionMode %4 SpacingEqual\n"
			"OpExecutionMode %4 VertexOrderCcw\n"
			"OpDecorate %30 ArrayStride 16\n"
			"OpMemberDecorate %31 0 Offset 0\n"
			"OpDecorate %31 Block\n"
			"OpDecorate %33 DescriptorSet 0\n"
			"OpDecorate %33 Binding 0\n"
			"OpDecorate %35 RelaxedPrecision\n"
			"OpDecorate %35 BuiltIn SubgroupLocalInvocationId\n"
			"OpDecorate %36 RelaxedPrecision\n"
			"OpDecorate %62 Location 0\n"
			"OpMemberDecorate %68 0 BuiltIn Position\n"
			"OpMemberDecorate %68 1 BuiltIn PointSize\n"
			"OpMemberDecorate %68 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %68 3 BuiltIn CullDistance\n"
			"OpDecorate %68 Block\n"
			"OpMemberDecorate %71 0 BuiltIn Position\n"
			"OpMemberDecorate %71 1 BuiltIn PointSize\n"
			"OpMemberDecorate %71 2 BuiltIn ClipDistance\n"
			"OpMemberDecorate %71 3 BuiltIn CullDistance\n"
			"OpDecorate %71 Block\n"
			"OpDecorate %83 BuiltIn TessCoord\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 0\n"
			"%7 = OpTypePointer Function %6\n"
			"%9 = OpConstant %6 0\n"
			"%10 = OpTypeVector %6 4\n"
			"%11 = OpConstantComposite %10 %9 %9 %9 %9\n"
			"%12 = OpTypeBool\n"
			"%13 = OpConstantTrue %12\n"
			"%14 = OpConstant %6 3\n"
			"%16 = OpTypeVector %12 4\n"
			"%20 = OpTypeInt 32 1\n"
			"%21 = OpConstant %20 1\n"
			"%22 = OpConstant %20 0\n"
			"%27 = OpTypePointer Function %12\n"
			"%29 = OpConstant %6 " + subgroupSizeStr.str() + "\n"
			"%30 = OpTypeArray %6 %29\n"
			"%31 = OpTypeStruct %30\n"
			"%32 = OpTypePointer Uniform %31\n"
			"%33 = OpVariable %32 Uniform\n"
			"%34 = OpTypePointer Input %6\n"
			"%35 = OpVariable %34 Input\n"
			"%37 = OpTypePointer Uniform %6\n"
			"%46 = OpConstant %20 2\n"
			"%51 = OpConstantFalse %12\n"
			"%55 = OpConstant %20 4\n"
			"%60 = OpTypeFloat 32\n"
			"%61 = OpTypePointer Output %60\n"
			"%62 = OpVariable %61 Output\n"
			"%65 = OpTypeVector %60 4\n"
			"%66 = OpConstant %6 1\n"
			"%67 = OpTypeArray %60 %66\n"
			"%68 = OpTypeStruct %65 %60 %67 %67\n"
			"%69 = OpTypePointer Output %68\n"
			"%70 = OpVariable %69 Output\n"
			"%71 = OpTypeStruct %65 %60 %67 %67\n"
			"%72 = OpConstant %6 32\n"
			"%73 = OpTypeArray %71 %72\n"
			"%74 = OpTypePointer Input %73\n"
			"%75 = OpVariable %74 Input\n"
			"%76 = OpTypePointer Input %65\n"
			"%81 = OpTypeVector %60 3\n"
			"%82 = OpTypePointer Input %81\n"
			"%83 = OpVariable %82 Input\n"
			"%84 = OpTypePointer Input %60\n"
			"%89 = OpTypePointer Output %65\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%8 = OpVariable %7 Function\n"
			"%28 = OpVariable %27 Function\n"
			"OpStore %8 %9\n"
			"%15 = OpGroupNonUniformBallot %10 %14 %13\n"
			"%17 = OpIEqual %16 %11 %15\n"
			"%18 = OpAll %12 %17\n"
			"%19 = OpLogicalNot %12 %18\n"
			"%23 = OpSelect %20 %19 %21 %22\n"
			"%24 = OpBitcast %6 %23\n"
			"%25 = OpLoad %6 %8\n"
			"%26 = OpBitwiseOr %6 %25 %24\n"
			"OpStore %8 %26\n"
			"%36 = OpLoad %6 %35\n"
			"%38 = OpAccessChain %37 %33 %22 %36\n"
			"%39 = OpLoad %6 %38\n"
			"%40 = OpINotEqual %12 %39 %9\n"
			"OpStore %28 %40\n"
			"%41 = OpLoad %12 %28\n"
			"%42 = OpGroupNonUniformBallot %10 %14 %41\n"
			"%43 = OpIEqual %16 %11 %42\n"
			"%44 = OpAll %12 %43\n"
			"%45 = OpLogicalNot %12 %44\n"
			"%47 = OpSelect %20 %45 %46 %22\n"
			"%48 = OpBitcast %6 %47\n"
			"%49 = OpLoad %6 %8\n"
			"%50 = OpBitwiseOr %6 %49 %48\n"
			"OpStore %8 %50\n"
			"%52 = OpGroupNonUniformBallot %10 %14 %51\n"
			"%53 = OpIEqual %16 %11 %52\n"
			"%54 = OpAll %12 %53\n"
			"%56 = OpSelect %20 %54 %55 %22\n"
			"%57 = OpBitcast %6 %56\n"
			"%58 = OpLoad %6 %8\n"
			"%59 = OpBitwiseOr %6 %58 %57\n"
			"OpStore %8 %59\n"
			"%63 = OpLoad %6 %8\n"
			"%64 = OpConvertUToF %60 %63\n"
			"OpStore %62 %64\n"
			"%77 = OpAccessChain %76 %75 %22 %22\n"
			"%78 = OpLoad %65 %77\n"
			"%79 = OpAccessChain %76 %75 %21 %22\n"
			"%80 = OpLoad %65 %79\n"
			"%85 = OpAccessChain %84 %83 %9\n"
			"%86 = OpLoad %60 %85\n"
			"%87 = OpCompositeConstruct %65 %86 %86 %86 %86\n"
			"%88 = OpExtInst %65 %1 FMix %78 %80 %87\n"
			"%90 = OpAccessChain %89 %70 %22\n"
			"OpStore %90 %88\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		subgroups::setTesCtrlShaderFrameBuffer(programCollection);
		programCollection.spirvAsmSources.add("tese") << evaluationSource << buildOptionsSpr;
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}


void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
			"local_size_z_id = 2) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  uint data[];\n"
			<< "};\n"
			<< "\n"
			<< subgroups::getSharedMemoryBallotHelper()
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< "  uint tempResult = 0;\n"
			<< "  tempResult |= sharedMemoryBallot(true) == subgroupBallot(true) ? 0x1 : 0;\n"
			<< "  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			<< "  tempResult |= sharedMemoryBallot(bData) == subgroupBallot(bData) ? 0x2 : 0;\n"
			<< "  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			<< "  result[offset] = tempResult;\n"
			<< "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		const string vertex =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  result[gl_VertexIndex] = tempResult;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
			"  gl_Position = vec4(float(gl_VertexIndex) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";

		const string tesc =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(vertices=1) out;\n"
			"layout(set = 0, binding = 1, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  result[gl_PrimitiveID] = tempResult;\n"
			"  if (gl_InvocationID == 0)\n"
			"  {\n"
			"    gl_TessLevelOuter[0] = 1.0f;\n"
			"    gl_TessLevelOuter[1] = 1.0f;\n"
			"  }\n"
			"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			"}\n";

		const string tese =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(isolines) in;\n"
			"layout(set = 0, binding = 2, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempResult;\n"
			"  float pixelSize = 2.0f/1024.0f;\n"
			"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
			"}\n";

		const string geometry =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(${TOPOLOGY}) in;\n"
			"layout(points, max_vertices = 1) out;\n"
			"layout(set = 0, binding = 3, std430) buffer Buffer1\n"
			"{\n"
			"  uint result[];\n"
			"};\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  result[gl_PrimitiveIDIn] = tempResult;\n"
			"  gl_Position = gl_in[0].gl_Position;\n"
			"  EmitVertex();\n"
			"  EndPrimitive();\n"
			"}\n";

		const string fragment =
			"#version 450\n"
			"#extension GL_KHR_shader_subgroup_ballot: enable\n"
			"layout(location = 0) out uint result;\n"
			"layout(set = 0, binding = 4, std430) readonly buffer Buffer1\n"
			"{\n"
			"  uint data[];\n"
			"};\n"
			"void main (void)\n"
			"{\n"
			"  uint tempResult = 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(true)) ? 0x1 : 0;\n"
			"  bool bData = data[gl_SubgroupInvocationID] != 0;\n"
			"  tempResult |= !bool(uvec4(0) == subgroupBallot(bData)) ? 0x2 : 0;\n"
			"  tempResult |= uvec4(0) == subgroupBallot(false) ? 0x4 : 0;\n"
			"  result = tempResult;\n"
			"}\n";

		subgroups::addNoSubgroupShader(programCollection);

		programCollection.glslSources.add("vert")
				<< glu::VertexSource(vertex) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		programCollection.glslSources.add("tesc")
				<< glu::TessellationControlSource(tesc) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		subgroups::addGeometryShadersFromTemplate(geometry, vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u),
												  programCollection.glslSources);
		programCollection.glslSources.add("fragment")
				<< glu::FragmentSource(fragment)<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	DE_UNREF(caseDef);
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_BALLOT_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup ballot operations");
	}

	*caseDef.geometryPointSizeSupported = subgroups::isTessellationAndGeometryPointSizeSupported(context);
}

tcu::TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::areSubgroupOperationsSupportedForStage(
			context, caseDef.shaderStage))
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

	subgroups::SSBOData inputData[1];
	inputData[0].format = VK_FORMAT_R32_UINT;
	inputData[0].layout = subgroups::SSBOData::LayoutStd140;
	inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
	inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, inputData, 1, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
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
		subgroups::SSBOData inputData[1];
		inputData[0].format = VK_FORMAT_R32_UINT;
		inputData[0].layout = subgroups::SSBOData::LayoutStd430;
		inputData[0].numElements = subgroups::maxSupportedSubgroupSize();
		inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, inputData, 1, checkCompute);
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

		if ( VK_SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
		{
			if ( (stages & VK_SHADER_STAGE_FRAGMENT_BIT) == 0)
				TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
			else
				stages = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		if ((VkShaderStageFlagBits)0u == stages)
			TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

		subgroups::SSBOData inputData;
		inputData.format			= VK_FORMAT_R32_UINT;
		inputData.layout            = subgroups::SSBOData::LayoutStd430;
		inputData.numElements		= subgroups::maxSupportedSubgroupSize();
		inputData.initializeType	= subgroups::SSBOData::InitializeNonZero;
		inputData.binding			= 4u;
		inputData.stages			= stages;

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, stages);
	}
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsBallotTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup ballot category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup ballot category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup ballot category tests: framebuffer"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_VERTEX_BIT
	};

	{
		const CaseDefinition caseDef = {VK_SHADER_STAGE_COMPUTE_BIT, de::SharedPtr<bool>(new bool)};
		addFunctionCaseWithPrograms(computeGroup.get(), getShaderStageName(caseDef.shaderStage), "", supportedCheck, initPrograms, test, caseDef);
	}

	{
		const CaseDefinition caseDef = {VK_SHADER_STAGE_ALL_GRAPHICS, de::SharedPtr<bool>(new bool)};
		addFunctionCaseWithPrograms(graphicGroup.get(), "graphic", "", supportedCheck, initPrograms, test, caseDef);
	}

	for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
	{
		const CaseDefinition caseDef = {stages[stageIndex],de::SharedPtr<bool>(new bool)};
		addFunctionCaseWithPrograms(framebufferGroup.get(), getShaderStageName(caseDef.shaderStage), "",
					supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
	}

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "ballot", "Subgroup ballot category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // vkt
