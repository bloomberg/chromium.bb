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

#include "SpirvShader.hpp"

#include "SpirvProfiler.hpp"
#include "SpirvShaderDebug.hpp"

#include "System/Debug.hpp"
#include "Vulkan/VkPipelineLayout.hpp"
#include "Vulkan/VkRenderPass.hpp"

#include "marl/defer.h"

#include <spirv/unified1/spirv.hpp>

namespace sw {

SpirvShader::SpirvShader(
    VkShaderStageFlagBits pipelineStage,
    const char *entryPointName,
    SpirvBinary const &insns,
    const vk::RenderPass *renderPass,
    uint32_t subpassIndex,
    bool robustBufferAccess,
    const std::shared_ptr<vk::dbg::Context> &dbgctx,
    std::shared_ptr<SpirvProfiler> profiler)
    : insns{ insns }
    , inputs{ MAX_INTERFACE_COMPONENTS }
    , outputs{ MAX_INTERFACE_COMPONENTS }
    , robustBufferAccess(robustBufferAccess)
    , profiler(profiler)
{
	ASSERT(insns.size() > 0);

	if(dbgctx)
	{
		dbgInit(dbgctx);
	}

	if(renderPass)
	{
		// capture formats of any input attachments present
		auto subpass = renderPass->getSubpass(subpassIndex);
		inputAttachmentFormats.reserve(subpass.inputAttachmentCount);
		for(auto i = 0u; i < subpass.inputAttachmentCount; i++)
		{
			auto attachmentIndex = subpass.pInputAttachments[i].attachment;
			inputAttachmentFormats.push_back(attachmentIndex != VK_ATTACHMENT_UNUSED
			                                     ? renderPass->getAttachment(attachmentIndex).format
			                                     : VK_FORMAT_UNDEFINED);
		}
	}

	// The identifiers of all OpVariables that define the entry point's IO variables.
	std::unordered_set<Object::ID> interfaceIds;

	Function::ID currentFunction;
	Block::ID currentBlock;
	InsnIterator blockStart;

	for(auto insn : *this)
	{
		spv::Op opcode = insn.opcode();

		switch(opcode)
		{
		case spv::OpEntryPoint:
			{
				spv::ExecutionModel executionModel = spv::ExecutionModel(insn.word(1));
				Function::ID entryPoint = Function::ID(insn.word(2));
				const char *name = insn.string(3);
				VkShaderStageFlagBits stage = executionModelToStage(executionModel);

				if(stage == pipelineStage && strcmp(name, entryPointName) == 0)
				{
					ASSERT_MSG(this->entryPoint == 0, "Duplicate entry point with name '%s' and stage %d", name, int(stage));
					this->entryPoint = entryPoint;
					this->executionModel = executionModel;

					auto interfaceIdsOffset = 3 + insn.stringSizeInWords(3);
					for(uint32_t i = interfaceIdsOffset; i < insn.wordCount(); i++)
					{
						interfaceIds.emplace(insn.word(i));
					}
				}
			}
			break;

		case spv::OpExecutionMode:
		case spv::OpExecutionModeId:
			ProcessExecutionMode(insn);
			break;

		case spv::OpDecorate:
			{
				TypeOrObjectID targetId = insn.word(1);
				auto decoration = static_cast<spv::Decoration>(insn.word(2));
				uint32_t value = insn.wordCount() > 3 ? insn.word(3) : 0;

				decorations[targetId].Apply(decoration, value);

				switch(decoration)
				{
				case spv::DecorationDescriptorSet:
					descriptorDecorations[targetId].DescriptorSet = value;
					break;
				case spv::DecorationBinding:
					descriptorDecorations[targetId].Binding = value;
					break;
				case spv::DecorationInputAttachmentIndex:
					descriptorDecorations[targetId].InputAttachmentIndex = value;
					break;
				case spv::DecorationSample:
					analysis.ContainsSampleQualifier = true;
					break;
				default:
					// Only handling descriptor decorations here.
					break;
				}

				if(decoration == spv::DecorationCentroid)
				{
					analysis.NeedsCentroid = true;
				}
			}
			break;

		case spv::OpMemberDecorate:
			{
				Type::ID targetId = insn.word(1);
				auto memberIndex = insn.word(2);
				auto decoration = static_cast<spv::Decoration>(insn.word(3));
				uint32_t value = insn.wordCount() > 4 ? insn.word(4) : 0;

				auto &d = memberDecorations[targetId];
				if(memberIndex >= d.size())
					d.resize(memberIndex + 1);  // on demand; exact size would require another pass...

				d[memberIndex].Apply(decoration, value);

				if(decoration == spv::DecorationCentroid)
				{
					analysis.NeedsCentroid = true;
				}
			}
			break;

		case spv::OpDecorateId:
			{
				auto decoration = static_cast<spv::Decoration>(insn.word(2));

				// Currently OpDecorateId only supports UniformId, which provides information for
				// potential optimizations that we don't perform, and CounterBuffer, which is used
				// by HLSL to build the graphics pipeline with shader reflection. At the driver level,
				// the CounterBuffer decoration does nothing, so we can safely ignore both decorations.
				ASSERT(decoration == spv::DecorationUniformId || decoration == spv::DecorationCounterBuffer);
			}
			break;

		case spv::OpDecorateString:
			{
				auto decoration = static_cast<spv::Decoration>(insn.word(2));

				// We assume these are for HLSL semantics, ignore them (b/214576937).
				ASSERT(decoration == spv::DecorationUserSemantic || decoration == spv::DecorationUserTypeGOOGLE);
			}
			break;

		case spv::OpMemberDecorateString:
			{
				auto decoration = static_cast<spv::Decoration>(insn.word(3));

				// We assume these are for HLSL semantics, ignore them (b/214576937).
				ASSERT(decoration == spv::DecorationUserSemantic || decoration == spv::DecorationUserTypeGOOGLE);
			}
			break;

		case spv::OpDecorationGroup:
			// Nothing to do here. We don't need to record the definition of the group; we'll just have
			// the bundle of decorations float around. If we were to ever walk the decorations directly,
			// we might think about introducing this as a real Object.
			break;

		case spv::OpGroupDecorate:
			{
				uint32_t group = insn.word(1);
				auto const &groupDecorations = decorations[group];
				auto const &descriptorGroupDecorations = descriptorDecorations[group];
				for(auto i = 2u; i < insn.wordCount(); i++)
				{
					// Remaining operands are targets to apply the group to.
					uint32_t target = insn.word(i);
					decorations[target].Apply(groupDecorations);
					descriptorDecorations[target].Apply(descriptorGroupDecorations);
				}
			}
			break;

		case spv::OpGroupMemberDecorate:
			{
				auto const &srcDecorations = decorations[insn.word(1)];
				for(auto i = 2u; i < insn.wordCount(); i += 2)
				{
					// remaining operands are pairs of <id>, literal for members to apply to.
					auto &d = memberDecorations[insn.word(i)];
					auto memberIndex = insn.word(i + 1);
					if(memberIndex >= d.size())
						d.resize(memberIndex + 1);  // on demand resize, see above...
					d[memberIndex].Apply(srcDecorations);
				}
			}
			break;

		case spv::OpLabel:
			{
				ASSERT(currentBlock == 0);
				currentBlock = Block::ID(insn.word(1));
				blockStart = insn;
			}
			break;

		// Termination instructions:
		case spv::OpKill:
		case spv::OpTerminateInvocation:
			analysis.ContainsDiscard = true;
			// [[fallthrough]]

		case spv::OpUnreachable:

		// Branch Instructions (subset of Termination Instructions):
		case spv::OpBranch:
		case spv::OpBranchConditional:
		case spv::OpSwitch:
		case spv::OpReturn:
			{
				ASSERT(currentBlock != 0);
				ASSERT(currentFunction != 0);

				auto blockEnd = insn;
				blockEnd++;
				functions[currentFunction].blocks[currentBlock] = Block(blockStart, blockEnd);
				currentBlock = Block::ID(0);
			}
			break;

		case spv::OpDemoteToHelperInvocation:
			analysis.ContainsDiscard = true;
			break;

		case spv::OpLoopMerge:
		case spv::OpSelectionMerge:
			break;  // Nothing to do in analysis pass.

		case spv::OpTypeVoid:
		case spv::OpTypeBool:
		case spv::OpTypeInt:
		case spv::OpTypeFloat:
		case spv::OpTypeVector:
		case spv::OpTypeMatrix:
		case spv::OpTypeImage:
		case spv::OpTypeSampler:
		case spv::OpTypeSampledImage:
		case spv::OpTypeArray:
		case spv::OpTypeRuntimeArray:
		case spv::OpTypeStruct:
		case spv::OpTypePointer:
		case spv::OpTypeFunction:
			DeclareType(insn);
			break;

		case spv::OpVariable:
			{
				Type::ID typeId = insn.word(1);
				Object::ID resultId = insn.word(2);
				auto storageClass = static_cast<spv::StorageClass>(insn.word(3));

				auto &object = defs[resultId];
				object.kind = Object::Kind::Pointer;
				object.definition = insn;

				ASSERT(getType(typeId).definition.opcode() == spv::OpTypePointer);
				ASSERT(getType(typeId).storageClass == storageClass);

				switch(storageClass)
				{
				case spv::StorageClassInput:
				case spv::StorageClassOutput:
					if(interfaceIds.count(resultId))
					{
						ProcessInterfaceVariable(object);
					}
					break;

				case spv::StorageClassUniform:
				case spv::StorageClassStorageBuffer:
					object.kind = Object::Kind::DescriptorSet;
					break;

				case spv::StorageClassPushConstant:
				case spv::StorageClassPrivate:
				case spv::StorageClassFunction:
				case spv::StorageClassUniformConstant:
					break;  // Correctly handled.

				case spv::StorageClassWorkgroup:
					{
						auto &elTy = getType(getType(typeId).element);
						auto sizeInBytes = elTy.componentCount * static_cast<uint32_t>(sizeof(float));
						workgroupMemory.allocate(resultId, sizeInBytes);
						object.kind = Object::Kind::Pointer;
					}
					break;
				case spv::StorageClassAtomicCounter:
				case spv::StorageClassImage:
					UNSUPPORTED("StorageClass %d not yet supported", (int)storageClass);
					break;

				case spv::StorageClassCrossWorkgroup:
					UNSUPPORTED("SPIR-V OpenCL Execution Model (StorageClassCrossWorkgroup)");
					break;

				case spv::StorageClassGeneric:
					UNSUPPORTED("SPIR-V GenericPointer Capability (StorageClassGeneric)");
					break;

				default:
					UNREACHABLE("Unexpected StorageClass %d", storageClass);  // See Appendix A of the Vulkan spec.
					break;
				}
			}
			break;

		case spv::OpConstant:
		case spv::OpSpecConstant:
			CreateConstant(insn).constantValue[0] = insn.word(3);
			break;
		case spv::OpConstantFalse:
		case spv::OpSpecConstantFalse:
			CreateConstant(insn).constantValue[0] = 0;  // Represent Boolean false as zero.
			break;
		case spv::OpConstantTrue:
		case spv::OpSpecConstantTrue:
			CreateConstant(insn).constantValue[0] = ~0u;  // Represent Boolean true as all bits set.
			break;
		case spv::OpConstantNull:
		case spv::OpUndef:
			{
				// TODO: consider a real LLVM-level undef. For now, zero is a perfectly good value.
				// OpConstantNull forms a constant of arbitrary type, all zeros.
				auto &object = CreateConstant(insn);
				auto &objectTy = getType(object);
				for(auto i = 0u; i < objectTy.componentCount; i++)
				{
					object.constantValue[i] = 0;
				}
			}
			break;
		case spv::OpConstantComposite:
		case spv::OpSpecConstantComposite:
			{
				auto &object = CreateConstant(insn);
				auto offset = 0u;
				for(auto i = 0u; i < insn.wordCount() - 3; i++)
				{
					auto &constituent = getObject(insn.word(i + 3));
					auto &constituentTy = getType(constituent);
					for(auto j = 0u; j < constituentTy.componentCount; j++)
					{
						object.constantValue[offset++] = constituent.constantValue[j];
					}
				}

				auto objectId = Object::ID(insn.word(2));
				auto decorationsIt = decorations.find(objectId);
				if(decorationsIt != decorations.end() &&
				   decorationsIt->second.BuiltIn == spv::BuiltInWorkgroupSize)
				{
					// https://www.khronos.org/registry/vulkan/specs/1.1/html/vkspec.html#interfaces-builtin-variables :
					// Decorating an object with the WorkgroupSize built-in
					// decoration will make that object contain the dimensions
					// of a local workgroup. If an object is decorated with the
					// WorkgroupSize decoration, this must take precedence over
					// any execution mode set for LocalSize.
					// The object decorated with WorkgroupSize must be declared
					// as a three-component vector of 32-bit integers.
					ASSERT(getType(object).componentCount == 3);
					executionModes.WorkgroupSizeX = object.constantValue[0];
					executionModes.WorkgroupSizeY = object.constantValue[1];
					executionModes.WorkgroupSizeZ = object.constantValue[2];
					executionModes.useWorkgroupSizeId = false;
				}
			}
			break;
		case spv::OpSpecConstantOp:
			EvalSpecConstantOp(insn);
			break;

		case spv::OpCapability:
			{
				auto capability = static_cast<spv::Capability>(insn.word(1));
				switch(capability)
				{
				case spv::CapabilityMatrix: capabilities.Matrix = true; break;
				case spv::CapabilityShader: capabilities.Shader = true; break;
				case spv::CapabilityStorageImageMultisample: capabilities.StorageImageMultisample = true; break;
				case spv::CapabilityClipDistance: capabilities.ClipDistance = true; break;
				case spv::CapabilityCullDistance: capabilities.CullDistance = true; break;
				case spv::CapabilityImageCubeArray: capabilities.ImageCubeArray = true; break;
				case spv::CapabilitySampleRateShading: capabilities.SampleRateShading = true; break;
				case spv::CapabilityInputAttachment: capabilities.InputAttachment = true; break;
				case spv::CapabilitySampled1D: capabilities.Sampled1D = true; break;
				case spv::CapabilityImage1D: capabilities.Image1D = true; break;
				case spv::CapabilitySampledBuffer: capabilities.SampledBuffer = true; break;
				case spv::CapabilitySampledCubeArray: capabilities.SampledCubeArray = true; break;
				case spv::CapabilityImageBuffer: capabilities.ImageBuffer = true; break;
				case spv::CapabilityImageMSArray: capabilities.ImageMSArray = true; break;
				case spv::CapabilityStorageImageExtendedFormats: capabilities.StorageImageExtendedFormats = true; break;
				case spv::CapabilityImageQuery: capabilities.ImageQuery = true; break;
				case spv::CapabilityDerivativeControl: capabilities.DerivativeControl = true; break;
				case spv::CapabilityDotProductInputAll: capabilities.DotProductInputAll = true; break;
				case spv::CapabilityDotProductInput4x8Bit: capabilities.DotProductInput4x8Bit = true; break;
				case spv::CapabilityDotProductInput4x8BitPacked: capabilities.DotProductInput4x8BitPacked = true; break;
				case spv::CapabilityDotProduct: capabilities.DotProduct = true; break;
				case spv::CapabilityInterpolationFunction: capabilities.InterpolationFunction = true; break;
				case spv::CapabilityStorageImageWriteWithoutFormat: capabilities.StorageImageWriteWithoutFormat = true; break;
				case spv::CapabilityGroupNonUniform: capabilities.GroupNonUniform = true; break;
				case spv::CapabilityGroupNonUniformVote: capabilities.GroupNonUniformVote = true; break;
				case spv::CapabilityGroupNonUniformArithmetic: capabilities.GroupNonUniformArithmetic = true; break;
				case spv::CapabilityGroupNonUniformBallot: capabilities.GroupNonUniformBallot = true; break;
				case spv::CapabilityGroupNonUniformShuffle: capabilities.GroupNonUniformShuffle = true; break;
				case spv::CapabilityGroupNonUniformShuffleRelative: capabilities.GroupNonUniformShuffleRelative = true; break;
				case spv::CapabilityDeviceGroup: capabilities.DeviceGroup = true; break;
				case spv::CapabilityMultiView: capabilities.MultiView = true; break;
				case spv::CapabilityDemoteToHelperInvocation: capabilities.DemoteToHelperInvocation = true; break;
				case spv::CapabilityStencilExportEXT: capabilities.StencilExportEXT = true; break;
				case spv::CapabilityVulkanMemoryModel: capabilities.VulkanMemoryModel = true; break;
				case spv::CapabilityVulkanMemoryModelDeviceScope: capabilities.VulkanMemoryModelDeviceScope = true; break;
				default:
					UNSUPPORTED("Unsupported capability %u", insn.word(1));
				}

				// Various capabilities will be declared, but none affect our code generation at this point.
			}
			break;

		case spv::OpMemoryModel:
			{
				addressingModel = static_cast<spv::AddressingModel>(insn.word(1));
				memoryModel = static_cast<spv::MemoryModel>(insn.word(2));
			}
			break;

		case spv::OpFunction:
			{
				auto functionId = Function::ID(insn.word(2));
				ASSERT_MSG(currentFunction == 0, "Functions %d and %d overlap", currentFunction.value(), functionId.value());
				currentFunction = functionId;
				auto &function = functions[functionId];
				function.result = Type::ID(insn.word(1));
				function.type = Type::ID(insn.word(4));
				// Scan forward to find the function's label.
				for(auto it = insn; it != end(); it++)
				{
					if(it.opcode() == spv::OpLabel)
					{
						function.entry = Block::ID(it.word(1));
						break;
					}
				}
				ASSERT_MSG(function.entry != 0, "Function<%d> has no label", currentFunction.value());
			}
			break;

		case spv::OpFunctionEnd:
			currentFunction = 0;
			break;

		case spv::OpExtInstImport:
			{
				static constexpr std::pair<const char *, Extension::Name> extensionsByName[] = {
					{ "GLSL.std.450", Extension::GLSLstd450 },
					{ "OpenCL.DebugInfo.100", Extension::OpenCLDebugInfo100 },
					{ "NonSemantic.", Extension::NonSemanticInfo },
				};
				static constexpr auto extensionCount = sizeof(extensionsByName) / sizeof(extensionsByName[0]);

				auto id = Extension::ID(insn.word(1));
				auto name = insn.string(2);
				auto ext = Extension{ Extension::Unknown };
				for(size_t i = 0; i < extensionCount; i++)
				{
					if(0 == strncmp(name, extensionsByName[i].first, strlen(extensionsByName[i].first)))
					{
						ext = Extension{ extensionsByName[i].second };
						break;
					}
				}
				if(ext.name == Extension::Unknown)
				{
					UNSUPPORTED("SPIR-V Extension: %s", name);
					break;
				}
				extensionsByID.emplace(id, ext);
				extensionsImported.emplace(ext.name);
			}
			break;
		case spv::OpName:
		case spv::OpMemberName:
		case spv::OpSource:
		case spv::OpSourceContinued:
		case spv::OpSourceExtension:
		case spv::OpLine:
		case spv::OpNoLine:
		case spv::OpModuleProcessed:
			// No semantic impact
			break;

		case spv::OpString:
			strings.emplace(insn.word(1), insn.string(2));
			break;

		case spv::OpFunctionParameter:
			// These should have all been removed by preprocessing passes. If we see them here,
			// our assumptions are wrong and we will probably generate wrong code.
			UNREACHABLE("%s should have already been lowered.", OpcodeName(opcode));
			break;

		case spv::OpFunctionCall:
			// TODO(b/141246700): Add full support for spv::OpFunctionCall
			break;

		case spv::OpFConvert:
			UNSUPPORTED("SPIR-V Float16 or Float64 Capability (OpFConvert)");
			break;

		case spv::OpSConvert:
			UNSUPPORTED("SPIR-V Int16 or Int64 Capability (OpSConvert)");
			break;

		case spv::OpUConvert:
			UNSUPPORTED("SPIR-V Int16 or Int64 Capability (OpUConvert)");
			break;

		case spv::OpLoad:
		case spv::OpAccessChain:
		case spv::OpInBoundsAccessChain:
		case spv::OpSampledImage:
		case spv::OpImage:
			{
				// Propagate the descriptor decorations to the result.
				Object::ID resultId = insn.word(2);
				Object::ID pointerId = insn.word(3);
				const auto &d = descriptorDecorations.find(pointerId);

				if(d != descriptorDecorations.end())
				{
					descriptorDecorations[resultId] = d->second;
				}

				DefineResult(insn);

				if(opcode == spv::OpAccessChain || opcode == spv::OpInBoundsAccessChain)
				{
					Decorations dd{};
					ApplyDecorationsForAccessChain(&dd, &descriptorDecorations[resultId], pointerId, Span(insn, 4, insn.wordCount() - 4));
					// Note: offset is the one thing that does *not* propagate, as the access chain accounts for it.
					dd.HasOffset = false;
					decorations[resultId].Apply(dd);
				}
			}
			break;

		case spv::OpCompositeConstruct:
		case spv::OpCompositeInsert:
		case spv::OpCompositeExtract:
		case spv::OpVectorShuffle:
		case spv::OpVectorTimesScalar:
		case spv::OpMatrixTimesScalar:
		case spv::OpMatrixTimesVector:
		case spv::OpVectorTimesMatrix:
		case spv::OpMatrixTimesMatrix:
		case spv::OpOuterProduct:
		case spv::OpTranspose:
		case spv::OpVectorExtractDynamic:
		case spv::OpVectorInsertDynamic:
		// Unary ops
		case spv::OpNot:
		case spv::OpBitFieldInsert:
		case spv::OpBitFieldSExtract:
		case spv::OpBitFieldUExtract:
		case spv::OpBitReverse:
		case spv::OpBitCount:
		case spv::OpSNegate:
		case spv::OpFNegate:
		case spv::OpLogicalNot:
		case spv::OpQuantizeToF16:
		// Binary ops
		case spv::OpIAdd:
		case spv::OpISub:
		case spv::OpIMul:
		case spv::OpSDiv:
		case spv::OpUDiv:
		case spv::OpFAdd:
		case spv::OpFSub:
		case spv::OpFMul:
		case spv::OpFDiv:
		case spv::OpFMod:
		case spv::OpFRem:
		case spv::OpFOrdEqual:
		case spv::OpFUnordEqual:
		case spv::OpFOrdNotEqual:
		case spv::OpFUnordNotEqual:
		case spv::OpFOrdLessThan:
		case spv::OpFUnordLessThan:
		case spv::OpFOrdGreaterThan:
		case spv::OpFUnordGreaterThan:
		case spv::OpFOrdLessThanEqual:
		case spv::OpFUnordLessThanEqual:
		case spv::OpFOrdGreaterThanEqual:
		case spv::OpFUnordGreaterThanEqual:
		case spv::OpSMod:
		case spv::OpSRem:
		case spv::OpUMod:
		case spv::OpIEqual:
		case spv::OpINotEqual:
		case spv::OpUGreaterThan:
		case spv::OpSGreaterThan:
		case spv::OpUGreaterThanEqual:
		case spv::OpSGreaterThanEqual:
		case spv::OpULessThan:
		case spv::OpSLessThan:
		case spv::OpULessThanEqual:
		case spv::OpSLessThanEqual:
		case spv::OpShiftRightLogical:
		case spv::OpShiftRightArithmetic:
		case spv::OpShiftLeftLogical:
		case spv::OpBitwiseOr:
		case spv::OpBitwiseXor:
		case spv::OpBitwiseAnd:
		case spv::OpLogicalOr:
		case spv::OpLogicalAnd:
		case spv::OpLogicalEqual:
		case spv::OpLogicalNotEqual:
		case spv::OpUMulExtended:
		case spv::OpSMulExtended:
		case spv::OpIAddCarry:
		case spv::OpISubBorrow:
		case spv::OpDot:
		case spv::OpSDot:
		case spv::OpUDot:
		case spv::OpSUDot:
		case spv::OpSDotAccSat:
		case spv::OpUDotAccSat:
		case spv::OpSUDotAccSat:
		case spv::OpConvertFToU:
		case spv::OpConvertFToS:
		case spv::OpConvertSToF:
		case spv::OpConvertUToF:
		case spv::OpBitcast:
		case spv::OpSelect:
		case spv::OpIsInf:
		case spv::OpIsNan:
		case spv::OpAny:
		case spv::OpAll:
		case spv::OpDPdx:
		case spv::OpDPdxCoarse:
		case spv::OpDPdy:
		case spv::OpDPdyCoarse:
		case spv::OpFwidth:
		case spv::OpFwidthCoarse:
		case spv::OpDPdxFine:
		case spv::OpDPdyFine:
		case spv::OpFwidthFine:
		case spv::OpAtomicLoad:
		case spv::OpAtomicIAdd:
		case spv::OpAtomicISub:
		case spv::OpAtomicSMin:
		case spv::OpAtomicSMax:
		case spv::OpAtomicUMin:
		case spv::OpAtomicUMax:
		case spv::OpAtomicAnd:
		case spv::OpAtomicOr:
		case spv::OpAtomicXor:
		case spv::OpAtomicIIncrement:
		case spv::OpAtomicIDecrement:
		case spv::OpAtomicExchange:
		case spv::OpAtomicCompareExchange:
		case spv::OpPhi:
		case spv::OpImageSampleImplicitLod:
		case spv::OpImageSampleExplicitLod:
		case spv::OpImageSampleDrefImplicitLod:
		case spv::OpImageSampleDrefExplicitLod:
		case spv::OpImageSampleProjImplicitLod:
		case spv::OpImageSampleProjExplicitLod:
		case spv::OpImageSampleProjDrefImplicitLod:
		case spv::OpImageSampleProjDrefExplicitLod:
		case spv::OpImageGather:
		case spv::OpImageDrefGather:
		case spv::OpImageFetch:
		case spv::OpImageQuerySizeLod:
		case spv::OpImageQuerySize:
		case spv::OpImageQueryLod:
		case spv::OpImageQueryLevels:
		case spv::OpImageQuerySamples:
		case spv::OpImageRead:
		case spv::OpImageTexelPointer:
		case spv::OpGroupNonUniformElect:
		case spv::OpGroupNonUniformAll:
		case spv::OpGroupNonUniformAny:
		case spv::OpGroupNonUniformAllEqual:
		case spv::OpGroupNonUniformBroadcast:
		case spv::OpGroupNonUniformBroadcastFirst:
		case spv::OpGroupNonUniformBallot:
		case spv::OpGroupNonUniformInverseBallot:
		case spv::OpGroupNonUniformBallotBitExtract:
		case spv::OpGroupNonUniformBallotBitCount:
		case spv::OpGroupNonUniformBallotFindLSB:
		case spv::OpGroupNonUniformBallotFindMSB:
		case spv::OpGroupNonUniformShuffle:
		case spv::OpGroupNonUniformShuffleXor:
		case spv::OpGroupNonUniformShuffleUp:
		case spv::OpGroupNonUniformShuffleDown:
		case spv::OpGroupNonUniformIAdd:
		case spv::OpGroupNonUniformFAdd:
		case spv::OpGroupNonUniformIMul:
		case spv::OpGroupNonUniformFMul:
		case spv::OpGroupNonUniformSMin:
		case spv::OpGroupNonUniformUMin:
		case spv::OpGroupNonUniformFMin:
		case spv::OpGroupNonUniformSMax:
		case spv::OpGroupNonUniformUMax:
		case spv::OpGroupNonUniformFMax:
		case spv::OpGroupNonUniformBitwiseAnd:
		case spv::OpGroupNonUniformBitwiseOr:
		case spv::OpGroupNonUniformBitwiseXor:
		case spv::OpGroupNonUniformLogicalAnd:
		case spv::OpGroupNonUniformLogicalOr:
		case spv::OpGroupNonUniformLogicalXor:
		case spv::OpCopyObject:
		case spv::OpCopyLogical:
		case spv::OpArrayLength:
		case spv::OpIsHelperInvocationEXT:
			// Instructions that yield an intermediate value or divergent pointer
			DefineResult(insn);
			break;

		case spv::OpExtInst:
			switch(getExtension(insn.word(3)).name)
			{
			case Extension::GLSLstd450:
				DefineResult(insn);
				break;
			case Extension::OpenCLDebugInfo100:
				DefineOpenCLDebugInfo100(insn);
				break;
			case Extension::NonSemanticInfo:
				// An extended set name which is prefixed with "NonSemantic." is
				// guaranteed to contain only non-semantic instructions and all
				// OpExtInst instructions referencing this set can be ignored.
				break;
			default:
				UNREACHABLE("Unexpected Extension name %d", int(getExtension(insn.word(3)).name));
				break;
			}
			break;

		case spv::OpStore:
		case spv::OpAtomicStore:
		case spv::OpImageWrite:
		case spv::OpCopyMemory:
		case spv::OpMemoryBarrier:
			// Don't need to do anything during analysis pass
			break;

		case spv::OpControlBarrier:
			analysis.ContainsControlBarriers = true;
			break;

		case spv::OpExtension:
			{
				const char *ext = insn.string(1);
				// Part of core SPIR-V 1.3. Vulkan 1.1 implementations must also accept the pre-1.3
				// extension per Appendix A, `Vulkan Environment for SPIR-V`.
				if(!strcmp(ext, "SPV_KHR_storage_buffer_storage_class")) break;
				if(!strcmp(ext, "SPV_KHR_shader_draw_parameters")) break;
				if(!strcmp(ext, "SPV_KHR_16bit_storage")) break;
				if(!strcmp(ext, "SPV_KHR_variable_pointers")) break;
				if(!strcmp(ext, "SPV_KHR_device_group")) break;
				if(!strcmp(ext, "SPV_KHR_multiview")) break;
				if(!strcmp(ext, "SPV_EXT_demote_to_helper_invocation")) break;
				if(!strcmp(ext, "SPV_KHR_terminate_invocation")) break;
				if(!strcmp(ext, "SPV_EXT_shader_stencil_export")) break;
				if(!strcmp(ext, "SPV_KHR_float_controls")) break;
				if(!strcmp(ext, "SPV_KHR_integer_dot_product")) break;
				if(!strcmp(ext, "SPV_KHR_non_semantic_info")) break;
				if(!strcmp(ext, "SPV_KHR_vulkan_memory_model")) break;
				if(!strcmp(ext, "SPV_GOOGLE_decorate_string")) break;
				if(!strcmp(ext, "SPV_GOOGLE_hlsl_functionality1")) break;
				if(!strcmp(ext, "SPV_GOOGLE_user_type")) break;
				UNSUPPORTED("SPIR-V Extension: %s", ext);
			}
			break;

		default:
			UNSUPPORTED("%s", OpcodeName(opcode));
		}
	}

	ASSERT_MSG(entryPoint != 0, "Entry point '%s' not found", entryPointName);
	for(auto &it : functions)
	{
		it.second.AssignBlockFields();
	}

#ifdef SPIRV_SHADER_CFG_GRAPHVIZ_DOT_FILEPATH
	{
		char path[1024];
		snprintf(path, sizeof(path), SPIRV_SHADER_CFG_GRAPHVIZ_DOT_FILEPATH, codeSerialID);
		WriteCFGGraphVizDotFile(path);
	}
#endif

	dbgCreateFile();
}

SpirvShader::~SpirvShader()
{
	dbgTerm();
}

void SpirvShader::DeclareType(InsnIterator insn)
{
	Type::ID resultId = insn.word(1);

	auto &type = types[resultId];
	type.definition = insn;
	type.componentCount = ComputeTypeSize(insn);

	// A structure is a builtin block if it has a builtin
	// member. All members of such a structure are builtins.
	switch(insn.opcode())
	{
	case spv::OpTypeStruct:
		{
			auto d = memberDecorations.find(resultId);
			if(d != memberDecorations.end())
			{
				for(auto &m : d->second)
				{
					if(m.HasBuiltIn)
					{
						type.isBuiltInBlock = true;
						break;
					}
				}
			}
		}
		break;
	case spv::OpTypePointer:
		{
			Type::ID elementTypeId = insn.word(3);
			type.element = elementTypeId;
			type.isBuiltInBlock = getType(elementTypeId).isBuiltInBlock;
			type.storageClass = static_cast<spv::StorageClass>(insn.word(2));
		}
		break;
	case spv::OpTypeVector:
	case spv::OpTypeMatrix:
	case spv::OpTypeArray:
	case spv::OpTypeRuntimeArray:
		{
			Type::ID elementTypeId = insn.word(2);
			type.element = elementTypeId;
		}
		break;
	default:
		break;
	}
}

SpirvShader::Object &SpirvShader::CreateConstant(InsnIterator insn)
{
	Type::ID typeId = insn.word(1);
	Object::ID resultId = insn.word(2);
	auto &object = defs[resultId];
	auto &objectTy = getType(typeId);
	object.kind = Object::Kind::Constant;
	object.definition = insn;
	object.constantValue.resize(objectTy.componentCount);

	return object;
}

void SpirvShader::ProcessInterfaceVariable(Object &object)
{
	auto &objectTy = getType(object);
	ASSERT(objectTy.storageClass == spv::StorageClassInput || objectTy.storageClass == spv::StorageClassOutput);

	ASSERT(objectTy.opcode() == spv::OpTypePointer);
	auto pointeeTy = getType(objectTy.element);

	auto &builtinInterface = (objectTy.storageClass == spv::StorageClassInput) ? inputBuiltins : outputBuiltins;
	auto &userDefinedInterface = (objectTy.storageClass == spv::StorageClassInput) ? inputs : outputs;

	ASSERT(object.opcode() == spv::OpVariable);
	Object::ID resultId = object.definition.word(2);

	if(objectTy.isBuiltInBlock)
	{
		// Walk the builtin block, registering each of its members separately.
		auto m = memberDecorations.find(objectTy.element);
		ASSERT(m != memberDecorations.end());  // Otherwise we wouldn't have marked the type chain
		auto &structType = pointeeTy.definition;
		auto memberIndex = 0u;
		auto offset = 0u;

		for(auto &member : m->second)
		{
			auto &memberType = getType(structType.word(2 + memberIndex));

			if(member.HasBuiltIn)
			{
				builtinInterface[member.BuiltIn] = { resultId, offset, memberType.componentCount };
			}

			offset += memberType.componentCount;
			++memberIndex;
		}

		return;
	}

	auto d = decorations.find(resultId);
	if(d != decorations.end() && d->second.HasBuiltIn)
	{
		builtinInterface[d->second.BuiltIn] = { resultId, 0, pointeeTy.componentCount };
	}
	else
	{
		object.kind = Object::Kind::InterfaceVariable;
		VisitInterface(resultId,
		               [&userDefinedInterface](Decorations const &d, AttribType type) {
			               // Populate a single scalar slot in the interface from a collection of decorations and the intended component type.
			               int32_t scalarSlot = (d.Location << 2) | d.Component;
			               ASSERT(scalarSlot >= 0 &&
			                      scalarSlot < static_cast<int32_t>(userDefinedInterface.size()));

			               auto &slot = userDefinedInterface[scalarSlot];
			               slot.Type = type;
			               slot.Flat = d.Flat;
			               slot.NoPerspective = d.NoPerspective;
			               slot.Centroid = d.Centroid;
		               });
	}
}

uint32_t SpirvShader::GetNumInputComponents(int32_t location) const
{
	ASSERT(location >= 0);

	// Verify how many component(s) per input
	// 1 to 4, for float, vec2, vec3, vec4.
	// Note that matrices are divided over multiple inputs
	uint32_t num_components_per_input = 0;
	for(; num_components_per_input < 4; ++num_components_per_input)
	{
		if(inputs[(location << 2) | num_components_per_input].Type == ATTRIBTYPE_UNUSED)
		{
			break;
		}
	}

	return num_components_per_input;
}

uint32_t SpirvShader::GetPackedInterpolant(int32_t location) const
{
	ASSERT(location >= 0);
	const uint32_t maxInterpolant = (location << 2);

	// Return the number of used components only at location
	uint32_t packedInterpolant = 0;
	for(uint32_t i = 0; i < maxInterpolant; ++i)
	{
		if(inputs[i].Type != ATTRIBTYPE_UNUSED)
		{
			++packedInterpolant;
		}
	}

	return packedInterpolant;
}

void SpirvShader::ProcessExecutionMode(InsnIterator insn)
{
	Function::ID function = insn.word(1);
	if(function != entryPoint)
	{
		return;
	}

	auto mode = static_cast<spv::ExecutionMode>(insn.word(2));
	switch(mode)
	{
	case spv::ExecutionModeEarlyFragmentTests:
		executionModes.EarlyFragmentTests = true;
		break;
	case spv::ExecutionModeDepthReplacing:
		executionModes.DepthReplacing = true;
		break;
	case spv::ExecutionModeDepthGreater:
		// TODO(b/177915067): Can be used to optimize depth test, currently unused.
		executionModes.DepthGreater = true;
		break;
	case spv::ExecutionModeDepthLess:
		// TODO(b/177915067): Can be used to optimize depth test, currently unused.
		executionModes.DepthLess = true;
		break;
	case spv::ExecutionModeDepthUnchanged:
		// TODO(b/177915067): Can be used to optimize depth test, currently unused.
		executionModes.DepthUnchanged = true;
		break;
	case spv::ExecutionModeLocalSize:
	case spv::ExecutionModeLocalSizeId:
		executionModes.WorkgroupSizeX = insn.word(3);
		executionModes.WorkgroupSizeY = insn.word(4);
		executionModes.WorkgroupSizeZ = insn.word(5);
		executionModes.useWorkgroupSizeId = (mode == spv::ExecutionModeLocalSizeId);
		break;
	case spv::ExecutionModeOriginUpperLeft:
		// This is always the case for a Vulkan shader. Do nothing.
		break;
	default:
		UNREACHABLE("Execution mode: %d", int(mode));
	}
}

uint32_t SpirvShader::getWorkgroupSizeX() const
{
	return executionModes.useWorkgroupSizeId ? getObject(executionModes.WorkgroupSizeX).constantValue[0] : executionModes.WorkgroupSizeX.value();
}

uint32_t SpirvShader::getWorkgroupSizeY() const
{
	return executionModes.useWorkgroupSizeId ? getObject(executionModes.WorkgroupSizeY).constantValue[0] : executionModes.WorkgroupSizeY.value();
}

uint32_t SpirvShader::getWorkgroupSizeZ() const
{
	return executionModes.useWorkgroupSizeId ? getObject(executionModes.WorkgroupSizeZ).constantValue[0] : executionModes.WorkgroupSizeZ.value();
}

uint32_t SpirvShader::ComputeTypeSize(InsnIterator insn)
{
	// Types are always built from the bottom up (with the exception of forward ptrs, which
	// don't appear in Vulkan shaders. Therefore, we can always assume our component parts have
	// already been described (and so their sizes determined)
	switch(insn.opcode())
	{
	case spv::OpTypeVoid:
	case spv::OpTypeSampler:
	case spv::OpTypeImage:
	case spv::OpTypeSampledImage:
	case spv::OpTypeFunction:
	case spv::OpTypeRuntimeArray:
		// Objects that don't consume any space.
		// Descriptor-backed objects currently only need exist at compile-time.
		// Runtime arrays don't appear in places where their size would be interesting
		return 0;

	case spv::OpTypeBool:
	case spv::OpTypeFloat:
	case spv::OpTypeInt:
		// All the fundamental types are 1 component. If we ever add support for 8/16/64-bit components,
		// we might need to change this, but only 32 bit components are required for Vulkan 1.1.
		return 1;

	case spv::OpTypeVector:
	case spv::OpTypeMatrix:
		// Vectors and matrices both consume element count * element size.
		return getType(insn.word(2)).componentCount * insn.word(3);

	case spv::OpTypeArray:
		{
			// Element count * element size. Array sizes come from constant ids.
			auto arraySize = GetConstScalarInt(insn.word(3));
			return getType(insn.word(2)).componentCount * arraySize;
		}

	case spv::OpTypeStruct:
		{
			uint32_t size = 0;
			for(uint32_t i = 2u; i < insn.wordCount(); i++)
			{
				size += getType(insn.word(i)).componentCount;
			}
			return size;
		}

	case spv::OpTypePointer:
		// Runtime representation of a pointer is a per-lane index.
		// Note: clients are expected to look through the pointer if they want the pointee size instead.
		return 1;

	default:
		UNREACHABLE("%s", OpcodeName(insn.opcode()));
		return 0;
	}
}

int SpirvShader::VisitInterfaceInner(Type::ID id, Decorations d, const InterfaceVisitor &f) const
{
	// Recursively walks variable definition and its type tree, taking into account
	// any explicit Location or Component decorations encountered; where explicit
	// Locations or Components are not specified, assigns them sequentially.
	// Collected decorations are carried down toward the leaves and across
	// siblings; Effect of decorations intentionally does not flow back up the tree.
	//
	// F is a functor to be called with the effective decoration set for every component.
	//
	// Returns the next available location, and calls f().

	// This covers the rules in Vulkan 1.1 spec, 14.1.4 Location Assignment.

	ApplyDecorationsForId(&d, id);

	auto const &obj = getType(id);
	switch(obj.opcode())
	{
	case spv::OpTypePointer:
		return VisitInterfaceInner(obj.definition.word(3), d, f);
	case spv::OpTypeMatrix:
		for(auto i = 0u; i < obj.definition.word(3); i++, d.Location++)
		{
			// consumes same components of N consecutive locations
			VisitInterfaceInner(obj.definition.word(2), d, f);
		}
		return d.Location;
	case spv::OpTypeVector:
		for(auto i = 0u; i < obj.definition.word(3); i++, d.Component++)
		{
			// consumes N consecutive components in the same location
			VisitInterfaceInner(obj.definition.word(2), d, f);
		}
		return d.Location + 1;
	case spv::OpTypeFloat:
		f(d, ATTRIBTYPE_FLOAT);
		return d.Location + 1;
	case spv::OpTypeInt:
		f(d, obj.definition.word(3) ? ATTRIBTYPE_INT : ATTRIBTYPE_UINT);
		return d.Location + 1;
	case spv::OpTypeBool:
		f(d, ATTRIBTYPE_UINT);
		return d.Location + 1;
	case spv::OpTypeStruct:
		{
			// iterate over members, which may themselves have Location/Component decorations
			for(auto i = 0u; i < obj.definition.wordCount() - 2; i++)
			{
				Decorations dMember = d;
				ApplyDecorationsForIdMember(&dMember, id, i);
				d.Location = VisitInterfaceInner(obj.definition.word(i + 2), dMember, f);
				d.Component = 0;  // Implicit locations always have component=0
			}
			return d.Location;
		}
	case spv::OpTypeArray:
		{
			auto arraySize = GetConstScalarInt(obj.definition.word(3));
			for(auto i = 0u; i < arraySize; i++)
			{
				d.Location = VisitInterfaceInner(obj.definition.word(2), d, f);
			}
			return d.Location;
		}
	default:
		// Intentionally partial; most opcodes do not participate in type hierarchies
		return 0;
	}
}

void SpirvShader::VisitInterface(Object::ID id, const InterfaceVisitor &f) const
{
	// Walk a variable definition and call f for each component in it.
	Decorations d = GetDecorationsForId(id);

	auto def = getObject(id).definition;
	ASSERT(def.opcode() == spv::OpVariable);
	VisitInterfaceInner(def.word(1), d, f);
}

void SpirvShader::ApplyDecorationsForAccessChain(Decorations *d, DescriptorDecorations *dd, Object::ID baseId, const Span &indexIds) const
{
	ApplyDecorationsForId(d, baseId);
	auto &baseObject = getObject(baseId);
	ApplyDecorationsForId(d, baseObject.typeId());
	auto typeId = getType(baseObject).element;

	for(uint32_t i = 0; i < indexIds.size(); i++)
	{
		ApplyDecorationsForId(d, typeId);
		auto &type = getType(typeId);
		switch(type.opcode())
		{
		case spv::OpTypeStruct:
			{
				int memberIndex = GetConstScalarInt(indexIds[i]);
				ApplyDecorationsForIdMember(d, typeId, memberIndex);
				typeId = type.definition.word(2u + memberIndex);
			}
			break;
		case spv::OpTypeArray:
		case spv::OpTypeRuntimeArray:
			if(dd->InputAttachmentIndex >= 0)
			{
				dd->InputAttachmentIndex += GetConstScalarInt(indexIds[i]);
			}
			typeId = type.element;
			break;
		case spv::OpTypeVector:
			typeId = type.element;
			break;
		case spv::OpTypeMatrix:
			typeId = type.element;
			d->InsideMatrix = true;
			break;
		default:
			UNREACHABLE("%s", OpcodeName(type.definition.opcode()));
		}
	}
}

SIMD::Pointer SpirvShader::WalkExplicitLayoutAccessChain(Object::ID baseId, const Span &indexIds, const EmitState *state) const
{
	// Produce a offset into external memory in sizeof(float) units

	auto &baseObject = getObject(baseId);
	Type::ID typeId = getType(baseObject).element;
	Decorations d = GetDecorationsForId(baseObject.typeId());

	Int arrayIndex = 0;
	uint32_t start = 0;
	if(baseObject.kind == Object::Kind::DescriptorSet)
	{
		auto type = getType(typeId).definition.opcode();
		if(type == spv::OpTypeArray || type == spv::OpTypeRuntimeArray)
		{
			auto &obj = getObject(indexIds[0]);
			ASSERT(obj.kind == Object::Kind::Constant || obj.kind == Object::Kind::Intermediate);
			if(obj.kind == Object::Kind::Constant)
			{
				arrayIndex = GetConstScalarInt(indexIds[0]);
			}
			else
			{
				// Note: the value of indexIds[0] must be dynamically uniform.
				arrayIndex = Extract(state->getIntermediate(indexIds[0]).Int(0), 0);
			}

			start = 1;
			typeId = getType(typeId).element;
		}
	}

	auto ptr = GetPointerToData(baseId, arrayIndex, state);

	int constantOffset = 0;

	for(uint32_t i = start; i < indexIds.size(); i++)
	{
		auto &type = getType(typeId);
		ApplyDecorationsForId(&d, typeId);

		switch(type.definition.opcode())
		{
		case spv::OpTypeStruct:
			{
				int memberIndex = GetConstScalarInt(indexIds[i]);
				ApplyDecorationsForIdMember(&d, typeId, memberIndex);
				ASSERT(d.HasOffset);
				constantOffset += d.Offset;
				typeId = type.definition.word(2u + memberIndex);
			}
			break;
		case spv::OpTypeArray:
		case spv::OpTypeRuntimeArray:
			{
				// TODO: b/127950082: Check bounds.
				ASSERT(d.HasArrayStride);
				auto &obj = getObject(indexIds[i]);
				if(obj.kind == Object::Kind::Constant)
				{
					constantOffset += d.ArrayStride * GetConstScalarInt(indexIds[i]);
				}
				else
				{
					ptr += SIMD::Int(d.ArrayStride) * state->getIntermediate(indexIds[i]).Int(0);
				}
				typeId = type.element;
			}
			break;
		case spv::OpTypeMatrix:
			{
				// TODO: b/127950082: Check bounds.
				ASSERT(d.HasMatrixStride);
				d.InsideMatrix = true;
				auto columnStride = (d.HasRowMajor && d.RowMajor) ? static_cast<int32_t>(sizeof(float)) : d.MatrixStride;
				auto &obj = getObject(indexIds[i]);
				if(obj.kind == Object::Kind::Constant)
				{
					constantOffset += columnStride * GetConstScalarInt(indexIds[i]);
				}
				else
				{
					ptr += SIMD::Int(columnStride) * state->getIntermediate(indexIds[i]).Int(0);
				}
				typeId = type.element;
			}
			break;
		case spv::OpTypeVector:
			{
				auto elemStride = (d.InsideMatrix && d.HasRowMajor && d.RowMajor) ? d.MatrixStride : static_cast<int32_t>(sizeof(float));
				auto &obj = getObject(indexIds[i]);
				if(obj.kind == Object::Kind::Constant)
				{
					constantOffset += elemStride * GetConstScalarInt(indexIds[i]);
				}
				else
				{
					ptr += SIMD::Int(elemStride) * state->getIntermediate(indexIds[i]).Int(0);
				}
				typeId = type.element;
			}
			break;
		default:
			UNREACHABLE("%s", OpcodeName(type.definition.opcode()));
		}
	}

	ptr += constantOffset;
	return ptr;
}

SIMD::Pointer SpirvShader::WalkAccessChain(Object::ID baseId, const Span &indexIds, EmitState const *state) const
{
	// TODO: avoid doing per-lane work in some cases if we can?
	auto routine = state->routine;
	auto &baseObject = getObject(baseId);
	Type::ID typeId = getType(baseObject).element;

	auto ptr = state->getPointer(baseId);

	int constantOffset = 0;

	for(uint32_t i = 0; i < indexIds.size(); i++)
	{
		auto &type = getType(typeId);
		switch(type.opcode())
		{
		case spv::OpTypeStruct:
			{
				int memberIndex = GetConstScalarInt(indexIds[i]);
				int offsetIntoStruct = 0;
				for(auto j = 0; j < memberIndex; j++)
				{
					auto memberType = type.definition.word(2u + j);
					offsetIntoStruct += getType(memberType).componentCount * sizeof(float);
				}
				constantOffset += offsetIntoStruct;
				typeId = type.definition.word(2u + memberIndex);
			}
			break;

		case spv::OpTypeVector:
		case spv::OpTypeMatrix:
		case spv::OpTypeArray:
		case spv::OpTypeRuntimeArray:
			{
				// TODO(b/127950082): Check bounds.
				if(getType(baseObject).storageClass == spv::StorageClassUniformConstant)
				{
					// indexing into an array of descriptors.
					auto d = descriptorDecorations.at(baseId);
					ASSERT(d.DescriptorSet >= 0);
					ASSERT(d.Binding >= 0);
					uint32_t descriptorSize = routine->pipelineLayout->getDescriptorSize(d.DescriptorSet, d.Binding);

					auto &obj = getObject(indexIds[i]);
					if(obj.kind == Object::Kind::Constant)
					{
						ptr.base += descriptorSize * GetConstScalarInt(indexIds[i]);
					}
					else
					{
						// Note: the value of indexIds[i] must be dynamically uniform.
						ptr.base += descriptorSize * Extract(state->getIntermediate(indexIds[i]).Int(0), 0);
					}
				}
				else
				{
					auto stride = getType(type.element).componentCount * static_cast<uint32_t>(sizeof(float));
					auto &obj = getObject(indexIds[i]);
					if(obj.kind == Object::Kind::Constant)
					{
						ptr += stride * GetConstScalarInt(indexIds[i]);
					}
					else
					{
						ptr += SIMD::Int(stride) * state->getIntermediate(indexIds[i]).Int(0);
					}
				}
				typeId = type.element;
			}
			break;

		default:
			UNREACHABLE("%s", OpcodeName(type.opcode()));
		}
	}

	if(constantOffset != 0)
	{
		ptr += constantOffset;
	}
	return ptr;
}

uint32_t SpirvShader::WalkLiteralAccessChain(Type::ID typeId, const Span &indexes) const
{
	uint32_t componentOffset = 0;

	for(uint32_t i = 0; i < indexes.size(); i++)
	{
		auto &type = getType(typeId);
		switch(type.opcode())
		{
		case spv::OpTypeStruct:
			{
				int memberIndex = indexes[i];
				int offsetIntoStruct = 0;
				for(auto j = 0; j < memberIndex; j++)
				{
					auto memberType = type.definition.word(2u + j);
					offsetIntoStruct += getType(memberType).componentCount;
				}
				componentOffset += offsetIntoStruct;
				typeId = type.definition.word(2u + memberIndex);
			}
			break;

		case spv::OpTypeVector:
		case spv::OpTypeMatrix:
		case spv::OpTypeArray:
			{
				auto elementType = type.definition.word(2);
				auto stride = getType(elementType).componentCount;
				componentOffset += stride * indexes[i];
				typeId = elementType;
			}
			break;

		default:
			UNREACHABLE("%s", OpcodeName(type.opcode()));
		}
	}

	return componentOffset;
}

void SpirvShader::Decorations::Apply(spv::Decoration decoration, uint32_t arg)
{
	switch(decoration)
	{
	case spv::DecorationLocation:
		HasLocation = true;
		Location = static_cast<int32_t>(arg);
		break;
	case spv::DecorationComponent:
		HasComponent = true;
		Component = arg;
		break;
	case spv::DecorationBuiltIn:
		HasBuiltIn = true;
		BuiltIn = static_cast<spv::BuiltIn>(arg);
		break;
	case spv::DecorationFlat:
		Flat = true;
		break;
	case spv::DecorationNoPerspective:
		NoPerspective = true;
		break;
	case spv::DecorationCentroid:
		Centroid = true;
		break;
	case spv::DecorationBlock:
		Block = true;
		break;
	case spv::DecorationBufferBlock:
		BufferBlock = true;
		break;
	case spv::DecorationOffset:
		HasOffset = true;
		Offset = static_cast<int32_t>(arg);
		break;
	case spv::DecorationArrayStride:
		HasArrayStride = true;
		ArrayStride = static_cast<int32_t>(arg);
		break;
	case spv::DecorationMatrixStride:
		HasMatrixStride = true;
		MatrixStride = static_cast<int32_t>(arg);
		break;
	case spv::DecorationRelaxedPrecision:
		RelaxedPrecision = true;
		break;
	case spv::DecorationRowMajor:
		HasRowMajor = true;
		RowMajor = true;
		break;
	case spv::DecorationColMajor:
		HasRowMajor = true;
		RowMajor = false;
	default:
		// Intentionally partial, there are many decorations we just don't care about.
		break;
	}
}

void SpirvShader::Decorations::Apply(const sw::SpirvShader::Decorations &src)
{
	// Apply a decoration group to this set of decorations
	if(src.HasBuiltIn)
	{
		HasBuiltIn = true;
		BuiltIn = src.BuiltIn;
	}

	if(src.HasLocation)
	{
		HasLocation = true;
		Location = src.Location;
	}

	if(src.HasComponent)
	{
		HasComponent = true;
		Component = src.Component;
	}

	if(src.HasOffset)
	{
		HasOffset = true;
		Offset = src.Offset;
	}

	if(src.HasArrayStride)
	{
		HasArrayStride = true;
		ArrayStride = src.ArrayStride;
	}

	if(src.HasMatrixStride)
	{
		HasMatrixStride = true;
		MatrixStride = src.MatrixStride;
	}

	if(src.HasRowMajor)
	{
		HasRowMajor = true;
		RowMajor = src.RowMajor;
	}

	Flat |= src.Flat;
	NoPerspective |= src.NoPerspective;
	Centroid |= src.Centroid;
	Block |= src.Block;
	BufferBlock |= src.BufferBlock;
	RelaxedPrecision |= src.RelaxedPrecision;
	InsideMatrix |= src.InsideMatrix;
}

void SpirvShader::DescriptorDecorations::Apply(const sw::SpirvShader::DescriptorDecorations &src)
{
	if(src.DescriptorSet >= 0)
	{
		DescriptorSet = src.DescriptorSet;
	}

	if(src.Binding >= 0)
	{
		Binding = src.Binding;
	}

	if(src.InputAttachmentIndex >= 0)
	{
		InputAttachmentIndex = src.InputAttachmentIndex;
	}
}

SpirvShader::Decorations SpirvShader::GetDecorationsForId(TypeOrObjectID id) const
{
	Decorations d;
	ApplyDecorationsForId(&d, id);

	return d;
}

void SpirvShader::ApplyDecorationsForId(Decorations *d, TypeOrObjectID id) const
{
	auto it = decorations.find(id);
	if(it != decorations.end())
	{
		d->Apply(it->second);
	}
}

void SpirvShader::ApplyDecorationsForIdMember(Decorations *d, Type::ID id, uint32_t member) const
{
	auto it = memberDecorations.find(id);
	if(it != memberDecorations.end() && member < it->second.size())
	{
		d->Apply(it->second[member]);
	}
}

void SpirvShader::DefineResult(const InsnIterator &insn)
{
	Type::ID typeId = insn.word(1);
	Object::ID resultId = insn.word(2);
	auto &object = defs[resultId];

	switch(getType(typeId).opcode())
	{
	case spv::OpTypePointer:
	case spv::OpTypeImage:
	case spv::OpTypeSampledImage:
	case spv::OpTypeSampler:
		object.kind = Object::Kind::Pointer;
		break;

	default:
		object.kind = Object::Kind::Intermediate;
	}

	object.definition = insn;
	dbgDeclareResult(insn, resultId);
}

OutOfBoundsBehavior SpirvShader::getOutOfBoundsBehavior(Object::ID pointerId, EmitState const *state) const
{
	auto it = descriptorDecorations.find(pointerId);
	if(it != descriptorDecorations.end())
	{
		const auto &d = it->second;
		if((d.DescriptorSet >= 0) && (d.Binding >= 0))
		{
			auto descriptorType = state->routine->pipelineLayout->getDescriptorType(d.DescriptorSet, d.Binding);
			if(descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
			{
				return OutOfBoundsBehavior::UndefinedBehavior;
			}
		}
	}

	auto &pointer = getObject(pointerId);
	auto &pointerTy = getType(pointer);
	switch(pointerTy.storageClass)
	{
	case spv::StorageClassUniform:
	case spv::StorageClassStorageBuffer:
		// Buffer resource access. robustBufferAccess feature applies.
		return robustBufferAccess ? OutOfBoundsBehavior::RobustBufferAccess
		                          : OutOfBoundsBehavior::UndefinedBehavior;

	case spv::StorageClassImage:
		// VK_EXT_image_robustness requires nullifying out-of-bounds accesses.
		// TODO(b/162327166): Only perform bounds checks when VK_EXT_image_robustness is enabled.
		return OutOfBoundsBehavior::Nullify;

	case spv::StorageClassInput:
		if(executionModel == spv::ExecutionModelVertex)
		{
			// Vertex attributes follow robustBufferAccess rules.
			return robustBufferAccess ? OutOfBoundsBehavior::RobustBufferAccess
			                          : OutOfBoundsBehavior::UndefinedBehavior;
		}
		// Fall through to default case.
	default:
		// TODO(b/192310780): StorageClassFunction out-of-bounds accesses are undefined behavior.
		// TODO(b/137183137): Optimize if the pointer resulted from OpInBoundsAccessChain.
		// TODO(b/131224163): Optimize cases statically known to be within bounds.
		return OutOfBoundsBehavior::UndefinedValue;
	}

	return OutOfBoundsBehavior::Nullify;
}

// emit-time

void SpirvShader::emitProlog(SpirvRoutine *routine) const
{
	if(IsProfilingEnabled())
	{
		routine->profData = std::make_unique<SpirvProfileData>();
	}

	for(auto insn : *this)
	{
		switch(insn.opcode())
		{
		case spv::OpVariable:
			{
				auto resultPointerType = getType(insn.resultTypeId());
				auto pointeeType = getType(resultPointerType.element);

				if(pointeeType.componentCount > 0)  // TODO: what to do about zero-slot objects?
				{
					routine->createVariable(insn.resultId(), pointeeType.componentCount);
				}
			}
			break;

		case spv::OpPhi:
			{
				auto type = getType(insn.resultTypeId());
				routine->phis.emplace(insn.resultId(), SpirvRoutine::Variable(type.componentCount));
			}
			break;

		case spv::OpImageSampleImplicitLod:
		case spv::OpImageSampleExplicitLod:
		case spv::OpImageSampleDrefImplicitLod:
		case spv::OpImageSampleDrefExplicitLod:
		case spv::OpImageSampleProjImplicitLod:
		case spv::OpImageSampleProjExplicitLod:
		case spv::OpImageSampleProjDrefImplicitLod:
		case spv::OpImageSampleProjDrefExplicitLod:
		case spv::OpImageFetch:
		case spv::OpImageGather:
		case spv::OpImageDrefGather:
		case spv::OpImageWrite:
		case spv::OpImageQueryLod:
			{
				// The 'inline' sampler caches must be created in the prolog to initialize the tags.
				uint32_t instructionPosition = insn.distanceFrom(this->begin());
				routine->samplerCache.emplace(instructionPosition, SpirvRoutine::SamplerCache{});
			}
			break;

		default:
			// Nothing else produces interface variables, so can all be safely ignored.
			break;
		}
	}
}

void SpirvShader::emit(SpirvRoutine *routine, RValue<SIMD::Int> const &activeLaneMask, RValue<SIMD::Int> const &storesAndAtomicsMask, const vk::DescriptorSet::Bindings &descriptorSets, unsigned int multiSampleCount) const
{
	EmitState state(routine, entryPoint, activeLaneMask, storesAndAtomicsMask, descriptorSets, multiSampleCount);

	dbgBeginEmit(&state);
	defer(dbgEndEmit(&state));

	// Emit everything up to the first label
	// TODO: Separate out dispatch of block from non-block instructions?
	for(auto insn : *this)
	{
		if(insn.opcode() == spv::OpLabel)
		{
			break;
		}
		EmitInstruction(insn, &state);
	}

	// Emit all the blocks starting from entryPoint.
	EmitBlocks(getFunction(entryPoint).entry, &state);
}

void SpirvShader::EmitInstructions(InsnIterator begin, InsnIterator end, EmitState *state) const
{
	for(auto insn = begin; insn != end; insn++)
	{
		auto res = EmitInstruction(insn, state);
		switch(res)
		{
		case EmitResult::Continue:
			continue;
		case EmitResult::Terminator:
			break;
		default:
			UNREACHABLE("Unexpected EmitResult %d", int(res));
			break;
		}
	}
}

SpirvShader::EmitResult SpirvShader::EmitInstruction(InsnIterator insn, EmitState *state) const
{
	dbgBeginEmitInstruction(insn, state);
	defer(dbgEndEmitInstruction(insn, state));

	auto opcode = insn.opcode();

	if(IsProfilingEnabled() && IsStatement(opcode))
	{
		int64_t *counter = &state->routine->profData->spvOpExecutionCount[opcode];
		AddAtomic(Pointer<Long>(ConstantPointer(counter)), 1);
	}

#if SPIRV_SHADER_ENABLE_DBG
	{
		auto text = spvtools::spvInstructionBinaryToText(
		    vk::SPIRV_VERSION,
		    insn.data(),
		    insn.wordCount(),
		    insns.data(),
		    insns.size(),
		    SPV_BINARY_TO_TEXT_OPTION_NO_HEADER);
		SPIRV_SHADER_DBG("{0}", text);
	}
#endif  // ENABLE_DBG_MSGS

	switch(opcode)
	{
	case spv::OpTypeVoid:
	case spv::OpTypeInt:
	case spv::OpTypeFloat:
	case spv::OpTypeBool:
	case spv::OpTypeVector:
	case spv::OpTypeArray:
	case spv::OpTypeRuntimeArray:
	case spv::OpTypeMatrix:
	case spv::OpTypeStruct:
	case spv::OpTypePointer:
	case spv::OpTypeFunction:
	case spv::OpTypeImage:
	case spv::OpTypeSampledImage:
	case spv::OpTypeSampler:
	case spv::OpExecutionMode:
	case spv::OpExecutionModeId:
	case spv::OpMemoryModel:
	case spv::OpFunction:
	case spv::OpFunctionEnd:
	case spv::OpConstant:
	case spv::OpConstantNull:
	case spv::OpConstantTrue:
	case spv::OpConstantFalse:
	case spv::OpConstantComposite:
	case spv::OpSpecConstant:
	case spv::OpSpecConstantTrue:
	case spv::OpSpecConstantFalse:
	case spv::OpSpecConstantComposite:
	case spv::OpSpecConstantOp:
	case spv::OpUndef:
	case spv::OpExtension:
	case spv::OpCapability:
	case spv::OpEntryPoint:
	case spv::OpExtInstImport:
	case spv::OpDecorate:
	case spv::OpMemberDecorate:
	case spv::OpGroupDecorate:
	case spv::OpGroupMemberDecorate:
	case spv::OpDecorationGroup:
	case spv::OpDecorateId:
	case spv::OpDecorateString:
	case spv::OpMemberDecorateString:
	case spv::OpName:
	case spv::OpMemberName:
	case spv::OpSource:
	case spv::OpSourceContinued:
	case spv::OpSourceExtension:
	case spv::OpNoLine:
	case spv::OpModuleProcessed:
	case spv::OpString:
		// Nothing to do at emit time. These are either fully handled at analysis time,
		// or don't require any work at all.
		return EmitResult::Continue;

	case spv::OpLine:
		return EmitLine(insn, state);

	case spv::OpLabel:
		return EmitResult::Continue;

	case spv::OpVariable:
		return EmitVariable(insn, state);

	case spv::OpLoad:
	case spv::OpAtomicLoad:
		return EmitLoad(insn, state);

	case spv::OpStore:
	case spv::OpAtomicStore:
		return EmitStore(insn, state);

	case spv::OpAtomicIAdd:
	case spv::OpAtomicISub:
	case spv::OpAtomicSMin:
	case spv::OpAtomicSMax:
	case spv::OpAtomicUMin:
	case spv::OpAtomicUMax:
	case spv::OpAtomicAnd:
	case spv::OpAtomicOr:
	case spv::OpAtomicXor:
	case spv::OpAtomicIIncrement:
	case spv::OpAtomicIDecrement:
	case spv::OpAtomicExchange:
		return EmitAtomicOp(insn, state);

	case spv::OpAtomicCompareExchange:
		return EmitAtomicCompareExchange(insn, state);

	case spv::OpAccessChain:
	case spv::OpInBoundsAccessChain:
		return EmitAccessChain(insn, state);

	case spv::OpCompositeConstruct:
		return EmitCompositeConstruct(insn, state);

	case spv::OpCompositeInsert:
		return EmitCompositeInsert(insn, state);

	case spv::OpCompositeExtract:
		return EmitCompositeExtract(insn, state);

	case spv::OpVectorShuffle:
		return EmitVectorShuffle(insn, state);

	case spv::OpVectorExtractDynamic:
		return EmitVectorExtractDynamic(insn, state);

	case spv::OpVectorInsertDynamic:
		return EmitVectorInsertDynamic(insn, state);

	case spv::OpVectorTimesScalar:
	case spv::OpMatrixTimesScalar:
		return EmitVectorTimesScalar(insn, state);

	case spv::OpMatrixTimesVector:
		return EmitMatrixTimesVector(insn, state);

	case spv::OpVectorTimesMatrix:
		return EmitVectorTimesMatrix(insn, state);

	case spv::OpMatrixTimesMatrix:
		return EmitMatrixTimesMatrix(insn, state);

	case spv::OpOuterProduct:
		return EmitOuterProduct(insn, state);

	case spv::OpTranspose:
		return EmitTranspose(insn, state);

	case spv::OpNot:
	case spv::OpBitFieldInsert:
	case spv::OpBitFieldSExtract:
	case spv::OpBitFieldUExtract:
	case spv::OpBitReverse:
	case spv::OpBitCount:
	case spv::OpSNegate:
	case spv::OpFNegate:
	case spv::OpLogicalNot:
	case spv::OpConvertFToU:
	case spv::OpConvertFToS:
	case spv::OpConvertSToF:
	case spv::OpConvertUToF:
	case spv::OpBitcast:
	case spv::OpIsInf:
	case spv::OpIsNan:
	case spv::OpDPdx:
	case spv::OpDPdxCoarse:
	case spv::OpDPdy:
	case spv::OpDPdyCoarse:
	case spv::OpFwidth:
	case spv::OpFwidthCoarse:
	case spv::OpDPdxFine:
	case spv::OpDPdyFine:
	case spv::OpFwidthFine:
	case spv::OpQuantizeToF16:
		return EmitUnaryOp(insn, state);

	case spv::OpIAdd:
	case spv::OpISub:
	case spv::OpIMul:
	case spv::OpSDiv:
	case spv::OpUDiv:
	case spv::OpFAdd:
	case spv::OpFSub:
	case spv::OpFMul:
	case spv::OpFDiv:
	case spv::OpFMod:
	case spv::OpFRem:
	case spv::OpFOrdEqual:
	case spv::OpFUnordEqual:
	case spv::OpFOrdNotEqual:
	case spv::OpFUnordNotEqual:
	case spv::OpFOrdLessThan:
	case spv::OpFUnordLessThan:
	case spv::OpFOrdGreaterThan:
	case spv::OpFUnordGreaterThan:
	case spv::OpFOrdLessThanEqual:
	case spv::OpFUnordLessThanEqual:
	case spv::OpFOrdGreaterThanEqual:
	case spv::OpFUnordGreaterThanEqual:
	case spv::OpSMod:
	case spv::OpSRem:
	case spv::OpUMod:
	case spv::OpIEqual:
	case spv::OpINotEqual:
	case spv::OpUGreaterThan:
	case spv::OpSGreaterThan:
	case spv::OpUGreaterThanEqual:
	case spv::OpSGreaterThanEqual:
	case spv::OpULessThan:
	case spv::OpSLessThan:
	case spv::OpULessThanEqual:
	case spv::OpSLessThanEqual:
	case spv::OpShiftRightLogical:
	case spv::OpShiftRightArithmetic:
	case spv::OpShiftLeftLogical:
	case spv::OpBitwiseOr:
	case spv::OpBitwiseXor:
	case spv::OpBitwiseAnd:
	case spv::OpLogicalOr:
	case spv::OpLogicalAnd:
	case spv::OpLogicalEqual:
	case spv::OpLogicalNotEqual:
	case spv::OpUMulExtended:
	case spv::OpSMulExtended:
	case spv::OpIAddCarry:
	case spv::OpISubBorrow:
		return EmitBinaryOp(insn, state);

	case spv::OpDot:
	case spv::OpSDot:
	case spv::OpUDot:
	case spv::OpSUDot:
	case spv::OpSDotAccSat:
	case spv::OpUDotAccSat:
	case spv::OpSUDotAccSat:
		return EmitDot(insn, state);

	case spv::OpSelect:
		return EmitSelect(insn, state);

	case spv::OpExtInst:
		return EmitExtendedInstruction(insn, state);

	case spv::OpAny:
		return EmitAny(insn, state);

	case spv::OpAll:
		return EmitAll(insn, state);

	case spv::OpBranch:
		return EmitBranch(insn, state);

	case spv::OpPhi:
		return EmitPhi(insn, state);

	case spv::OpSelectionMerge:
	case spv::OpLoopMerge:
		return EmitResult::Continue;

	case spv::OpBranchConditional:
		return EmitBranchConditional(insn, state);

	case spv::OpSwitch:
		return EmitSwitch(insn, state);

	case spv::OpUnreachable:
		return EmitUnreachable(insn, state);

	case spv::OpReturn:
		return EmitReturn(insn, state);

	case spv::OpFunctionCall:
		return EmitFunctionCall(insn, state);

	case spv::OpKill:
	case spv::OpTerminateInvocation:
		return EmitTerminateInvocation(insn, state);

	case spv::OpDemoteToHelperInvocation:
		return EmitDemoteToHelperInvocation(insn, state);

	case spv::OpIsHelperInvocationEXT:
		return EmitIsHelperInvocation(insn, state);

	case spv::OpImageSampleImplicitLod:
	case spv::OpImageSampleExplicitLod:
	case spv::OpImageSampleDrefImplicitLod:
	case spv::OpImageSampleDrefExplicitLod:
	case spv::OpImageSampleProjImplicitLod:
	case spv::OpImageSampleProjExplicitLod:
	case spv::OpImageSampleProjDrefImplicitLod:
	case spv::OpImageSampleProjDrefExplicitLod:
	case spv::OpImageGather:
	case spv::OpImageDrefGather:
	case spv::OpImageFetch:
	case spv::OpImageQueryLod:
		return EmitImageSample(ImageInstruction(insn, *this), state);

	case spv::OpImageQuerySizeLod:
		return EmitImageQuerySizeLod(insn, state);

	case spv::OpImageQuerySize:
		return EmitImageQuerySize(insn, state);

	case spv::OpImageQueryLevels:
		return EmitImageQueryLevels(insn, state);

	case spv::OpImageQuerySamples:
		return EmitImageQuerySamples(insn, state);

	case spv::OpImageRead:
		return EmitImageRead(ImageInstruction(insn, *this), state);

	case spv::OpImageWrite:
		return EmitImageWrite(ImageInstruction(insn, *this), state);

	case spv::OpImageTexelPointer:
		return EmitImageTexelPointer(ImageInstruction(insn, *this), state);

	case spv::OpSampledImage:
	case spv::OpImage:
		return EmitSampledImageCombineOrSplit(insn, state);

	case spv::OpCopyObject:
	case spv::OpCopyLogical:
		return EmitCopyObject(insn, state);

	case spv::OpCopyMemory:
		return EmitCopyMemory(insn, state);

	case spv::OpControlBarrier:
		return EmitControlBarrier(insn, state);

	case spv::OpMemoryBarrier:
		return EmitMemoryBarrier(insn, state);

	case spv::OpGroupNonUniformElect:
	case spv::OpGroupNonUniformAll:
	case spv::OpGroupNonUniformAny:
	case spv::OpGroupNonUniformAllEqual:
	case spv::OpGroupNonUniformBroadcast:
	case spv::OpGroupNonUniformBroadcastFirst:
	case spv::OpGroupNonUniformBallot:
	case spv::OpGroupNonUniformInverseBallot:
	case spv::OpGroupNonUniformBallotBitExtract:
	case spv::OpGroupNonUniformBallotBitCount:
	case spv::OpGroupNonUniformBallotFindLSB:
	case spv::OpGroupNonUniformBallotFindMSB:
	case spv::OpGroupNonUniformShuffle:
	case spv::OpGroupNonUniformShuffleXor:
	case spv::OpGroupNonUniformShuffleUp:
	case spv::OpGroupNonUniformShuffleDown:
	case spv::OpGroupNonUniformIAdd:
	case spv::OpGroupNonUniformFAdd:
	case spv::OpGroupNonUniformIMul:
	case spv::OpGroupNonUniformFMul:
	case spv::OpGroupNonUniformSMin:
	case spv::OpGroupNonUniformUMin:
	case spv::OpGroupNonUniformFMin:
	case spv::OpGroupNonUniformSMax:
	case spv::OpGroupNonUniformUMax:
	case spv::OpGroupNonUniformFMax:
	case spv::OpGroupNonUniformBitwiseAnd:
	case spv::OpGroupNonUniformBitwiseOr:
	case spv::OpGroupNonUniformBitwiseXor:
	case spv::OpGroupNonUniformLogicalAnd:
	case spv::OpGroupNonUniformLogicalOr:
	case spv::OpGroupNonUniformLogicalXor:
		return EmitGroupNonUniform(insn, state);

	case spv::OpArrayLength:
		return EmitArrayLength(insn, state);

	default:
		UNREACHABLE("%s", OpcodeName(opcode));
		break;
	}

	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitAccessChain(InsnIterator insn, EmitState *state) const
{
	Type::ID typeId = insn.word(1);
	Object::ID resultId = insn.word(2);
	Object::ID baseId = insn.word(3);
	auto &type = getType(typeId);
	ASSERT(type.componentCount == 1);
	ASSERT(getObject(resultId).kind == Object::Kind::Pointer);

	if(type.storageClass == spv::StorageClassPushConstant ||
	   type.storageClass == spv::StorageClassUniform ||
	   type.storageClass == spv::StorageClassStorageBuffer)
	{
		auto ptr = WalkExplicitLayoutAccessChain(baseId, Span(insn, 4, insn.wordCount() - 4), state);
		state->createPointer(resultId, ptr);
	}
	else
	{
		auto ptr = WalkAccessChain(baseId, Span(insn, 4, insn.wordCount() - 4), state);
		state->createPointer(resultId, ptr);
	}

	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitCompositeConstruct(InsnIterator insn, EmitState *state) const
{
	auto &type = getType(insn.resultTypeId());
	auto &dst = state->createIntermediate(insn.resultId(), type.componentCount);
	auto offset = 0u;

	for(auto i = 0u; i < insn.wordCount() - 3; i++)
	{
		Object::ID srcObjectId = insn.word(3u + i);
		auto &srcObject = getObject(srcObjectId);
		auto &srcObjectTy = getType(srcObject);
		Operand srcObjectAccess(this, state, srcObjectId);

		for(auto j = 0u; j < srcObjectTy.componentCount; j++)
		{
			dst.move(offset++, srcObjectAccess.Float(j));
		}
	}

	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitCompositeInsert(InsnIterator insn, EmitState *state) const
{
	Type::ID resultTypeId = insn.word(1);
	auto &type = getType(resultTypeId);
	auto &dst = state->createIntermediate(insn.resultId(), type.componentCount);
	auto &newPartObject = getObject(insn.word(3));
	auto &newPartObjectTy = getType(newPartObject);
	auto firstNewComponent = WalkLiteralAccessChain(resultTypeId, Span(insn, 5, insn.wordCount() - 5));

	Operand srcObjectAccess(this, state, insn.word(4));
	Operand newPartObjectAccess(this, state, insn.word(3));

	// old components before
	for(auto i = 0u; i < firstNewComponent; i++)
	{
		dst.move(i, srcObjectAccess.Float(i));
	}
	// new part
	for(auto i = 0u; i < newPartObjectTy.componentCount; i++)
	{
		dst.move(firstNewComponent + i, newPartObjectAccess.Float(i));
	}
	// old components after
	for(auto i = firstNewComponent + newPartObjectTy.componentCount; i < type.componentCount; i++)
	{
		dst.move(i, srcObjectAccess.Float(i));
	}

	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitCompositeExtract(InsnIterator insn, EmitState *state) const
{
	auto &type = getType(insn.resultTypeId());
	auto &dst = state->createIntermediate(insn.resultId(), type.componentCount);
	auto &compositeObject = getObject(insn.word(3));
	Type::ID compositeTypeId = compositeObject.definition.word(1);
	auto firstComponent = WalkLiteralAccessChain(compositeTypeId, Span(insn, 4, insn.wordCount() - 4));

	Operand compositeObjectAccess(this, state, insn.word(3));
	for(auto i = 0u; i < type.componentCount; i++)
	{
		dst.move(i, compositeObjectAccess.Float(firstComponent + i));
	}

	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitVectorShuffle(InsnIterator insn, EmitState *state) const
{
	auto &type = getType(insn.resultTypeId());
	auto &dst = state->createIntermediate(insn.resultId(), type.componentCount);

	// Note: number of components in result type, first half type, and second
	// half type are all independent.
	auto &firstHalfType = getObjectType(insn.word(3));

	Operand firstHalfAccess(this, state, insn.word(3));
	Operand secondHalfAccess(this, state, insn.word(4));

	for(auto i = 0u; i < type.componentCount; i++)
	{
		auto selector = insn.word(5 + i);
		if(selector == static_cast<uint32_t>(-1))
		{
			// Undefined value. Until we decide to do real undef values, zero is as good
			// a value as any
			dst.move(i, RValue<SIMD::Float>(0.0f));
		}
		else if(selector < firstHalfType.componentCount)
		{
			dst.move(i, firstHalfAccess.Float(selector));
		}
		else
		{
			dst.move(i, secondHalfAccess.Float(selector - firstHalfType.componentCount));
		}
	}

	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitVectorExtractDynamic(InsnIterator insn, EmitState *state) const
{
	auto &type = getType(insn.resultTypeId());
	auto &dst = state->createIntermediate(insn.resultId(), type.componentCount);
	auto &srcType = getObjectType(insn.word(3));

	Operand src(this, state, insn.word(3));
	Operand index(this, state, insn.word(4));

	SIMD::UInt v = SIMD::UInt(0);

	for(auto i = 0u; i < srcType.componentCount; i++)
	{
		v |= CmpEQ(index.UInt(0), SIMD::UInt(i)) & src.UInt(i);
	}

	dst.move(0, v);
	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitVectorInsertDynamic(InsnIterator insn, EmitState *state) const
{
	auto &type = getType(insn.resultTypeId());
	auto &dst = state->createIntermediate(insn.resultId(), type.componentCount);

	Operand src(this, state, insn.word(3));
	Operand component(this, state, insn.word(4));
	Operand index(this, state, insn.word(5));

	for(auto i = 0u; i < type.componentCount; i++)
	{
		SIMD::UInt mask = CmpEQ(SIMD::UInt(i), index.UInt(0));
		dst.move(i, (src.UInt(i) & ~mask) | (component.UInt(0) & mask));
	}
	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitSelect(InsnIterator insn, EmitState *state) const
{
	auto &type = getType(insn.resultTypeId());
	auto &dst = state->createIntermediate(insn.resultId(), type.componentCount);
	auto cond = Operand(this, state, insn.word(3));
	auto condIsScalar = (cond.componentCount == 1);
	auto lhs = Operand(this, state, insn.word(4));
	auto rhs = Operand(this, state, insn.word(5));

	for(auto i = 0u; i < type.componentCount; i++)
	{
		auto sel = cond.Int(condIsScalar ? 0 : i);
		dst.move(i, (sel & lhs.Int(i)) | (~sel & rhs.Int(i)));  // TODO: IfThenElse()
	}

	SPIRV_SHADER_DBG("{0}: {1}", insn.word(2), dst);
	SPIRV_SHADER_DBG("{0}: {1}", insn.word(3), cond);
	SPIRV_SHADER_DBG("{0}: {1}", insn.word(4), lhs);
	SPIRV_SHADER_DBG("{0}: {1}", insn.word(5), rhs);

	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitAny(InsnIterator insn, EmitState *state) const
{
	auto &type = getType(insn.resultTypeId());
	ASSERT(type.componentCount == 1);
	auto &dst = state->createIntermediate(insn.resultId(), type.componentCount);
	auto &srcType = getObjectType(insn.word(3));
	auto src = Operand(this, state, insn.word(3));

	SIMD::UInt result = src.UInt(0);

	for(auto i = 1u; i < srcType.componentCount; i++)
	{
		result |= src.UInt(i);
	}

	dst.move(0, result);
	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitAll(InsnIterator insn, EmitState *state) const
{
	auto &type = getType(insn.resultTypeId());
	ASSERT(type.componentCount == 1);
	auto &dst = state->createIntermediate(insn.resultId(), type.componentCount);
	auto &srcType = getObjectType(insn.word(3));
	auto src = Operand(this, state, insn.word(3));

	SIMD::UInt result = src.UInt(0);

	for(auto i = 1u; i < srcType.componentCount; i++)
	{
		result &= src.UInt(i);
	}

	dst.move(0, result);
	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitAtomicOp(InsnIterator insn, EmitState *state) const
{
	auto &resultType = getType(Type::ID(insn.word(1)));
	Object::ID resultId = insn.word(2);
	Object::ID pointerId = insn.word(3);
	Object::ID semanticsId = insn.word(5);
	auto memorySemantics = static_cast<spv::MemorySemanticsMask>(getObject(semanticsId).constantValue[0]);
	auto memoryOrder = MemoryOrder(memorySemantics);
	// Where no value is provided (increment/decrement) use an implicit value of 1.
	auto value = (insn.wordCount() == 7) ? Operand(this, state, insn.word(6)).UInt(0) : RValue<SIMD::UInt>(1);
	auto &dst = state->createIntermediate(resultId, resultType.componentCount);
	auto ptr = state->getPointer(pointerId);
	auto ptrOffsets = ptr.offsets();

	SIMD::Int mask = state->activeLaneMask() & state->storesAndAtomicsMask();

	if(getObject(pointerId).opcode() == spv::OpImageTexelPointer)
	{
		mask &= ptr.isInBounds(sizeof(int32_t), OutOfBoundsBehavior::Nullify);
	}

	SIMD::UInt result(0);
	for(int j = 0; j < SIMD::Width; j++)
	{
		If(Extract(mask, j) != 0)
		{
			auto offset = Extract(ptrOffsets, j);
			auto laneValue = Extract(value, j);
			UInt v;
			switch(insn.opcode())
			{
			case spv::OpAtomicIAdd:
			case spv::OpAtomicIIncrement:
				v = AddAtomic(Pointer<UInt>(&ptr.base[offset]), laneValue, memoryOrder);
				break;
			case spv::OpAtomicISub:
			case spv::OpAtomicIDecrement:
				v = SubAtomic(Pointer<UInt>(&ptr.base[offset]), laneValue, memoryOrder);
				break;
			case spv::OpAtomicAnd:
				v = AndAtomic(Pointer<UInt>(&ptr.base[offset]), laneValue, memoryOrder);
				break;
			case spv::OpAtomicOr:
				v = OrAtomic(Pointer<UInt>(&ptr.base[offset]), laneValue, memoryOrder);
				break;
			case spv::OpAtomicXor:
				v = XorAtomic(Pointer<UInt>(&ptr.base[offset]), laneValue, memoryOrder);
				break;
			case spv::OpAtomicSMin:
				v = As<UInt>(MinAtomic(Pointer<Int>(&ptr.base[offset]), As<Int>(laneValue), memoryOrder));
				break;
			case spv::OpAtomicSMax:
				v = As<UInt>(MaxAtomic(Pointer<Int>(&ptr.base[offset]), As<Int>(laneValue), memoryOrder));
				break;
			case spv::OpAtomicUMin:
				v = MinAtomic(Pointer<UInt>(&ptr.base[offset]), laneValue, memoryOrder);
				break;
			case spv::OpAtomicUMax:
				v = MaxAtomic(Pointer<UInt>(&ptr.base[offset]), laneValue, memoryOrder);
				break;
			case spv::OpAtomicExchange:
				v = ExchangeAtomic(Pointer<UInt>(&ptr.base[offset]), laneValue, memoryOrder);
				break;
			default:
				UNREACHABLE("%s", OpcodeName(insn.opcode()));
				break;
			}
			result = Insert(result, v, j);
		}
	}

	dst.move(0, result);
	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitAtomicCompareExchange(InsnIterator insn, EmitState *state) const
{
	// Separate from EmitAtomicOp due to different instruction encoding
	auto &resultType = getType(Type::ID(insn.word(1)));
	Object::ID resultId = insn.word(2);

	auto memorySemanticsEqual = static_cast<spv::MemorySemanticsMask>(getObject(insn.word(5)).constantValue[0]);
	auto memoryOrderEqual = MemoryOrder(memorySemanticsEqual);
	auto memorySemanticsUnequal = static_cast<spv::MemorySemanticsMask>(getObject(insn.word(6)).constantValue[0]);
	auto memoryOrderUnequal = MemoryOrder(memorySemanticsUnequal);

	auto value = Operand(this, state, insn.word(7));
	auto comparator = Operand(this, state, insn.word(8));
	auto &dst = state->createIntermediate(resultId, resultType.componentCount);
	auto ptr = state->getPointer(insn.word(3));
	auto ptrOffsets = ptr.offsets();

	SIMD::UInt x(0);
	auto mask = state->activeLaneMask() & state->storesAndAtomicsMask();
	for(int j = 0; j < SIMD::Width; j++)
	{
		If(Extract(mask, j) != 0)
		{
			auto offset = Extract(ptrOffsets, j);
			auto laneValue = Extract(value.UInt(0), j);
			auto laneComparator = Extract(comparator.UInt(0), j);
			UInt v = CompareExchangeAtomic(Pointer<UInt>(&ptr.base[offset]), laneValue, laneComparator, memoryOrderEqual, memoryOrderUnequal);
			x = Insert(x, v, j);
		}
	}

	dst.move(0, x);
	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitCopyObject(InsnIterator insn, EmitState *state) const
{
	auto type = getType(insn.resultTypeId());
	auto &dst = state->createIntermediate(insn.resultId(), type.componentCount);
	auto src = Operand(this, state, insn.word(3));
	for(uint32_t i = 0; i < type.componentCount; i++)
	{
		dst.move(i, src.Int(i));
	}
	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitArrayLength(InsnIterator insn, EmitState *state) const
{
	auto structPtrId = Object::ID(insn.word(3));
	auto arrayFieldIdx = insn.word(4);

	auto &resultType = getType(insn.resultTypeId());
	ASSERT(resultType.componentCount == 1);
	ASSERT(resultType.definition.opcode() == spv::OpTypeInt);

	auto &structPtrTy = getObjectType(structPtrId);
	auto &structTy = getType(structPtrTy.element);
	auto arrayId = Type::ID(structTy.definition.word(2 + arrayFieldIdx));

	auto &result = state->createIntermediate(insn.resultId(), 1);
	auto structBase = GetPointerToData(structPtrId, 0, state);

	Decorations structDecorations = {};
	ApplyDecorationsForIdMember(&structDecorations, structPtrTy.element, arrayFieldIdx);
	ASSERT(structDecorations.HasOffset);

	auto arrayBase = structBase + structDecorations.Offset;
	auto arraySizeInBytes = SIMD::Int(arrayBase.limit()) - arrayBase.offsets();

	Decorations arrayDecorations = GetDecorationsForId(arrayId);
	ASSERT(arrayDecorations.HasArrayStride);
	auto arrayLength = arraySizeInBytes / SIMD::Int(arrayDecorations.ArrayStride);

	result.move(0, SIMD::Int(arrayLength));

	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitExtendedInstruction(InsnIterator insn, EmitState *state) const
{
	auto ext = getExtension(insn.word(3));
	switch(ext.name)
	{
	case Extension::GLSLstd450:
		return EmitExtGLSLstd450(insn, state);
	case Extension::OpenCLDebugInfo100:
		return EmitOpenCLDebugInfo100(insn, state);
	case Extension::NonSemanticInfo:
		// An extended set name which is prefixed with "NonSemantic." is
		// guaranteed to contain only non-semantic instructions and all
		// OpExtInst instructions referencing this set can be ignored.
		break;
	default:
		UNREACHABLE("Unknown Extension::Name<%d>", int(ext.name));
	}
	return EmitResult::Continue;
}

uint32_t SpirvShader::GetConstScalarInt(Object::ID id) const
{
	auto &scopeObj = getObject(id);
	ASSERT(scopeObj.kind == Object::Kind::Constant);
	ASSERT(getType(scopeObj).componentCount == 1);
	return scopeObj.constantValue[0];
}

void SpirvShader::emitEpilog(SpirvRoutine *routine) const
{
	for(auto insn : *this)
	{
		switch(insn.opcode())
		{
		case spv::OpVariable:
			{
				auto &object = getObject(insn.resultId());
				auto &objectTy = getType(object);
				if(object.kind == Object::Kind::InterfaceVariable && objectTy.storageClass == spv::StorageClassOutput)
				{
					auto &dst = routine->getVariable(insn.resultId());
					int offset = 0;
					VisitInterface(insn.resultId(),
					               [&](Decorations const &d, AttribType type) {
						               auto scalarSlot = d.Location << 2 | d.Component;
						               routine->outputs[scalarSlot] = dst[offset++];
					               });
				}
			}
			break;
		default:
			break;
		}
	}

	if(IsProfilingEnabled())
	{
		profiler->RegisterShaderForProfiling(std::to_string(insns.getIdentifier()) + "_" + std::to_string((uintptr_t)routine), std::move(routine->profData));
	}
}

void SpirvShader::clearPhis(SpirvRoutine *routine) const
{
	// Clear phis that are no longer used. This serves two purposes:
	// (1) The phi rr::Variables are destructed, preventing pointless
	//     materialization.
	// (2) Frees memory that will never be used again.
	routine->phis.clear();
}

VkShaderStageFlagBits SpirvShader::executionModelToStage(spv::ExecutionModel model)
{
	switch(model)
	{
	case spv::ExecutionModelVertex: return VK_SHADER_STAGE_VERTEX_BIT;
	// case spv::ExecutionModelTessellationControl:    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	// case spv::ExecutionModelTessellationEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	// case spv::ExecutionModelGeometry:               return VK_SHADER_STAGE_GEOMETRY_BIT;
	case spv::ExecutionModelFragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
	case spv::ExecutionModelGLCompute: return VK_SHADER_STAGE_COMPUTE_BIT;
	// case spv::ExecutionModelKernel:                 return VkShaderStageFlagBits(0); // Not supported by vulkan.
	// case spv::ExecutionModelTaskNV:                 return VK_SHADER_STAGE_TASK_BIT_NV;
	// case spv::ExecutionModelMeshNV:                 return VK_SHADER_STAGE_MESH_BIT_NV;
	// case spv::ExecutionModelRayGenerationNV:        return VK_SHADER_STAGE_RAYGEN_BIT_NV;
	// case spv::ExecutionModelIntersectionNV:         return VK_SHADER_STAGE_INTERSECTION_BIT_NV;
	// case spv::ExecutionModelAnyHitNV:               return VK_SHADER_STAGE_ANY_HIT_BIT_NV;
	// case spv::ExecutionModelClosestHitNV:           return VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
	// case spv::ExecutionModelMissNV:                 return VK_SHADER_STAGE_MISS_BIT_NV;
	// case spv::ExecutionModelCallableNV:             return VK_SHADER_STAGE_CALLABLE_BIT_NV;
	default:
		UNSUPPORTED("ExecutionModel: %d", int(model));
		return VkShaderStageFlagBits(0);
	}
}

SpirvShader::Operand::Operand(const SpirvShader *shader, const EmitState *state, SpirvShader::Object::ID objectId)
    : Operand(state, shader->getObject(objectId))
{}

SpirvShader::Operand::Operand(const EmitState *state, const Object &object)
    : constant(object.kind == SpirvShader::Object::Kind::Constant ? object.constantValue.data() : nullptr)
    , intermediate(object.kind == SpirvShader::Object::Kind::Intermediate ? &state->getIntermediate(object.id()) : nullptr)
    , componentCount(intermediate ? intermediate->componentCount : object.constantValue.size())
{
	ASSERT(intermediate || constant);
}

SpirvShader::Operand::Operand(const Intermediate &value)
    : constant(nullptr)
    , intermediate(&value)
    , componentCount(value.componentCount)
{
}

bool SpirvShader::Object::isConstantZero() const
{
	if(kind != Kind::Constant)
	{
		return false;
	}

	for(uint32_t i = 0; i < constantValue.size(); i++)
	{
		if(constantValue[i] != 0)
		{
			return false;
		}
	}

	return true;
}

SpirvRoutine::SpirvRoutine(vk::PipelineLayout const *pipelineLayout)
    : pipelineLayout(pipelineLayout)
{
}

void SpirvRoutine::setImmutableInputBuiltins(SpirvShader const *shader)
{
	setInputBuiltin(shader, spv::BuiltInSubgroupLocalInvocationId, [&](const SpirvShader::BuiltinMapping &builtin, Array<SIMD::Float> &value) {
		ASSERT(builtin.SizeInComponents == 1);
		value[builtin.FirstComponent] = As<SIMD::Float>(SIMD::Int(0, 1, 2, 3));
	});

	setInputBuiltin(shader, spv::BuiltInSubgroupEqMask, [&](const SpirvShader::BuiltinMapping &builtin, Array<SIMD::Float> &value) {
		ASSERT(builtin.SizeInComponents == 4);
		value[builtin.FirstComponent + 0] = As<SIMD::Float>(SIMD::Int(1, 2, 4, 8));
		value[builtin.FirstComponent + 1] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
		value[builtin.FirstComponent + 2] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
		value[builtin.FirstComponent + 3] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
	});

	setInputBuiltin(shader, spv::BuiltInSubgroupGeMask, [&](const SpirvShader::BuiltinMapping &builtin, Array<SIMD::Float> &value) {
		ASSERT(builtin.SizeInComponents == 4);
		value[builtin.FirstComponent + 0] = As<SIMD::Float>(SIMD::Int(15, 14, 12, 8));
		value[builtin.FirstComponent + 1] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
		value[builtin.FirstComponent + 2] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
		value[builtin.FirstComponent + 3] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
	});

	setInputBuiltin(shader, spv::BuiltInSubgroupGtMask, [&](const SpirvShader::BuiltinMapping &builtin, Array<SIMD::Float> &value) {
		ASSERT(builtin.SizeInComponents == 4);
		value[builtin.FirstComponent + 0] = As<SIMD::Float>(SIMD::Int(14, 12, 8, 0));
		value[builtin.FirstComponent + 1] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
		value[builtin.FirstComponent + 2] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
		value[builtin.FirstComponent + 3] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
	});

	setInputBuiltin(shader, spv::BuiltInSubgroupLeMask, [&](const SpirvShader::BuiltinMapping &builtin, Array<SIMD::Float> &value) {
		ASSERT(builtin.SizeInComponents == 4);
		value[builtin.FirstComponent + 0] = As<SIMD::Float>(SIMD::Int(1, 3, 7, 15));
		value[builtin.FirstComponent + 1] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
		value[builtin.FirstComponent + 2] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
		value[builtin.FirstComponent + 3] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
	});

	setInputBuiltin(shader, spv::BuiltInSubgroupLtMask, [&](const SpirvShader::BuiltinMapping &builtin, Array<SIMD::Float> &value) {
		ASSERT(builtin.SizeInComponents == 4);
		value[builtin.FirstComponent + 0] = As<SIMD::Float>(SIMD::Int(0, 1, 3, 7));
		value[builtin.FirstComponent + 1] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
		value[builtin.FirstComponent + 2] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
		value[builtin.FirstComponent + 3] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
	});

	setInputBuiltin(shader, spv::BuiltInDeviceIndex, [&](const SpirvShader::BuiltinMapping &builtin, Array<SIMD::Float> &value) {
		ASSERT(builtin.SizeInComponents == 1);
		// Only a single physical device is supported.
		value[builtin.FirstComponent] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
	});
}

}  // namespace sw
