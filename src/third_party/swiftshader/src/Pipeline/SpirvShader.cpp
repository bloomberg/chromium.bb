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

#include "SamplerCore.hpp"
#include "Reactor/Coroutine.hpp"
#include "System/Math.hpp"
#include "Vulkan/VkBuffer.hpp"
#include "Vulkan/VkBufferView.hpp"
#include "Vulkan/VkDebug.hpp"
#include "Vulkan/VkDescriptorSet.hpp"
#include "Vulkan/VkPipelineLayout.hpp"
#include "Vulkan/VkDescriptorSetLayout.hpp"
#include "Vulkan/VkRenderPass.hpp"
#include "Device/Config.hpp"

#include <spirv/unified1/spirv.hpp>
#include <spirv/unified1/GLSL.std.450.h>

namespace
{
	constexpr float PI = 3.141592653589793f;

	rr::RValue<rr::Bool> AnyTrue(rr::RValue<sw::SIMD::Int> const &ints)
	{
		return rr::SignMask(ints) != 0;
	}

	rr::RValue<rr::Bool> AnyFalse(rr::RValue<sw::SIMD::Int> const &ints)
	{
		return rr::SignMask(~ints) != 0;
	}

	rr::RValue<sw::SIMD::Float> Sign(rr::RValue<sw::SIMD::Float> const &val)
	{
		return rr::As<sw::SIMD::Float>((rr::As<sw::SIMD::UInt>(val) & sw::SIMD::UInt(0x80000000)) | sw::SIMD::UInt(0x3f800000));
	}

	// Returns the <whole, frac> of val.
	// Both whole and frac will have the same sign as val.
	std::pair<rr::RValue<sw::SIMD::Float>, rr::RValue<sw::SIMD::Float>>
	Modf(rr::RValue<sw::SIMD::Float> const &val)
	{
		auto abs = Abs(val);
		auto sign = Sign(val);
		auto whole = Floor(abs) * sign;
		auto frac = Frac(abs) * sign;
		return std::make_pair(whole, frac);
	}

	// Returns 1 << bits.
	// If the resulting bit overflows a 32 bit integer, 0 is returned.
	rr::RValue<sw::SIMD::UInt> NthBit32(rr::RValue<sw::SIMD::UInt> const &bits)
	{
		return ((sw::SIMD::UInt(1) << bits) & rr::CmpLT(bits, sw::SIMD::UInt(32)));
	}

	// Returns bitCount number of of 1's starting from the LSB.
	rr::RValue<sw::SIMD::UInt> Bitmask32(rr::RValue<sw::SIMD::UInt> const &bitCount)
	{
		return NthBit32(bitCount) - sw::SIMD::UInt(1);
	}

	// Performs a fused-multiply add, returning a * b + c.
	rr::RValue<sw::SIMD::Float> FMA(
			rr::RValue<sw::SIMD::Float> const &a,
			rr::RValue<sw::SIMD::Float> const &b,
			rr::RValue<sw::SIMD::Float> const &c)
	{
		return a * b + c;
	}

	// Returns the exponent of the floating point number f.
	// Assumes IEEE 754
	rr::RValue<sw::SIMD::Int> Exponent(rr::RValue<sw::SIMD::Float> f)
	{
		auto v = rr::As<sw::SIMD::UInt>(f);
		return (sw::SIMD::Int((v >> sw::SIMD::UInt(23)) & sw::SIMD::UInt(0xFF)) - sw::SIMD::Int(126));
	}

	// Returns y if y < x; otherwise result is x.
	// If one operand is a NaN, the other operand is the result.
	// If both operands are NaN, the result is a NaN.
	rr::RValue<sw::SIMD::Float> NMin(rr::RValue<sw::SIMD::Float> const &x, rr::RValue<sw::SIMD::Float> const &y)
	{
		using namespace rr;
		auto xIsNan = IsNan(x);
		auto yIsNan = IsNan(y);
		return As<sw::SIMD::Float>(
			// If neither are NaN, return min
			((~xIsNan & ~yIsNan) & As<sw::SIMD::Int>(Min(x, y))) |
			// If one operand is a NaN, the other operand is the result
			// If both operands are NaN, the result is a NaN.
			((~xIsNan &  yIsNan) & As<sw::SIMD::Int>(x)) |
			(( xIsNan          ) & As<sw::SIMD::Int>(y)));
	}

	// Returns y if y > x; otherwise result is x.
	// If one operand is a NaN, the other operand is the result.
	// If both operands are NaN, the result is a NaN.
	rr::RValue<sw::SIMD::Float> NMax(rr::RValue<sw::SIMD::Float> const &x, rr::RValue<sw::SIMD::Float> const &y)
	{
		using namespace rr;
		auto xIsNan = IsNan(x);
		auto yIsNan = IsNan(y);
		return As<sw::SIMD::Float>(
			// If neither are NaN, return max
			((~xIsNan & ~yIsNan) & As<sw::SIMD::Int>(Max(x, y))) |
			// If one operand is a NaN, the other operand is the result
			// If both operands are NaN, the result is a NaN.
			((~xIsNan &  yIsNan) & As<sw::SIMD::Int>(x)) |
			(( xIsNan          ) & As<sw::SIMD::Int>(y)));
	}

	// Returns the determinant of a 2x2 matrix.
	rr::RValue<sw::SIMD::Float> Determinant(
		rr::RValue<sw::SIMD::Float> const &a, rr::RValue<sw::SIMD::Float> const &b,
		rr::RValue<sw::SIMD::Float> const &c, rr::RValue<sw::SIMD::Float> const &d)
	{
		return a*d - b*c;
	}

	// Returns the determinant of a 3x3 matrix.
	rr::RValue<sw::SIMD::Float> Determinant(
		rr::RValue<sw::SIMD::Float> const &a, rr::RValue<sw::SIMD::Float> const &b, rr::RValue<sw::SIMD::Float> const &c,
		rr::RValue<sw::SIMD::Float> const &d, rr::RValue<sw::SIMD::Float> const &e, rr::RValue<sw::SIMD::Float> const &f,
		rr::RValue<sw::SIMD::Float> const &g, rr::RValue<sw::SIMD::Float> const &h, rr::RValue<sw::SIMD::Float> const &i)
	{
		return a*e*i + b*f*g + c*d*h - c*e*g - b*d*i - a*f*h;
	}

	// Returns the determinant of a 4x4 matrix.
	rr::RValue<sw::SIMD::Float> Determinant(
		rr::RValue<sw::SIMD::Float> const &a, rr::RValue<sw::SIMD::Float> const &b, rr::RValue<sw::SIMD::Float> const &c, rr::RValue<sw::SIMD::Float> const &d,
		rr::RValue<sw::SIMD::Float> const &e, rr::RValue<sw::SIMD::Float> const &f, rr::RValue<sw::SIMD::Float> const &g, rr::RValue<sw::SIMD::Float> const &h,
		rr::RValue<sw::SIMD::Float> const &i, rr::RValue<sw::SIMD::Float> const &j, rr::RValue<sw::SIMD::Float> const &k, rr::RValue<sw::SIMD::Float> const &l,
		rr::RValue<sw::SIMD::Float> const &m, rr::RValue<sw::SIMD::Float> const &n, rr::RValue<sw::SIMD::Float> const &o, rr::RValue<sw::SIMD::Float> const &p)
	{
		return a * Determinant(f, g, h,
		                       j, k, l,
		                       n, o, p) -
		       b * Determinant(e, g, h,
		                       i, k, l,
		                       m, o, p) +
		       c * Determinant(e, f, h,
		                       i, j, l,
		                       m, n, p) -
		       d * Determinant(e, f, g,
		                       i, j, k,
		                       m, n, o);
	}

	// Returns the inverse of a 2x2 matrix.
	std::array<rr::RValue<sw::SIMD::Float>, 4> MatrixInverse(
		rr::RValue<sw::SIMD::Float> const &a, rr::RValue<sw::SIMD::Float> const &b,
		rr::RValue<sw::SIMD::Float> const &c, rr::RValue<sw::SIMD::Float> const &d)
	{
		auto s = sw::SIMD::Float(1.0f) / Determinant(a, b, c, d);
		return {{s*d, -s*b, -s*c, s*a}};
	}

	// Returns the inverse of a 3x3 matrix.
	std::array<rr::RValue<sw::SIMD::Float>, 9> MatrixInverse(
		rr::RValue<sw::SIMD::Float> const &a, rr::RValue<sw::SIMD::Float> const &b, rr::RValue<sw::SIMD::Float> const &c,
		rr::RValue<sw::SIMD::Float> const &d, rr::RValue<sw::SIMD::Float> const &e, rr::RValue<sw::SIMD::Float> const &f,
		rr::RValue<sw::SIMD::Float> const &g, rr::RValue<sw::SIMD::Float> const &h, rr::RValue<sw::SIMD::Float> const &i)
	{
		auto s = sw::SIMD::Float(1.0f) / Determinant(
				a, b, c,
				d, e, f,
				g, h, i); // TODO: duplicate arithmetic calculating the det and below.

		return {{
			s * (e*i - f*h), s * (c*h - b*i), s * (b*f - c*e),
			s * (f*g - d*i), s * (a*i - c*g), s * (c*d - a*f),
			s * (d*h - e*g), s * (b*g - a*h), s * (a*e - b*d),
		}};
	}

	// Returns the inverse of a 4x4 matrix.
	std::array<rr::RValue<sw::SIMD::Float>, 16> MatrixInverse(
		rr::RValue<sw::SIMD::Float> const &a, rr::RValue<sw::SIMD::Float> const &b, rr::RValue<sw::SIMD::Float> const &c, rr::RValue<sw::SIMD::Float> const &d,
		rr::RValue<sw::SIMD::Float> const &e, rr::RValue<sw::SIMD::Float> const &f, rr::RValue<sw::SIMD::Float> const &g, rr::RValue<sw::SIMD::Float> const &h,
		rr::RValue<sw::SIMD::Float> const &i, rr::RValue<sw::SIMD::Float> const &j, rr::RValue<sw::SIMD::Float> const &k, rr::RValue<sw::SIMD::Float> const &l,
		rr::RValue<sw::SIMD::Float> const &m, rr::RValue<sw::SIMD::Float> const &n, rr::RValue<sw::SIMD::Float> const &o, rr::RValue<sw::SIMD::Float> const &p)
	{
		auto s = sw::SIMD::Float(1.0f) / Determinant(
				a, b, c, d,
				e, f, g, h,
				i, j, k, l,
				m, n, o, p); // TODO: duplicate arithmetic calculating the det and below.

		auto kplo = k*p - l*o, jpln = j*p - l*n, jokn = j*o - k*n;
		auto gpho = g*p - h*o, fphn = f*p - h*n, fogn = f*o - g*n;
		auto glhk = g*l - h*k, flhj = f*l - h*j, fkgj = f*k - g*j;
		auto iplm = i*p - l*m, iokm = i*o - k*m, ephm = e*p - h*m;
		auto eogm = e*o - g*m, elhi = e*l - h*i, ekgi = e*k - g*i;
		auto injm = i*n - j*m, enfm = e*n - f*m, ejfi = e*j - f*i;

		return {{
			s * ( f * kplo - g * jpln + h * jokn),
			s * (-b * kplo + c * jpln - d * jokn),
			s * ( b * gpho - c * fphn + d * fogn),
			s * (-b * glhk + c * flhj - d * fkgj),

			s * (-e * kplo + g * iplm - h * iokm),
			s * ( a * kplo - c * iplm + d * iokm),
			s * (-a * gpho + c * ephm - d * eogm),
			s * ( a * glhk - c * elhi + d * ekgi),

			s * ( e * jpln - f * iplm + h * injm),
			s * (-a * jpln + b * iplm - d * injm),
			s * ( a * fphn - b * ephm + d * enfm),
			s * (-a * flhj + b * elhi - d * ejfi),

			s * (-e * jokn + f * iokm - g * injm),
			s * ( a * jokn - b * iokm + c * injm),
			s * (-a * fogn + b * eogm - c * enfm),
			s * ( a * fkgj - b * ekgi + c * ejfi),
		}};
	}


	sw::SIMD::Pointer interleaveByLane(sw::SIMD::Pointer p)
	{
		p *= sw::SIMD::Width;
		p.staticOffsets[0] += 0 * sizeof(float);
		p.staticOffsets[1] += 1 * sizeof(float);
		p.staticOffsets[2] += 2 * sizeof(float);
		p.staticOffsets[3] += 3 * sizeof(float);
		return p;
	}

	VkFormat SpirvFormatToVulkanFormat(spv::ImageFormat format)
	{
		switch (format)
		{
		case spv::ImageFormatRgba32f: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case spv::ImageFormatRgba32i: return VK_FORMAT_R32G32B32A32_SINT;
		case spv::ImageFormatRgba32ui: return VK_FORMAT_R32G32B32A32_UINT;
		case spv::ImageFormatR32f: return VK_FORMAT_R32_SFLOAT;
		case spv::ImageFormatR32i: return VK_FORMAT_R32_SINT;
		case spv::ImageFormatR32ui: return VK_FORMAT_R32_UINT;
		case spv::ImageFormatRgba8: return VK_FORMAT_R8G8B8A8_UNORM;
		case spv::ImageFormatRgba8Snorm: return VK_FORMAT_R8G8B8A8_SNORM;
		case spv::ImageFormatRgba8i: return VK_FORMAT_R8G8B8A8_SINT;
		case spv::ImageFormatRgba8ui: return VK_FORMAT_R8G8B8A8_UINT;
		case spv::ImageFormatRgba16f: return VK_FORMAT_R16G16B16A16_SFLOAT;
		case spv::ImageFormatRgba16i: return VK_FORMAT_R16G16B16A16_SINT;
		case spv::ImageFormatRgba16ui: return VK_FORMAT_R16G16B16A16_UINT;

		default:
			UNIMPLEMENTED("SPIR-V ImageFormat %u", format);
			return VK_FORMAT_UNDEFINED;
		}
	}

	sw::SIMD::Float sRGBtoLinear(sw::SIMD::Float c)
	{
		sw::SIMD::Float lc = c * sw::SIMD::Float(1.0f / 12.92f);
		sw::SIMD::Float ec = sw::power((c + sw::SIMD::Float(0.055f)) * sw::SIMD::Float(1.0f / 1.055f), sw::SIMD::Float(2.4f));

		sw::SIMD::Int linear = CmpLT(c, sw::SIMD::Float(0.04045f));

		sw::SIMD::Float s = c;
		s.xyz = rr::As<sw::SIMD::Float>((linear & rr::As<sw::SIMD::Int>(lc)) | (~linear & rr::As<sw::SIMD::Int>(ec)));   // TODO: IfThenElse()

		return s;
	}

} // anonymous namespace

namespace sw
{
	namespace SIMD
	{

		template<typename T>
		T Load(Pointer ptr, Int mask, bool atomic /* = false */, std::memory_order order /* = std::memory_order_relaxed */)
		{
			using EL = typename Element<T>::type;
			auto offsets = ptr.offsets();
			mask &= CmpLT(offsets + SIMD::Int(sizeof(float) - 1), SIMD::Int(ptr.limit)); // Disable OOB reads.
			if (!atomic && order == std::memory_order_relaxed)
			{
				return rr::Gather(rr::Pointer<EL>(ptr.base), offsets, mask, sizeof(float));
			}
			else
			{
				T out;
				auto anyLanesDisabled = AnyFalse(mask);
				If(ptr.hasEqualOffsets() && !anyLanesDisabled)
				{
					// Load one, replicate.
					auto offset = Extract(offsets, 0);
					out = T(rr::Load(rr::Pointer<EL>(&ptr.base[offset]), sizeof(float), atomic, order));
				}
				Else If(ptr.hasSequentialOffsets() && !anyLanesDisabled)
				{
					// Load all elements in a single SIMD instruction.
					auto offset = Extract(offsets, 0);
					out = rr::Load(rr::Pointer<T>(&ptr.base[offset]), sizeof(float), atomic, order);
				}
				Else
				{
					// Divergent offsets or masked lanes.
					out = T(0);
					for (int i = 0; i < SIMD::Width; i++)
					{
						If(Extract(mask, i) != 0)
						{
							auto offset = Extract(offsets, i);
							auto el = rr::Load(rr::Pointer<EL>(&ptr.base[offset]), sizeof(float), atomic, order);
							out = Insert(out, el, i);
						}
					}
				}
				return out;
			}
		}

		template<typename T>
		void Store(Pointer ptr, T val, Int mask, bool atomic /* = false */, std::memory_order order /* = std::memory_order_relaxed */)
		{
			using EL = typename Element<T>::type;
			auto offsets = ptr.offsets();
			mask &= CmpLT(offsets + SIMD::Int(sizeof(float) - 1), SIMD::Int(ptr.limit)); // Disable OOB reads.
			if (!atomic && order == std::memory_order_relaxed)
			{
				return rr::Scatter(rr::Pointer<EL>(ptr.base), val, offsets, mask, sizeof(float));
			}
			else
			{
				auto anyLanesDisabled = AnyFalse(mask);
				If(ptr.hasSequentialOffsets() && !anyLanesDisabled)
				{
					// Store all elements in a single SIMD instruction.
					auto offset = Extract(offsets, 0);
					Store(val, rr::Pointer<T>(&ptr.base[offset]), sizeof(float), atomic, order);
				}
				Else
				{
					// Divergent offsets or masked lanes.
					for (int i = 0; i < SIMD::Width; i++)
					{
						If(Extract(mask, i) != 0)
						{
							auto offset = Extract(offsets, i);
							rr::Store(Extract(val, i), rr::Pointer<EL>(&ptr.base[offset]), sizeof(float), atomic, order);
						}
					}
				}
			}
		}

	} // namespace SIMD

	std::atomic<int> SpirvShader::serialCounter(1);    // Start at 1, 0 is invalid shader.

	SpirvShader::SpirvShader(
			VkPipelineShaderStageCreateInfo const *createInfo,
			InsnStore const &insns,
			vk::RenderPass *renderPass,
			uint32_t subpassIndex)
				: insns{insns}, inputs{MAX_INTERFACE_COMPONENTS},
				outputs{MAX_INTERFACE_COMPONENTS},
				serialID{serialCounter++}, modes{}
	{
		ASSERT(insns.size() > 0);

		if (renderPass != VK_NULL_HANDLE)
		{
			// capture formats of any input attachments present
			auto subpass = renderPass->getSubpass(subpassIndex);
			inputAttachmentFormats.reserve(subpass.inputAttachmentCount);
			for (auto i = 0u; i < subpass.inputAttachmentCount; i++)
			{
				auto attachmentIndex = subpass.pInputAttachments[i].attachment;
				inputAttachmentFormats.push_back(attachmentIndex != VK_ATTACHMENT_UNUSED
												 ? renderPass->getAttachment(attachmentIndex).format : VK_FORMAT_UNDEFINED);
			}
		}

		// Simplifying assumptions (to be satisfied by earlier transformations)
		// - The only input/output OpVariables present are those used by the entrypoint

		Object::ID entryPointFunctionId;
		Block::ID currentBlock;
		InsnIterator blockStart;

		for (auto insn : *this)
		{
			spv::Op opcode = insn.opcode();

			switch (opcode)
			{
			case spv::OpEntryPoint:
			{
				auto executionModel = spv::ExecutionModel(insn.word(1));
				auto id = Object::ID(insn.word(2));
				auto name = insn.string(3);
				auto stage = executionModelToStage(executionModel);
				if (stage == createInfo->stage && strcmp(name, createInfo->pName) == 0)
				{
					ASSERT_MSG(entryPointFunctionId == 0, "Duplicate entry point with name '%s' and stage %d", name, int(stage));
					entryPointFunctionId = id;
				}
				break;
			}

			case spv::OpExecutionMode:
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
				default:
					// Only handling descriptor decorations here.
					break;
				}

				if (decoration == spv::DecorationCentroid)
					modes.NeedsCentroid = true;
				break;
			}

			case spv::OpMemberDecorate:
			{
				Type::ID targetId = insn.word(1);
				auto memberIndex = insn.word(2);
				auto decoration = static_cast<spv::Decoration>(insn.word(3));
				uint32_t value = insn.wordCount() > 4 ? insn.word(4) : 0;

				auto &d = memberDecorations[targetId];
				if (memberIndex >= d.size())
					d.resize(memberIndex + 1);    // on demand; exact size would require another pass...

				d[memberIndex].Apply(decoration, value);

				if (decoration == spv::DecorationCentroid)
					modes.NeedsCentroid = true;
				break;
			}

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
				for (auto i = 2u; i < insn.wordCount(); i++)
				{
					// Remaining operands are targets to apply the group to.
					uint32_t target = insn.word(i);
					decorations[target].Apply(groupDecorations);
					descriptorDecorations[target].Apply(descriptorGroupDecorations);
				}

				break;
			}

			case spv::OpGroupMemberDecorate:
			{
				auto const &srcDecorations = decorations[insn.word(1)];
				for (auto i = 2u; i < insn.wordCount(); i += 2)
				{
					// remaining operands are pairs of <id>, literal for members to apply to.
					auto &d = memberDecorations[insn.word(i)];
					auto memberIndex = insn.word(i + 1);
					if (memberIndex >= d.size())
						d.resize(memberIndex + 1);    // on demand resize, see above...
					d[memberIndex].Apply(srcDecorations);
				}
				break;
			}

			case spv::OpLabel:
			{
				ASSERT(currentBlock.value() == 0);
				currentBlock = Block::ID(insn.word(1));
				blockStart = insn;
				break;
			}

			// Branch Instructions (subset of Termination Instructions):
			case spv::OpBranch:
			case spv::OpBranchConditional:
			case spv::OpSwitch:
			case spv::OpReturn:
			// fallthrough

			// Termination instruction:
			case spv::OpKill:
			case spv::OpUnreachable:
			{
				ASSERT(currentBlock.value() != 0);
				auto blockEnd = insn; blockEnd++;
				blocks[currentBlock] = Block(blockStart, blockEnd);
				currentBlock = Block::ID(0);

				if (opcode == spv::OpKill)
				{
					modes.ContainsKill = true;
				}
				break;
			}

			case spv::OpLoopMerge:
			case spv::OpSelectionMerge:
				break; // Nothing to do in analysis pass.

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
				object.type = typeId;

				ASSERT(getType(typeId).definition.opcode() == spv::OpTypePointer);
				ASSERT(getType(typeId).storageClass == storageClass);

				switch (storageClass)
				{
				case spv::StorageClassInput:
				case spv::StorageClassOutput:
					ProcessInterfaceVariable(object);
					break;

				case spv::StorageClassUniform:
				case spv::StorageClassStorageBuffer:
					object.kind = Object::Kind::DescriptorSet;
					break;

				case spv::StorageClassPushConstant:
				case spv::StorageClassPrivate:
				case spv::StorageClassFunction:
				case spv::StorageClassUniformConstant:
					break; // Correctly handled.

				case spv::StorageClassWorkgroup:
				{
					auto &elTy = getType(getType(typeId).element);
					auto sizeInBytes = elTy.sizeInComponents * sizeof(float);
					workgroupMemory.allocate(resultId, sizeInBytes);
					object.kind = Object::Kind::Pointer;
					break;
				}
				case spv::StorageClassAtomicCounter:
				case spv::StorageClassImage:
					UNIMPLEMENTED("StorageClass %d not yet implemented", (int)storageClass);
					break;

				case spv::StorageClassCrossWorkgroup:
					UNSUPPORTED("SPIR-V OpenCL Execution Model (StorageClassCrossWorkgroup)");
					break;

				case spv::StorageClassGeneric:
					UNSUPPORTED("SPIR-V GenericPointer Capability (StorageClassGeneric)");
					break;

				default:
					UNREACHABLE("Unexpected StorageClass %d", storageClass); // See Appendix A of the Vulkan spec.
					break;
				}
				break;
			}

			case spv::OpConstant:
			case spv::OpSpecConstant:
				CreateConstant(insn).constantValue[0] = insn.word(3);
				break;
			case spv::OpConstantFalse:
			case spv::OpSpecConstantFalse:
				CreateConstant(insn).constantValue[0] = 0;		// represent boolean false as zero
				break;
			case spv::OpConstantTrue:
			case spv::OpSpecConstantTrue:
				CreateConstant(insn).constantValue[0] = ~0u;	// represent boolean true as all bits set
				break;
			case spv::OpConstantNull:
			case spv::OpUndef:
			{
				// TODO: consider a real LLVM-level undef. For now, zero is a perfectly good value.
				// OpConstantNull forms a constant of arbitrary type, all zeros.
				auto &object = CreateConstant(insn);
				auto &objectTy = getType(object.type);
				for (auto i = 0u; i < objectTy.sizeInComponents; i++)
				{
					object.constantValue[i] = 0;
				}
				break;
			}
			case spv::OpConstantComposite:
			case spv::OpSpecConstantComposite:
			{
				auto &object = CreateConstant(insn);
				auto offset = 0u;
				for (auto i = 0u; i < insn.wordCount() - 3; i++)
				{
					auto &constituent = getObject(insn.word(i + 3));
					auto &constituentTy = getType(constituent.type);
					for (auto j = 0u; j < constituentTy.sizeInComponents; j++)
						object.constantValue[offset++] = constituent.constantValue[j];
				}

				auto objectId = Object::ID(insn.word(2));
				auto decorationsIt = decorations.find(objectId);
				if (decorationsIt != decorations.end() &&
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
					ASSERT(getType(object.type).sizeInComponents == 3);
					modes.WorkgroupSizeX = object.constantValue[0];
					modes.WorkgroupSizeY = object.constantValue[1];
					modes.WorkgroupSizeZ = object.constantValue[2];
				}
				break;
			}
			case spv::OpSpecConstantOp:
				EvalSpecConstantOp(insn);
				break;

			case spv::OpCapability:
			{
				auto capability = static_cast<spv::Capability>(insn.word(1));
				switch (capability)
				{
				case spv::CapabilityMatrix:
				case spv::CapabilityShader:
				case spv::CapabilityInputAttachment:
				case spv::CapabilitySampled1D:
				case spv::CapabilityImage1D:
				case spv::CapabilitySampledBuffer:
				case spv::CapabilityImageBuffer:
				case spv::CapabilityImageQuery:
				case spv::CapabilityDerivativeControl:
				case spv::CapabilityMultiView:
				case spv::CapabilityDeviceGroup:
				case spv::CapabilityGroupNonUniform:
					break;

				default:
					UNSUPPORTED("Unsupported capability %u", insn.word(1));
				}
				break; // Various capabilities will be declared, but none affect our code generation at this point.
			}

			case spv::OpMemoryModel:
				break; // Memory model does not affect our code generation until we decide to do Vulkan Memory Model support.

			case spv::OpFunction:
			{
				auto functionId = Object::ID(insn.word(2));
				if (functionId == entryPointFunctionId)
				{
					// Scan forward to find the function's label.
					for (auto it = insn; it != end() && entryPointBlockId == 0; it++)
					{
						switch (it.opcode())
						{
						case spv::OpFunction:
						case spv::OpFunctionParameter:
							break;
						case spv::OpLabel:
							entryPointBlockId = Block::ID(it.word(1));
							break;
						default:
							WARN("Unexpected opcode '%s' following OpFunction", OpcodeName(it.opcode()).c_str());
						}
					}
				}
				else
				{
					// All non-entry point functions should be inlined into an
					// entry point function.
					// This isn't the target entry point, so must be another
					// entry point that we are not interested in. Just skip it.
					for (; insn != end() && insn.opcode() != spv::OpFunctionEnd; insn++) {}
				}

				break;
			}
			case spv::OpFunctionEnd:
				// Due to preprocessing, the entrypoint and its function provide no value.
				break;
			case spv::OpExtInstImport:
			{
				// We will only support the GLSL 450 extended instruction set, so no point in tracking the ID we assign it.
				// Valid shaders will not attempt to import any other instruction sets.
				auto ext = insn.string(2);
				if (0 != strcmp("GLSL.std.450", ext))
				{
					UNSUPPORTED("SPIR-V Extension: %s", ext);
				}
				break;
			}
			case spv::OpName:
			case spv::OpMemberName:
			case spv::OpSource:
			case spv::OpSourceContinued:
			case spv::OpSourceExtension:
			case spv::OpLine:
			case spv::OpNoLine:
			case spv::OpModuleProcessed:
			case spv::OpString:
				// No semantic impact
				break;

			case spv::OpFunctionParameter:
			case spv::OpFunctionCall:
				// These should have all been removed by preprocessing passes. If we see them here,
				// our assumptions are wrong and we will probably generate wrong code.
				UNREACHABLE("%s should have already been lowered.", OpcodeName(opcode).c_str());
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

					if (opcode == spv::OpAccessChain || opcode == spv::OpInBoundsAccessChain)
					{
						Decorations dd{};
						ApplyDecorationsForAccessChain(&dd, &descriptorDecorations[resultId], pointerId, insn.wordCount() - 4, insn.wordPointer(4));
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
			case spv::OpConvertFToU:
			case spv::OpConvertFToS:
			case spv::OpConvertSToF:
			case spv::OpConvertUToF:
			case spv::OpBitcast:
			case spv::OpSelect:
			case spv::OpExtInst:
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
			case spv::OpCopyObject:
			case spv::OpArrayLength:
				// Instructions that yield an intermediate value or divergent pointer
				DefineResult(insn);
				break;

			case spv::OpStore:
			case spv::OpAtomicStore:
			case spv::OpImageWrite:
			case spv::OpCopyMemory:
			case spv::OpMemoryBarrier:
				// Don't need to do anything during analysis pass
				break;

			case spv::OpControlBarrier:
				modes.ContainsControlBarriers = true;
				break;

			case spv::OpExtension:
			{
				auto ext = insn.string(1);
				// Part of core SPIR-V 1.3. Vulkan 1.1 implementations must also accept the pre-1.3
				// extension per Appendix A, `Vulkan Environment for SPIR-V`.
				if (!strcmp(ext, "SPV_KHR_storage_buffer_storage_class")) break;
				if (!strcmp(ext, "SPV_KHR_shader_draw_parameters")) break;
				if (!strcmp(ext, "SPV_KHR_16bit_storage")) break;
				if (!strcmp(ext, "SPV_KHR_variable_pointers")) break;
				if (!strcmp(ext, "SPV_KHR_device_group")) break;
				UNSUPPORTED("SPIR-V Extension: %s", ext);
				break;
			}

			default:
				UNIMPLEMENTED("%s", OpcodeName(opcode).c_str());
			}
		}

		ASSERT_MSG(entryPointFunctionId != 0, "Entry point '%s' not found", createInfo->pName);
		AssignBlockFields();
	}

	void SpirvShader::TraverseReachableBlocks(Block::ID id, SpirvShader::Block::Set& reachable)
	{
		if (reachable.count(id) == 0)
		{
			reachable.emplace(id);
			for (auto out : getBlock(id).outs)
			{
				TraverseReachableBlocks(out, reachable);
			}
		}
	}

	void SpirvShader::AssignBlockFields()
	{
		Block::Set reachable;
		TraverseReachableBlocks(entryPointBlockId, reachable);

		for (auto &it : blocks)
		{
			auto &blockId = it.first;
			auto &block = it.second;
			if (reachable.count(blockId) > 0)
			{
				for (auto &outId : it.second.outs)
				{
					auto outIt = blocks.find(outId);
					ASSERT_MSG(outIt != blocks.end(), "Block %d has a non-existent out %d", blockId.value(), outId.value());
					auto &out = outIt->second;
					out.ins.emplace(blockId);
				}
				if (block.kind == Block::Loop)
				{
					auto mergeIt = blocks.find(block.mergeBlock);
					ASSERT_MSG(mergeIt != blocks.end(), "Loop block %d has a non-existent merge block %d", blockId.value(), block.mergeBlock.value());
					mergeIt->second.isLoopMerge = true;
				}
			}
		}
	}

	void SpirvShader::DeclareType(InsnIterator insn)
	{
		Type::ID resultId = insn.word(1);

		auto &type = types[resultId];
		type.definition = insn;
		type.sizeInComponents = ComputeTypeSize(insn);

		// A structure is a builtin block if it has a builtin
		// member. All members of such a structure are builtins.
		switch (insn.opcode())
		{
		case spv::OpTypeStruct:
		{
			auto d = memberDecorations.find(resultId);
			if (d != memberDecorations.end())
			{
				for (auto &m : d->second)
				{
					if (m.HasBuiltIn)
					{
						type.isBuiltInBlock = true;
						break;
					}
				}
			}
			break;
		}
		case spv::OpTypePointer:
		{
			Type::ID elementTypeId = insn.word(3);
			type.element = elementTypeId;
			type.isBuiltInBlock = getType(elementTypeId).isBuiltInBlock;
			type.storageClass = static_cast<spv::StorageClass>(insn.word(2));
			break;
		}
		case spv::OpTypeVector:
		case spv::OpTypeMatrix:
		case spv::OpTypeArray:
		case spv::OpTypeRuntimeArray:
		{
			Type::ID elementTypeId = insn.word(2);
			type.element = elementTypeId;
			break;
		}
		default:
			break;
		}
	}

	SpirvShader::Object& SpirvShader::CreateConstant(InsnIterator insn)
	{
		Type::ID typeId = insn.word(1);
		Object::ID resultId = insn.word(2);
		auto &object = defs[resultId];
		auto &objectTy = getType(typeId);
		object.type = typeId;
		object.kind = Object::Kind::Constant;
		object.definition = insn;
		object.constantValue = std::unique_ptr<uint32_t[]>(new uint32_t[objectTy.sizeInComponents]);
		return object;
	}

	void SpirvShader::ProcessInterfaceVariable(Object &object)
	{
		auto &objectTy = getType(object.type);
		ASSERT(objectTy.storageClass == spv::StorageClassInput || objectTy.storageClass == spv::StorageClassOutput);

		ASSERT(objectTy.opcode() == spv::OpTypePointer);
		auto pointeeTy = getType(objectTy.element);

		auto &builtinInterface = (objectTy.storageClass == spv::StorageClassInput) ? inputBuiltins : outputBuiltins;
		auto &userDefinedInterface = (objectTy.storageClass == spv::StorageClassInput) ? inputs : outputs;

		ASSERT(object.opcode() == spv::OpVariable);
		Object::ID resultId = object.definition.word(2);

		if (objectTy.isBuiltInBlock)
		{
			// walk the builtin block, registering each of its members separately.
			auto m = memberDecorations.find(objectTy.element);
			ASSERT(m != memberDecorations.end());        // otherwise we wouldn't have marked the type chain
			auto &structType = pointeeTy.definition;
			auto offset = 0u;
			auto word = 2u;
			for (auto &member : m->second)
			{
				auto &memberType = getType(structType.word(word));

				if (member.HasBuiltIn)
				{
					builtinInterface[member.BuiltIn] = {resultId, offset, memberType.sizeInComponents};
				}

				offset += memberType.sizeInComponents;
				++word;
			}
			return;
		}

		auto d = decorations.find(resultId);
		if (d != decorations.end() && d->second.HasBuiltIn)
		{
			builtinInterface[d->second.BuiltIn] = {resultId, 0, pointeeTy.sizeInComponents};
		}
		else
		{
			object.kind = Object::Kind::InterfaceVariable;
			VisitInterface(resultId,
						   [&userDefinedInterface](Decorations const &d, AttribType type) {
							   // Populate a single scalar slot in the interface from a collection of decorations and the intended component type.
							   auto scalarSlot = (d.Location << 2) | d.Component;
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

	void SpirvShader::ProcessExecutionMode(InsnIterator insn)
	{
		auto mode = static_cast<spv::ExecutionMode>(insn.word(2));
		switch (mode)
		{
		case spv::ExecutionModeEarlyFragmentTests:
			modes.EarlyFragmentTests = true;
			break;
		case spv::ExecutionModeDepthReplacing:
			modes.DepthReplacing = true;
			break;
		case spv::ExecutionModeDepthGreater:
			modes.DepthGreater = true;
			break;
		case spv::ExecutionModeDepthLess:
			modes.DepthLess = true;
			break;
		case spv::ExecutionModeDepthUnchanged:
			modes.DepthUnchanged = true;
			break;
		case spv::ExecutionModeLocalSize:
			modes.WorkgroupSizeX = insn.word(3);
			modes.WorkgroupSizeY = insn.word(4);
			modes.WorkgroupSizeZ = insn.word(5);
			break;
		case spv::ExecutionModeOriginUpperLeft:
			// This is always the case for a Vulkan shader. Do nothing.
			break;
		default:
			UNREACHABLE("Execution mode: %d", int(mode));
		}
	}

	uint32_t SpirvShader::ComputeTypeSize(InsnIterator insn)
	{
		// Types are always built from the bottom up (with the exception of forward ptrs, which
		// don't appear in Vulkan shaders. Therefore, we can always assume our component parts have
		// already been described (and so their sizes determined)
		switch (insn.opcode())
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
			return getType(insn.word(2)).sizeInComponents * insn.word(3);

		case spv::OpTypeArray:
		{
			// Element count * element size. Array sizes come from constant ids.
			auto arraySize = GetConstScalarInt(insn.word(3));
			return getType(insn.word(2)).sizeInComponents * arraySize;
		}

		case spv::OpTypeStruct:
		{
			uint32_t size = 0;
			for (uint32_t i = 2u; i < insn.wordCount(); i++)
			{
				size += getType(insn.word(i)).sizeInComponents;
			}
			return size;
		}

		case spv::OpTypePointer:
			// Runtime representation of a pointer is a per-lane index.
			// Note: clients are expected to look through the pointer if they want the pointee size instead.
			return 1;

		default:
			UNREACHABLE("%s", OpcodeName(insn.opcode()).c_str());
			return 0;
		}
	}

	bool SpirvShader::IsExplicitLayout(spv::StorageClass storageClass)
	{
		switch (storageClass)
		{
		case spv::StorageClassUniform:
		case spv::StorageClassStorageBuffer:
		case spv::StorageClassPushConstant:
			return true;
		default:
			return false;
		}
	}

	bool SpirvShader::IsStorageInterleavedByLane(spv::StorageClass storageClass)
	{
		switch (storageClass)
		{
		case spv::StorageClassUniform:
		case spv::StorageClassStorageBuffer:
		case spv::StorageClassPushConstant:
		case spv::StorageClassWorkgroup:
			return false;
		default:
			return true;
		}
	}

	template<typename F>
	int SpirvShader::VisitInterfaceInner(Type::ID id, Decorations d, F f) const
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
			return VisitInterfaceInner<F>(obj.definition.word(3), d, f);
		case spv::OpTypeMatrix:
			for (auto i = 0u; i < obj.definition.word(3); i++, d.Location++)
			{
				// consumes same components of N consecutive locations
				VisitInterfaceInner<F>(obj.definition.word(2), d, f);
			}
			return d.Location;
		case spv::OpTypeVector:
			for (auto i = 0u; i < obj.definition.word(3); i++, d.Component++)
			{
				// consumes N consecutive components in the same location
				VisitInterfaceInner<F>(obj.definition.word(2), d, f);
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
			for (auto i = 0u; i < obj.definition.wordCount() - 2; i++)
			{
				ApplyDecorationsForIdMember(&d, id, i);
				d.Location = VisitInterfaceInner<F>(obj.definition.word(i + 2), d, f);
				d.Component = 0;    // Implicit locations always have component=0
			}
			return d.Location;
		}
		case spv::OpTypeArray:
		{
			auto arraySize = GetConstScalarInt(obj.definition.word(3));
			for (auto i = 0u; i < arraySize; i++)
			{
				d.Location = VisitInterfaceInner<F>(obj.definition.word(2), d, f);
			}
			return d.Location;
		}
		default:
			// Intentionally partial; most opcodes do not participate in type hierarchies
			return 0;
		}
	}

	template<typename F>
	void SpirvShader::VisitInterface(Object::ID id, F f) const
	{
		// Walk a variable definition and call f for each component in it.
		Decorations d{};
		ApplyDecorationsForId(&d, id);

		auto def = getObject(id).definition;
		ASSERT(def.opcode() == spv::OpVariable);
		VisitInterfaceInner<F>(def.word(1), d, f);
	}

	template<typename F>
	void SpirvShader::VisitMemoryObjectInner(sw::SpirvShader::Type::ID id, sw::SpirvShader::Decorations d, uint32_t& index, uint32_t offset, F f) const
	{
		// Walk a type tree in an explicitly laid out storage class, calling
		// a functor for each scalar element within the object.

		// The functor's first parameter is the index of the scalar element;
		// the second parameter is the offset (in bytes) from the base of the
		// object.

		ApplyDecorationsForId(&d, id);
		auto const &type = getType(id);

		if (d.HasOffset)
		{
			offset += d.Offset;
			d.HasOffset = false;
		}

		switch (type.opcode())
		{
		case spv::OpTypePointer:
			VisitMemoryObjectInner<F>(type.definition.word(3), d, index, offset, f);
			break;
		case spv::OpTypeInt:
		case spv::OpTypeFloat:
			f(index++, offset);
			break;
		case spv::OpTypeVector:
		{
			auto elemStride = (d.InsideMatrix && d.HasRowMajor && d.RowMajor) ? d.MatrixStride : sizeof(float);
			for (auto i = 0u; i < type.definition.word(3); i++)
			{
				VisitMemoryObjectInner(type.definition.word(2), d, index, offset + elemStride * i, f);
			}
			break;
		}
		case spv::OpTypeMatrix:
		{
			auto columnStride = (d.HasRowMajor && d.RowMajor) ? sizeof(float) : d.MatrixStride;
			d.InsideMatrix = true;
			for (auto i = 0u; i < type.definition.word(3); i++)
			{
				ASSERT(d.HasMatrixStride);
				VisitMemoryObjectInner(type.definition.word(2), d, index, offset + columnStride * i, f);
			}
			break;
		}
		case spv::OpTypeStruct:
			for (auto i = 0u; i < type.definition.wordCount() - 2; i++)
			{
				ApplyDecorationsForIdMember(&d, id, i);
				VisitMemoryObjectInner<F>(type.definition.word(i + 2), d, index, offset, f);
			}
			break;
		case spv::OpTypeArray:
		{
			auto arraySize = GetConstScalarInt(type.definition.word(3));
			for (auto i = 0u; i < arraySize; i++)
			{
				ASSERT(d.HasArrayStride);
				VisitMemoryObjectInner<F>(type.definition.word(2), d, index, offset + i * d.ArrayStride, f);
			}
			break;
		}
		default:
			UNREACHABLE("%s", OpcodeName(type.opcode()).c_str());
		}
	}

	template<typename F>
	void SpirvShader::VisitMemoryObject(sw::SpirvShader::Object::ID id, F f) const
	{
		auto typeId = getObject(id).type;
		auto const & type = getType(typeId);
		if (IsExplicitLayout(type.storageClass))
		{
			Decorations d{};
			ApplyDecorationsForId(&d, id);
			uint32_t index = 0;
			VisitMemoryObjectInner<F>(typeId, d, index, 0, f);
		}
		else
		{
			// Objects without explicit layout are tightly packed.
			for (auto i = 0u; i < getType(type.element).sizeInComponents; i++)
			{
				f(i, i * sizeof(float));
			}
		}
	}

	SIMD::Pointer SpirvShader::GetPointerToData(Object::ID id, int arrayIndex, SpirvRoutine *routine) const
	{
		auto &object = getObject(id);
		switch (object.kind)
		{
			case Object::Kind::Pointer:
			case Object::Kind::InterfaceVariable:
				return routine->getPointer(id);

			case Object::Kind::DescriptorSet:
			{
				const auto &d = descriptorDecorations.at(id);
				ASSERT(d.DescriptorSet >= 0 && d.DescriptorSet < vk::MAX_BOUND_DESCRIPTOR_SETS);
				ASSERT(d.Binding >= 0);

				auto set = routine->getPointer(id);

				auto setLayout = routine->pipelineLayout->getDescriptorSetLayout(d.DescriptorSet);
				ASSERT_MSG(setLayout->hasBinding(d.Binding), "Descriptor set %d does not contain binding %d", int(d.DescriptorSet), int(d.Binding));
				int bindingOffset = static_cast<int>(setLayout->getBindingOffset(d.Binding, arrayIndex));

				Pointer<Byte> descriptor = set.base + bindingOffset; // BufferDescriptor*
				Pointer<Byte> data = *Pointer<Pointer<Byte>>(descriptor + OFFSET(vk::BufferDescriptor, ptr)); // void*
				Int size = *Pointer<Int>(descriptor + OFFSET(vk::BufferDescriptor, sizeInBytes));
				if (setLayout->isBindingDynamic(d.Binding))
				{
					uint32_t dynamicBindingIndex =
						routine->pipelineLayout->getDynamicOffsetBase(d.DescriptorSet) +
						setLayout->getDynamicDescriptorOffset(d.Binding) +
						arrayIndex;
					Int offset = routine->descriptorDynamicOffsets[dynamicBindingIndex];
					Int robustnessSize = *Pointer<Int>(descriptor + OFFSET(vk::BufferDescriptor, robustnessSize));
					return SIMD::Pointer(data + offset, Min(size, robustnessSize - offset));
				}
				else
				{
					return SIMD::Pointer(data, size);
				}
			}

			default:
				UNREACHABLE("Invalid pointer kind %d", int(object.kind));
				return SIMD::Pointer(Pointer<Byte>(), 0);
		}
	}

	void SpirvShader::ApplyDecorationsForAccessChain(Decorations *d, DescriptorDecorations *dd, Object::ID baseId, uint32_t numIndexes, uint32_t const *indexIds) const
	{
		ApplyDecorationsForId(d, baseId);
		auto &baseObject = getObject(baseId);
		ApplyDecorationsForId(d, baseObject.type);
		auto typeId = getType(baseObject.type).element;

		for (auto i = 0u; i < numIndexes; i++)
		{
			ApplyDecorationsForId(d, typeId);
			auto & type = getType(typeId);
			switch (type.opcode())
			{
			case spv::OpTypeStruct:
			{
				int memberIndex = GetConstScalarInt(indexIds[i]);
				ApplyDecorationsForIdMember(d, typeId, memberIndex);
				typeId = type.definition.word(2u + memberIndex);
				break;
			}
			case spv::OpTypeArray:
			case spv::OpTypeRuntimeArray:
				if (dd->InputAttachmentIndex >= 0)
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
				UNREACHABLE("%s", OpcodeName(type.definition.opcode()).c_str());
			}
		}
	}

	SIMD::Pointer SpirvShader::WalkExplicitLayoutAccessChain(Object::ID baseId, uint32_t numIndexes, uint32_t const *indexIds, SpirvRoutine *routine) const
	{
		// Produce a offset into external memory in sizeof(float) units

		auto &baseObject = getObject(baseId);
		Type::ID typeId = getType(baseObject.type).element;
		Decorations d = {};
		ApplyDecorationsForId(&d, baseObject.type);

		uint32_t arrayIndex = 0;
		if (baseObject.kind == Object::Kind::DescriptorSet)
		{
			auto type = getType(typeId).definition.opcode();
			if (type == spv::OpTypeArray || type == spv::OpTypeRuntimeArray)
			{
				ASSERT(getObject(indexIds[0]).kind == Object::Kind::Constant);
				arrayIndex = GetConstScalarInt(indexIds[0]);

				numIndexes--;
				indexIds++;
				typeId = getType(typeId).element;
			}
		}

		auto ptr = GetPointerToData(baseId, arrayIndex, routine);

		int constantOffset = 0;

		for (auto i = 0u; i < numIndexes; i++)
		{
			auto & type = getType(typeId);
			ApplyDecorationsForId(&d, typeId);

			switch (type.definition.opcode())
			{
			case spv::OpTypeStruct:
			{
				int memberIndex = GetConstScalarInt(indexIds[i]);
				ApplyDecorationsForIdMember(&d, typeId, memberIndex);
				ASSERT(d.HasOffset);
				constantOffset += d.Offset;
				typeId = type.definition.word(2u + memberIndex);
				break;
			}
			case spv::OpTypeArray:
			case spv::OpTypeRuntimeArray:
			{
				// TODO: b/127950082: Check bounds.
				ASSERT(d.HasArrayStride);
				auto & obj = getObject(indexIds[i]);
				if (obj.kind == Object::Kind::Constant)
				{
					constantOffset += d.ArrayStride * GetConstScalarInt(indexIds[i]);
				}
				else
				{
					ptr += SIMD::Int(d.ArrayStride) * routine->getIntermediate(indexIds[i]).Int(0);
				}
				typeId = type.element;
				break;
			}
			case spv::OpTypeMatrix:
			{
				// TODO: b/127950082: Check bounds.
				ASSERT(d.HasMatrixStride);
				d.InsideMatrix = true;
				auto columnStride = (d.HasRowMajor && d.RowMajor) ? sizeof(float) : d.MatrixStride;
				auto & obj = getObject(indexIds[i]);
				if (obj.kind == Object::Kind::Constant)
				{
					constantOffset += columnStride * GetConstScalarInt(indexIds[i]);
				}
				else
				{
					ptr += SIMD::Int(columnStride) * routine->getIntermediate(indexIds[i]).Int(0);
				}
				typeId = type.element;
				break;
			}
			case spv::OpTypeVector:
			{
				auto elemStride = (d.InsideMatrix && d.HasRowMajor && d.RowMajor) ? d.MatrixStride : sizeof(float);
				auto & obj = getObject(indexIds[i]);
				if (obj.kind == Object::Kind::Constant)
				{
					constantOffset += elemStride * GetConstScalarInt(indexIds[i]);
				}
				else
				{
					ptr += SIMD::Int(elemStride) * routine->getIntermediate(indexIds[i]).Int(0);
				}
				typeId = type.element;
				break;
			}
			default:
				UNREACHABLE("%s", OpcodeName(type.definition.opcode()).c_str());
			}
		}

		ptr += constantOffset;
		return ptr;
	}

	SIMD::Pointer SpirvShader::WalkAccessChain(Object::ID baseId, uint32_t numIndexes, uint32_t const *indexIds, SpirvRoutine *routine) const
	{
		// TODO: avoid doing per-lane work in some cases if we can?

		auto &baseObject = getObject(baseId);
		Type::ID typeId = getType(baseObject.type).element;

		auto ptr = routine->getPointer(baseId);

		int constantOffset = 0;

		for (auto i = 0u; i < numIndexes; i++)
		{
			auto & type = getType(typeId);
			switch(type.opcode())
			{
			case spv::OpTypeStruct:
			{
				int memberIndex = GetConstScalarInt(indexIds[i]);
				int offsetIntoStruct = 0;
				for (auto j = 0; j < memberIndex; j++) {
					auto memberType = type.definition.word(2u + j);
					offsetIntoStruct += getType(memberType).sizeInComponents * sizeof(float);
				}
				constantOffset += offsetIntoStruct;
				typeId = type.definition.word(2u + memberIndex);
				break;
			}

			case spv::OpTypeVector:
			case spv::OpTypeMatrix:
			case spv::OpTypeArray:
			case spv::OpTypeRuntimeArray:
			{
				// TODO: b/127950082: Check bounds.
				if (getType(baseObject.type).storageClass == spv::StorageClassUniformConstant)
				{
					// indexing into an array of descriptors.
					auto &obj = getObject(indexIds[i]);
					if (obj.kind != Object::Kind::Constant)
					{
						UNSUPPORTED("SPIR-V SampledImageArrayDynamicIndexing Capability");
					}

					auto d = descriptorDecorations.at(baseId);
					ASSERT(d.DescriptorSet >= 0);
					ASSERT(d.Binding >= 0);
					auto setLayout = routine->pipelineLayout->getDescriptorSetLayout(d.DescriptorSet);
					auto stride = setLayout->getBindingStride(d.Binding);
					ptr.base += stride * GetConstScalarInt(indexIds[i]);
				}
				else
				{
					auto stride = getType(type.element).sizeInComponents * sizeof(float);
					auto & obj = getObject(indexIds[i]);
					if (obj.kind == Object::Kind::Constant)
					{
						ptr += stride * GetConstScalarInt(indexIds[i]);
					}
					else
					{
						ptr += SIMD::Int(stride) * routine->getIntermediate(indexIds[i]).Int(0);
					}
				}
				typeId = type.element;
				break;
			}

			default:
				UNREACHABLE("%s", OpcodeName(type.opcode()).c_str());
			}
		}

		if (constantOffset != 0)
		{
			ptr += constantOffset;
		}
		return ptr;
	}

	uint32_t SpirvShader::WalkLiteralAccessChain(Type::ID typeId, uint32_t numIndexes, uint32_t const *indexes) const
	{
		uint32_t componentOffset = 0;

		for (auto i = 0u; i < numIndexes; i++)
		{
			auto & type = getType(typeId);
			switch(type.opcode())
			{
			case spv::OpTypeStruct:
			{
				int memberIndex = indexes[i];
				int offsetIntoStruct = 0;
				for (auto j = 0; j < memberIndex; j++) {
					auto memberType = type.definition.word(2u + j);
					offsetIntoStruct += getType(memberType).sizeInComponents;
				}
				componentOffset += offsetIntoStruct;
				typeId = type.definition.word(2u + memberIndex);
				break;
			}

			case spv::OpTypeVector:
			case spv::OpTypeMatrix:
			case spv::OpTypeArray:
			{
				auto elementType = type.definition.word(2);
				auto stride = getType(elementType).sizeInComponents;
				componentOffset += stride * indexes[i];
				typeId = elementType;
				break;
			}

			default:
				UNREACHABLE("%s", OpcodeName(type.opcode()).c_str());
			}
		}

		return componentOffset;
	}

	void SpirvShader::Decorations::Apply(spv::Decoration decoration, uint32_t arg)
	{
		switch (decoration)
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
		if (src.HasBuiltIn)
		{
			HasBuiltIn = true;
			BuiltIn = src.BuiltIn;
		}

		if (src.HasLocation)
		{
			HasLocation = true;
			Location = src.Location;
		}

		if (src.HasComponent)
		{
			HasComponent = true;
			Component = src.Component;
		}

		if (src.HasOffset)
		{
			HasOffset = true;
			Offset = src.Offset;
		}

		if (src.HasArrayStride)
		{
			HasArrayStride = true;
			ArrayStride = src.ArrayStride;
		}

		if (src.HasMatrixStride)
		{
			HasMatrixStride = true;
			MatrixStride = src.MatrixStride;
		}

		if (src.HasRowMajor)
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

		if (src.InputAttachmentIndex >= 0)
		{
			InputAttachmentIndex = src.InputAttachmentIndex;
		}
	}

	void SpirvShader::ApplyDecorationsForId(Decorations *d, TypeOrObjectID id) const
	{
		auto it = decorations.find(id);
		if (it != decorations.end())
			d->Apply(it->second);
	}

	void SpirvShader::ApplyDecorationsForIdMember(Decorations *d, Type::ID id, uint32_t member) const
	{
		auto it = memberDecorations.find(id);
		if (it != memberDecorations.end() && member < it->second.size())
		{
			d->Apply(it->second[member]);
		}
	}

	void SpirvShader::DefineResult(const InsnIterator &insn)
	{
		Type::ID typeId = insn.word(1);
		Object::ID resultId = insn.word(2);
		auto &object = defs[resultId];
		object.type = typeId;

		switch (getType(typeId).opcode())
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
	}

	// emit-time

	void SpirvShader::emitProlog(SpirvRoutine *routine) const
	{
		for (auto insn : *this)
		{
			switch (insn.opcode())
			{
			case spv::OpVariable:
			{
				Type::ID resultPointerTypeId = insn.word(1);
				auto resultPointerType = getType(resultPointerTypeId);
				auto pointeeType = getType(resultPointerType.element);

				if(pointeeType.sizeInComponents > 0)  // TODO: what to do about zero-slot objects?
				{
					Object::ID resultId = insn.word(2);
					routine->createVariable(resultId, pointeeType.sizeInComponents);
				}
				break;
			}
			case spv::OpPhi:
			{
				auto type = getType(insn.word(1));
				Object::ID resultId = insn.word(2);
				routine->phis.emplace(resultId, SpirvRoutine::Variable(type.sizeInComponents));
				break;
			}
			default:
				// Nothing else produces interface variables, so can all be safely ignored.
				break;
			}
		}
	}

	void SpirvShader::emit(SpirvRoutine *routine, RValue<SIMD::Int> const &activeLaneMask, const vk::DescriptorSet::Bindings &descriptorSets) const
	{
		EmitState state(routine, activeLaneMask, descriptorSets);

		// Emit everything up to the first label
		// TODO: Separate out dispatch of block from non-block instructions?
		for (auto insn : *this)
		{
			if (insn.opcode() == spv::OpLabel)
			{
				break;
			}
			EmitInstruction(insn, &state);
		}

		// Emit all the blocks starting from entryPointBlockId.
		EmitBlocks(entryPointBlockId, &state);
	}

	void SpirvShader::EmitBlocks(Block::ID id, EmitState *state, Block::ID ignore /* = 0 */) const
	{
		auto oldPending = state->pending;

		std::queue<Block::ID> pending;
		state->pending = &pending;
		pending.push(id);
		while (pending.size() > 0)
		{
			auto id = pending.front();
			pending.pop();

			auto const &block = getBlock(id);
			if (id == ignore)
			{
				continue;
			}

			state->currentBlock = id;

			switch (block.kind)
			{
				case Block::Simple:
				case Block::StructuredBranchConditional:
				case Block::UnstructuredBranchConditional:
				case Block::StructuredSwitch:
				case Block::UnstructuredSwitch:
					EmitNonLoop(state);
					break;

				case Block::Loop:
					EmitLoop(state);
					break;

				default:
					UNREACHABLE("Unexpected Block Kind: %d", int(block.kind));
			}
		}

		state->pending = oldPending;
	}

	void SpirvShader::EmitInstructions(InsnIterator begin, InsnIterator end, EmitState *state) const
	{
		for (auto insn = begin; insn != end; insn++)
		{
			auto res = EmitInstruction(insn, state);
			switch (res)
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

	void SpirvShader::EmitNonLoop(EmitState *state) const
	{
		auto blockId = state->currentBlock;
		auto block = getBlock(blockId);

		// Ensure all incoming blocks have been generated.
		auto depsDone = true;
		for (auto in : block.ins)
		{
			if (state->visited.count(in) == 0)
			{
				state->pending->emplace(in);
				depsDone = false;
			}
		}

		if (!depsDone)
		{
			// come back to this once the dependencies have been generated
			state->pending->emplace(blockId);
			return;
		}

		if (!state->visited.emplace(blockId).second)
		{
			return; // Already generated this block.
		}

		if (blockId != entryPointBlockId)
		{
			// Set the activeLaneMask.
			SIMD::Int activeLaneMask(0);
			for (auto in : block.ins)
			{
				auto inMask = GetActiveLaneMaskEdge(state, in, blockId);
				activeLaneMask |= inMask;
			}
			state->setActiveLaneMask(activeLaneMask);
		}

		EmitInstructions(block.begin(), block.end(), state);

		for (auto out : block.outs)
		{
			state->pending->emplace(out);
		}
	}

	void SpirvShader::EmitLoop(EmitState *state) const
	{
		auto blockId = state->currentBlock;
		auto &block = getBlock(blockId);
		auto mergeBlockId = block.mergeBlock;
		auto &mergeBlock = getBlock(mergeBlockId);

		// Ensure all incoming non-back edge blocks have been generated.
		auto depsDone = true;
		for (auto in : block.ins)
		{
			if (state->visited.count(in) == 0)
			{
				if (!existsPath(blockId, in, mergeBlockId)) // if not a loop back edge
				{
					state->pending->emplace(in);
					depsDone = false;
				}
			}
		}

		if (!depsDone)
		{
			// come back to this once the dependencies have been generated
			state->pending->emplace(blockId);
			return;
		}

		if (!state->visited.emplace(blockId).second)
		{
			return; // Already emitted this loop.
		}

		std::unordered_set<Block::ID> incomingBlocks;
		std::unordered_set<Block::ID> loopBlocks;
		for (auto in : block.ins)
		{
			if (!existsPath(blockId, in, mergeBlockId)) // if not a loop back-edge
			{
				incomingBlocks.emplace(in);
			}
			else
			{
				loopBlocks.emplace(in);
			}
		}

		// Emit the loop phi instructions, and initialize them with a value from
		// the incoming blocks.
		for (auto insn = block.begin(); insn != block.mergeInstruction; insn++)
		{
			if (insn.opcode() == spv::OpPhi)
			{
				StorePhi(blockId, insn, state, incomingBlocks);
			}
		}

		// loopActiveLaneMask is the mask of lanes that are continuing to loop.
		// This is initialized with the incoming active lane masks.
		SIMD::Int loopActiveLaneMask = SIMD::Int(0);
		for (auto in : incomingBlocks)
		{
			loopActiveLaneMask |= GetActiveLaneMaskEdge(state, in, blockId);
		}

		// mergeActiveLaneMasks contains edge lane masks for the merge block.
		// This is the union of all edge masks across all iterations of the loop.
		std::unordered_map<Block::ID, SIMD::Int> mergeActiveLaneMasks;
		for (auto in : getBlock(mergeBlockId).ins)
		{
			mergeActiveLaneMasks.emplace(in, SIMD::Int(0));
		}

		// Create the loop basic blocks
		auto headerBasicBlock = Nucleus::createBasicBlock();
		auto mergeBasicBlock = Nucleus::createBasicBlock();

		// Start emitting code inside the loop.
		Nucleus::createBr(headerBasicBlock);
		Nucleus::setInsertBlock(headerBasicBlock);

		// Load the active lane mask.
		state->setActiveLaneMask(loopActiveLaneMask);

		// Emit the non-phi loop header block's instructions.
		for (auto insn = block.begin(); insn != block.end(); insn++)
		{
			if (insn.opcode() == spv::OpPhi)
			{
				LoadPhi(insn, state);
			}
			else
			{
				EmitInstruction(insn, state);
			}
		}

		// Emit all blocks between the loop header and the merge block, but
		// don't emit the merge block yet.
		for (auto out : block.outs)
		{
			EmitBlocks(out, state, mergeBlockId);
		}

		// Restore current block id after emitting loop blocks.
		state->currentBlock = blockId;

		// Rebuild the loopActiveLaneMask from the loop back edges.
		loopActiveLaneMask = SIMD::Int(0);
		for (auto in : block.ins)
		{
			if (existsPath(blockId, in, mergeBlockId))
			{
				loopActiveLaneMask |= GetActiveLaneMaskEdge(state, in, blockId);
			}
		}

		// Add active lanes to the merge lane mask.
		for (auto in : getBlock(mergeBlockId).ins)
		{
			auto edge = Block::Edge{in, mergeBlockId};
			auto it = state->edgeActiveLaneMasks.find(edge);
			if (it != state->edgeActiveLaneMasks.end())
			{
				mergeActiveLaneMasks[in] |= it->second;
			}
		}

		// Update loop phi values.
		for (auto insn = block.begin(); insn != block.mergeInstruction; insn++)
		{
			if (insn.opcode() == spv::OpPhi)
			{
				StorePhi(blockId, insn, state, loopBlocks);
			}
		}

		// Use the [loop -> merge] active lane masks to update the phi values in
		// the merge block. We need to do this to handle divergent control flow
		// in the loop.
		//
		// Consider the following:
		//
		//     int phi_source = 0;
		//     for (uint i = 0; i < 4; i++)
		//     {
		//         phi_source = 0;
		//         if (gl_GlobalInvocationID.x % 4 == i) // divergent control flow
		//         {
		//             phi_source = 42; // single lane assignment.
		//             break; // activeLaneMask for [loop->merge] is active for a single lane.
		//         }
		//         // -- we are here --
		//     }
		//     // merge block
		//     int phi = phi_source; // OpPhi
		//
		// In this example, with each iteration of the loop, phi_source will
		// only have a single lane assigned. However by 'phi' value in the merge
		// block needs to be assigned the union of all the per-lane assignments
		// of phi_source when that lane exited the loop.
		for (auto insn = mergeBlock.begin(); insn != mergeBlock.end(); insn++)
		{
			if (insn.opcode() == spv::OpPhi)
			{
				StorePhi(mergeBlockId, insn, state, mergeBlock.ins);
			}
		}

		// Loop body now done.
		// If any lanes are still active, jump back to the loop header,
		// otherwise jump to the merge block.
		Nucleus::createCondBr(AnyTrue(loopActiveLaneMask).value, headerBasicBlock, mergeBasicBlock);

		// Continue emitting from the merge block.
		Nucleus::setInsertBlock(mergeBasicBlock);
		state->pending->emplace(mergeBlockId);
		for (auto it : mergeActiveLaneMasks)
		{
			state->addActiveLaneMaskEdge(it.first, mergeBlockId, it.second);
		}
	}

	SpirvShader::EmitResult SpirvShader::EmitInstruction(InsnIterator insn, EmitState *state) const
	{
		auto opcode = insn.opcode();

		switch (opcode)
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
		case spv::OpName:
		case spv::OpMemberName:
		case spv::OpSource:
		case spv::OpSourceContinued:
		case spv::OpSourceExtension:
		case spv::OpLine:
		case spv::OpNoLine:
		case spv::OpModuleProcessed:
		case spv::OpString:
			// Nothing to do at emit time. These are either fully handled at analysis time,
			// or don't require any work at all.
			return EmitResult::Continue;

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

		case spv::OpKill:
			return EmitKill(insn, state);

		case spv::OpImageSampleImplicitLod:
			return EmitImageSampleImplicitLod(None, insn, state);

		case spv::OpImageSampleExplicitLod:
			return EmitImageSampleExplicitLod(None, insn, state);

		case spv::OpImageSampleDrefImplicitLod:
			return EmitImageSampleImplicitLod(Dref, insn, state);

		case spv::OpImageSampleDrefExplicitLod:
			return EmitImageSampleExplicitLod(Dref, insn, state);

		case spv::OpImageSampleProjImplicitLod:
			return EmitImageSampleImplicitLod(Proj, insn, state);

		case spv::OpImageSampleProjExplicitLod:
			return EmitImageSampleExplicitLod(Proj, insn, state);

		case spv::OpImageSampleProjDrefImplicitLod:
			return EmitImageSampleImplicitLod(ProjDref, insn, state);

		case spv::OpImageSampleProjDrefExplicitLod:
			return EmitImageSampleExplicitLod(ProjDref, insn, state);

		case spv::OpImageGather:
			return EmitImageGather(None, insn, state);

		case spv::OpImageDrefGather:
			return EmitImageGather(Dref, insn, state);

		case spv::OpImageFetch:
			return EmitImageFetch(insn, state);

		case spv::OpImageQuerySizeLod:
			return EmitImageQuerySizeLod(insn, state);

		case spv::OpImageQuerySize:
			return EmitImageQuerySize(insn, state);

		case spv::OpImageQueryLod:
			return EmitImageQueryLod(insn, state);

		case spv::OpImageQueryLevels:
			return EmitImageQueryLevels(insn, state);

		case spv::OpImageQuerySamples:
			return EmitImageQuerySamples(insn, state);

		case spv::OpImageRead:
			return EmitImageRead(insn, state);

		case spv::OpImageWrite:
			return EmitImageWrite(insn, state);

		case spv::OpImageTexelPointer:
			return EmitImageTexelPointer(insn, state);

		case spv::OpSampledImage:
		case spv::OpImage:
			return EmitSampledImageCombineOrSplit(insn, state);

		case spv::OpCopyObject:
			return EmitCopyObject(insn, state);

		case spv::OpCopyMemory:
			return EmitCopyMemory(insn, state);

		case spv::OpControlBarrier:
			return EmitControlBarrier(insn, state);

		case spv::OpMemoryBarrier:
			return EmitMemoryBarrier(insn, state);

		case spv::OpGroupNonUniformElect:
			return EmitGroupNonUniform(insn, state);

		case spv::OpArrayLength:
			return EmitArrayLength(insn, state);

		default:
			UNREACHABLE("%s", OpcodeName(opcode).c_str());
			break;
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitVariable(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		Object::ID resultId = insn.word(2);
		auto &object = getObject(resultId);
		auto &objectTy = getType(object.type);

		switch (objectTy.storageClass)
		{
		case spv::StorageClassOutput:
		case spv::StorageClassPrivate:
		case spv::StorageClassFunction:
		{
			ASSERT(objectTy.opcode() == spv::OpTypePointer);
			auto base = &routine->getVariable(resultId)[0];
			auto elementTy = getType(objectTy.element);
			auto size = elementTy.sizeInComponents * sizeof(float) * SIMD::Width;
			routine->createPointer(resultId, SIMD::Pointer(base, size));
			break;
		}
		case spv::StorageClassWorkgroup:
		{
			ASSERT(objectTy.opcode() == spv::OpTypePointer);
			auto base = &routine->workgroupMemory[0];
			auto size = workgroupMemory.size();
			routine->createPointer(resultId, SIMD::Pointer(base, size, workgroupMemory.offsetOf(resultId)));
			break;
		}
		case spv::StorageClassInput:
		{
			if (object.kind == Object::Kind::InterfaceVariable)
			{
				auto &dst = routine->getVariable(resultId);
				int offset = 0;
				VisitInterface(resultId,
								[&](Decorations const &d, AttribType type) {
									auto scalarSlot = d.Location << 2 | d.Component;
									dst[offset++] = routine->inputs[scalarSlot];
								});
			}
			ASSERT(objectTy.opcode() == spv::OpTypePointer);
			auto base = &routine->getVariable(resultId)[0];
			auto elementTy = getType(objectTy.element);
			auto size = elementTy.sizeInComponents * sizeof(float) * SIMD::Width;
			routine->createPointer(resultId, SIMD::Pointer(base, size));
			break;
		}
		case spv::StorageClassUniformConstant:
		{
			const auto &d = descriptorDecorations.at(resultId);
			ASSERT(d.DescriptorSet >= 0);
			ASSERT(d.Binding >= 0);

			uint32_t arrayIndex = 0;  // TODO(b/129523279)
			auto setLayout = routine->pipelineLayout->getDescriptorSetLayout(d.DescriptorSet);
			if (setLayout->hasBinding(d.Binding))
			{
				size_t bindingOffset = setLayout->getBindingOffset(d.Binding, arrayIndex);
				Pointer<Byte> set = routine->descriptorSets[d.DescriptorSet];  // DescriptorSet*
				Pointer<Byte> binding = Pointer<Byte>(set + bindingOffset);    // vk::SampledImageDescriptor*
				auto size = 0; // Not required as this pointer is not directly used by SIMD::Read or SIMD::Write.
				routine->createPointer(resultId, SIMD::Pointer(binding, size));
			}
			else
			{
				// TODO: Error if the variable with the non-existant binding is
				// used? Or perhaps strip these unused variable declarations as
				// a preprocess on the SPIR-V?
			}
			break;
		}
		case spv::StorageClassUniform:
		case spv::StorageClassStorageBuffer:
		{
			const auto &d = descriptorDecorations.at(resultId);
			ASSERT(d.DescriptorSet >= 0 && d.DescriptorSet < vk::MAX_BOUND_DESCRIPTOR_SETS);
			auto size = 0; // Not required as this pointer is not directly used by SIMD::Read or SIMD::Write.
			routine->createPointer(resultId, SIMD::Pointer(routine->descriptorSets[d.DescriptorSet], size));
			break;
		}
		case spv::StorageClassPushConstant:
		{
			routine->createPointer(resultId, SIMD::Pointer(routine->pushConstants, vk::MAX_PUSH_CONSTANT_SIZE));
			break;
		}
		default:
			UNREACHABLE("Storage class %d", objectTy.storageClass);
			break;
		}

		if (insn.wordCount() > 4)
		{
			Object::ID initializerId = insn.word(4);
			if (getObject(initializerId).kind != Object::Kind::Constant)
			{
				UNIMPLEMENTED("Non-constant initializers not yet implemented");
			}
			switch (objectTy.storageClass)
			{
			case spv::StorageClassOutput:
			case spv::StorageClassPrivate:
			case spv::StorageClassFunction:
			{
				bool interleavedByLane = IsStorageInterleavedByLane(objectTy.storageClass);
				auto ptr = GetPointerToData(resultId, 0, routine);
				GenericValue initialValue(this, routine, initializerId);
				VisitMemoryObject(resultId, [&](uint32_t i, uint32_t offset)
				{
					auto p = ptr + offset;
					if (interleavedByLane) { p = interleaveByLane(p); }
					SIMD::Store(p, initialValue.Float(i), state->activeLaneMask());
				});
				break;
			}
			default:
				ASSERT_MSG(initializerId == 0, "Vulkan does not permit variables of storage class %d to have initializers", int(objectTy.storageClass));
			}
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitLoad(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		bool atomic = (insn.opcode() == spv::OpAtomicLoad);
		Object::ID resultId = insn.word(2);
		Object::ID pointerId = insn.word(3);
		auto &result = getObject(resultId);
		auto &resultTy = getType(result.type);
		auto &pointer = getObject(pointerId);
		auto &pointerTy = getType(pointer.type);
		std::memory_order memoryOrder = std::memory_order_relaxed;

		ASSERT(getType(pointer.type).element == result.type);
		ASSERT(Type::ID(insn.word(1)) == result.type);
		ASSERT(!atomic || getType(getType(pointer.type).element).opcode() == spv::OpTypeInt);  // Vulkan 1.1: "Atomic instructions must declare a scalar 32-bit integer type, for the value pointed to by Pointer."

		if(pointerTy.storageClass == spv::StorageClassUniformConstant)
		{
			// Just propagate the pointer.
			auto &ptr = routine->getPointer(pointerId);
			routine->createPointer(resultId, ptr);
			return EmitResult::Continue;
		}

		if(atomic)
		{
			Object::ID semanticsId = insn.word(5);
			auto memorySemantics = static_cast<spv::MemorySemanticsMask>(getObject(semanticsId).constantValue[0]);
			memoryOrder = MemoryOrder(memorySemantics);
		}

		auto ptr = GetPointerToData(pointerId, 0, routine);

		bool interleavedByLane = IsStorageInterleavedByLane(pointerTy.storageClass);

		auto &dst = routine->createIntermediate(resultId, resultTy.sizeInComponents);

		VisitMemoryObject(pointerId, [&](uint32_t i, uint32_t offset)
		{
			auto p = ptr + offset;
			if (interleavedByLane) { p = interleaveByLane(p); }
			dst.move(i, SIMD::Load<SIMD::Float>(p, state->activeLaneMask(), atomic, memoryOrder));
		});

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitStore(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		bool atomic = (insn.opcode() == spv::OpAtomicStore);
		Object::ID pointerId = insn.word(1);
		Object::ID objectId = insn.word(atomic ? 4 : 2);
		auto &object = getObject(objectId);
		auto &pointer = getObject(pointerId);
		auto &pointerTy = getType(pointer.type);
		auto &elementTy = getType(pointerTy.element);
		std::memory_order memoryOrder = std::memory_order_relaxed;

		if(atomic)
		{
			Object::ID semanticsId = insn.word(3);
			auto memorySemantics = static_cast<spv::MemorySemanticsMask>(getObject(semanticsId).constantValue[0]);
			memoryOrder = MemoryOrder(memorySemantics);
		}

		ASSERT(!atomic || elementTy.opcode() == spv::OpTypeInt);  // Vulkan 1.1: "Atomic instructions must declare a scalar 32-bit integer type, for the value pointed to by Pointer."

		auto ptr = GetPointerToData(pointerId, 0, routine);
		bool interleavedByLane = IsStorageInterleavedByLane(pointerTy.storageClass);

		if (object.kind == Object::Kind::Constant)
		{
			// Constant source data.
			auto src = reinterpret_cast<float *>(object.constantValue.get());
			VisitMemoryObject(pointerId, [&](uint32_t i, uint32_t offset)
			{
				auto p = ptr + offset;
				if (interleavedByLane) { p = interleaveByLane(p); }
				SIMD::Store(p, SIMD::Float(src[i]), state->activeLaneMask(), atomic, memoryOrder);
			});
		}
		else
		{
			// Intermediate source data.
			auto &src = routine->getIntermediate(objectId);
			VisitMemoryObject(pointerId, [&](uint32_t i, uint32_t offset)
			{
				auto p = ptr + offset;
				if (interleavedByLane) { p = interleaveByLane(p); }
				SIMD::Store(p, src.Float(i), state->activeLaneMask(), atomic, memoryOrder);
			});
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitAccessChain(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		Type::ID typeId = insn.word(1);
		Object::ID resultId = insn.word(2);
		Object::ID baseId = insn.word(3);
		uint32_t numIndexes = insn.wordCount() - 4;
		const uint32_t *indexes = insn.wordPointer(4);
		auto &type = getType(typeId);
		ASSERT(type.sizeInComponents == 1);
		ASSERT(getObject(resultId).kind == Object::Kind::Pointer);

		if(type.storageClass == spv::StorageClassPushConstant ||
		   type.storageClass == spv::StorageClassUniform ||
		   type.storageClass == spv::StorageClassStorageBuffer)
		{
			auto ptr = WalkExplicitLayoutAccessChain(baseId, numIndexes, indexes, routine);
			routine->createPointer(resultId, ptr);
		}
		else
		{
			auto ptr = WalkAccessChain(baseId, numIndexes, indexes, routine);
			routine->createPointer(resultId, ptr);
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitCompositeConstruct(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto offset = 0u;

		for (auto i = 0u; i < insn.wordCount() - 3; i++)
		{
			Object::ID srcObjectId = insn.word(3u + i);
			auto & srcObject = getObject(srcObjectId);
			auto & srcObjectTy = getType(srcObject.type);
			GenericValue srcObjectAccess(this, routine, srcObjectId);

			for (auto j = 0u; j < srcObjectTy.sizeInComponents; j++)
			{
				dst.move(offset++, srcObjectAccess.Float(j));
			}
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitCompositeInsert(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		Type::ID resultTypeId = insn.word(1);
		auto &type = getType(resultTypeId);
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto &newPartObject = getObject(insn.word(3));
		auto &newPartObjectTy = getType(newPartObject.type);
		auto firstNewComponent = WalkLiteralAccessChain(resultTypeId, insn.wordCount() - 5, insn.wordPointer(5));

		GenericValue srcObjectAccess(this, routine, insn.word(4));
		GenericValue newPartObjectAccess(this, routine, insn.word(3));

		// old components before
		for (auto i = 0u; i < firstNewComponent; i++)
		{
			dst.move(i, srcObjectAccess.Float(i));
		}
		// new part
		for (auto i = 0u; i < newPartObjectTy.sizeInComponents; i++)
		{
			dst.move(firstNewComponent + i, newPartObjectAccess.Float(i));
		}
		// old components after
		for (auto i = firstNewComponent + newPartObjectTy.sizeInComponents; i < type.sizeInComponents; i++)
		{
			dst.move(i, srcObjectAccess.Float(i));
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitCompositeExtract(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto &compositeObject = getObject(insn.word(3));
		Type::ID compositeTypeId = compositeObject.definition.word(1);
		auto firstComponent = WalkLiteralAccessChain(compositeTypeId, insn.wordCount() - 4, insn.wordPointer(4));

		GenericValue compositeObjectAccess(this, routine, insn.word(3));
		for (auto i = 0u; i < type.sizeInComponents; i++)
		{
			dst.move(i, compositeObjectAccess.Float(firstComponent + i));
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitVectorShuffle(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);

		// Note: number of components in result type, first half type, and second
		// half type are all independent.
		auto &firstHalfType = getType(getObject(insn.word(3)).type);

		GenericValue firstHalfAccess(this, routine, insn.word(3));
		GenericValue secondHalfAccess(this, routine, insn.word(4));

		for (auto i = 0u; i < type.sizeInComponents; i++)
		{
			auto selector = insn.word(5 + i);
			if (selector == static_cast<uint32_t>(-1))
			{
				// Undefined value. Until we decide to do real undef values, zero is as good
				// a value as any
				dst.move(i, RValue<SIMD::Float>(0.0f));
			}
			else if (selector < firstHalfType.sizeInComponents)
			{
				dst.move(i, firstHalfAccess.Float(selector));
			}
			else
			{
				dst.move(i, secondHalfAccess.Float(selector - firstHalfType.sizeInComponents));
			}
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitVectorExtractDynamic(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto &srcType = getType(getObject(insn.word(3)).type);

		GenericValue src(this, routine, insn.word(3));
		GenericValue index(this, routine, insn.word(4));

		SIMD::UInt v = SIMD::UInt(0);

		for (auto i = 0u; i < srcType.sizeInComponents; i++)
		{
			v |= CmpEQ(index.UInt(0), SIMD::UInt(i)) & src.UInt(i);
		}

		dst.move(0, v);
		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitVectorInsertDynamic(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);

		GenericValue src(this, routine, insn.word(3));
		GenericValue component(this, routine, insn.word(4));
		GenericValue index(this, routine, insn.word(5));

		for (auto i = 0u; i < type.sizeInComponents; i++)
		{
			SIMD::UInt mask = CmpEQ(SIMD::UInt(i), index.UInt(0));
			dst.move(i, (src.UInt(i) & ~mask) | (component.UInt(0) & mask));
		}
		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitVectorTimesScalar(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto lhs = GenericValue(this, routine, insn.word(3));
		auto rhs = GenericValue(this, routine, insn.word(4));

		for (auto i = 0u; i < type.sizeInComponents; i++)
		{
			dst.move(i, lhs.Float(i) * rhs.Float(0));
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitMatrixTimesVector(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto lhs = GenericValue(this, routine, insn.word(3));
		auto rhs = GenericValue(this, routine, insn.word(4));
		auto rhsType = getType(rhs.type);

		for (auto i = 0u; i < type.sizeInComponents; i++)
		{
			SIMD::Float v = lhs.Float(i) * rhs.Float(0);
			for (auto j = 1u; j < rhsType.sizeInComponents; j++)
			{
				v += lhs.Float(i + type.sizeInComponents * j) * rhs.Float(j);
			}
			dst.move(i, v);
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitVectorTimesMatrix(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto lhs = GenericValue(this, routine, insn.word(3));
		auto rhs = GenericValue(this, routine, insn.word(4));
		auto lhsType = getType(lhs.type);

		for (auto i = 0u; i < type.sizeInComponents; i++)
		{
			SIMD::Float v = lhs.Float(0) * rhs.Float(i * lhsType.sizeInComponents);
			for (auto j = 1u; j < lhsType.sizeInComponents; j++)
			{
				v += lhs.Float(j) * rhs.Float(i * lhsType.sizeInComponents + j);
			}
			dst.move(i, v);
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitMatrixTimesMatrix(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto lhs = GenericValue(this, routine, insn.word(3));
		auto rhs = GenericValue(this, routine, insn.word(4));

		auto numColumns = type.definition.word(3);
		auto numRows = getType(type.definition.word(2)).definition.word(3);
		auto numAdds = getType(getObject(insn.word(3)).type).definition.word(3);

		for (auto row = 0u; row < numRows; row++)
		{
			for (auto col = 0u; col < numColumns; col++)
			{
				SIMD::Float v = SIMD::Float(0);
				for (auto i = 0u; i < numAdds; i++)
				{
					v += lhs.Float(i * numRows + row) * rhs.Float(col * numAdds + i);
				}
				dst.move(numRows * col + row, v);
			}
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitOuterProduct(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto lhs = GenericValue(this, routine, insn.word(3));
		auto rhs = GenericValue(this, routine, insn.word(4));
		auto &lhsType = getType(lhs.type);
		auto &rhsType = getType(rhs.type);

		ASSERT(type.definition.opcode() == spv::OpTypeMatrix);
		ASSERT(lhsType.definition.opcode() == spv::OpTypeVector);
		ASSERT(rhsType.definition.opcode() == spv::OpTypeVector);
		ASSERT(getType(lhsType.element).opcode() == spv::OpTypeFloat);
		ASSERT(getType(rhsType.element).opcode() == spv::OpTypeFloat);

		auto numRows = lhsType.definition.word(3);
		auto numCols = rhsType.definition.word(3);

		for (auto col = 0u; col < numCols; col++)
		{
			for (auto row = 0u; row < numRows; row++)
			{
				dst.move(col * numRows + row, lhs.Float(row) * rhs.Float(col));
			}
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitTranspose(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto mat = GenericValue(this, routine, insn.word(3));

		auto numCols = type.definition.word(3);
		auto numRows = getType(type.definition.word(2)).sizeInComponents;

		for (auto col = 0u; col < numCols; col++)
		{
			for (auto row = 0u; row < numRows; row++)
			{
				dst.move(col * numRows + row, mat.Float(row * numCols + col));
			}
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitUnaryOp(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto src = GenericValue(this, routine, insn.word(3));

		for (auto i = 0u; i < type.sizeInComponents; i++)
		{
			switch (insn.opcode())
			{
			case spv::OpNot:
			case spv::OpLogicalNot:		// logical not == bitwise not due to all-bits boolean representation
				dst.move(i, ~src.UInt(i));
				break;
			case spv::OpBitFieldInsert:
			{
				auto insert = GenericValue(this, routine, insn.word(4)).UInt(i);
				auto offset = GenericValue(this, routine, insn.word(5)).UInt(0);
				auto count = GenericValue(this, routine, insn.word(6)).UInt(0);
				auto one = SIMD::UInt(1);
				auto v = src.UInt(i);
				auto mask = Bitmask32(offset + count) ^ Bitmask32(offset);
				dst.move(i, (v & ~mask) | ((insert << offset) & mask));
				break;
			}
			case spv::OpBitFieldSExtract:
			case spv::OpBitFieldUExtract:
			{
				auto offset = GenericValue(this, routine, insn.word(4)).UInt(0);
				auto count = GenericValue(this, routine, insn.word(5)).UInt(0);
				auto one = SIMD::UInt(1);
				auto v = src.UInt(i);
				SIMD::UInt out = (v >> offset) & Bitmask32(count);
				if (insn.opcode() == spv::OpBitFieldSExtract)
				{
					auto sign = out & NthBit32(count - one);
					auto sext = ~(sign - one);
					out |= sext;
				}
				dst.move(i, out);
				break;
			}
			case spv::OpBitReverse:
			{
				// TODO: Add an intrinsic to reactor. Even if there isn't a
				// single vector instruction, there may be target-dependent
				// ways to make this faster.
				// https://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel
				SIMD::UInt v = src.UInt(i);
				v = ((v >> 1) & SIMD::UInt(0x55555555)) | ((v & SIMD::UInt(0x55555555)) << 1);
				v = ((v >> 2) & SIMD::UInt(0x33333333)) | ((v & SIMD::UInt(0x33333333)) << 2);
				v = ((v >> 4) & SIMD::UInt(0x0F0F0F0F)) | ((v & SIMD::UInt(0x0F0F0F0F)) << 4);
				v = ((v >> 8) & SIMD::UInt(0x00FF00FF)) | ((v & SIMD::UInt(0x00FF00FF)) << 8);
				v = (v >> 16) | (v << 16);
				dst.move(i, v);
				break;
			}
			case spv::OpBitCount:
			{
				// TODO: Add an intrinsic to reactor. Even if there isn't a
				// single vector instruction, there may be target-dependent
				// ways to make this faster.
				// https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
				auto v = src.UInt(i);
				SIMD::UInt c = v - ((v >> 1) & SIMD::UInt(0x55555555));
				c = ((c >> 2) & SIMD::UInt(0x33333333)) + (c & SIMD::UInt(0x33333333));
				c = ((c >> 4) + c) & SIMD::UInt(0x0F0F0F0F);
				c = ((c >> 8) + c) & SIMD::UInt(0x00FF00FF);
				c = ((c >> 16) + c) & SIMD::UInt(0x0000FFFF);
				dst.move(i, c);
				break;
			}
			case spv::OpSNegate:
				dst.move(i, -src.Int(i));
				break;
			case spv::OpFNegate:
				dst.move(i, -src.Float(i));
				break;
			case spv::OpConvertFToU:
				dst.move(i, SIMD::UInt(src.Float(i)));
				break;
			case spv::OpConvertFToS:
				dst.move(i, SIMD::Int(src.Float(i)));
				break;
			case spv::OpConvertSToF:
				dst.move(i, SIMD::Float(src.Int(i)));
				break;
			case spv::OpConvertUToF:
				dst.move(i, SIMD::Float(src.UInt(i)));
				break;
			case spv::OpBitcast:
				dst.move(i, src.Float(i));
				break;
			case spv::OpIsInf:
				dst.move(i, IsInf(src.Float(i)));
				break;
			case spv::OpIsNan:
				dst.move(i, IsNan(src.Float(i)));
				break;
			case spv::OpDPdx:
			case spv::OpDPdxCoarse:
				// Derivative instructions: FS invocations are laid out like so:
				//    0 1
				//    2 3
				static_assert(SIMD::Width == 4, "All cross-lane instructions will need care when using a different width");
				dst.move(i, SIMD::Float(Extract(src.Float(i), 1) - Extract(src.Float(i), 0)));
				break;
			case spv::OpDPdy:
			case spv::OpDPdyCoarse:
				dst.move(i, SIMD::Float(Extract(src.Float(i), 2) - Extract(src.Float(i), 0)));
				break;
			case spv::OpFwidth:
			case spv::OpFwidthCoarse:
				dst.move(i, SIMD::Float(Abs(Extract(src.Float(i), 1) - Extract(src.Float(i), 0))
							+ Abs(Extract(src.Float(i), 2) - Extract(src.Float(i), 0))));
				break;
			case spv::OpDPdxFine:
			{
				auto firstRow = Extract(src.Float(i), 1) - Extract(src.Float(i), 0);
				auto secondRow = Extract(src.Float(i), 3) - Extract(src.Float(i), 2);
				SIMD::Float v = SIMD::Float(firstRow);
				v = Insert(v, secondRow, 2);
				v = Insert(v, secondRow, 3);
				dst.move(i, v);
				break;
			}
			case spv::OpDPdyFine:
			{
				auto firstColumn = Extract(src.Float(i), 2) - Extract(src.Float(i), 0);
				auto secondColumn = Extract(src.Float(i), 3) - Extract(src.Float(i), 1);
				SIMD::Float v = SIMD::Float(firstColumn);
				v = Insert(v, secondColumn, 1);
				v = Insert(v, secondColumn, 3);
				dst.move(i, v);
				break;
			}
			case spv::OpFwidthFine:
			{
				auto firstRow = Extract(src.Float(i), 1) - Extract(src.Float(i), 0);
				auto secondRow = Extract(src.Float(i), 3) - Extract(src.Float(i), 2);
				SIMD::Float dpdx = SIMD::Float(firstRow);
				dpdx = Insert(dpdx, secondRow, 2);
				dpdx = Insert(dpdx, secondRow, 3);
				auto firstColumn = Extract(src.Float(i), 2) - Extract(src.Float(i), 0);
				auto secondColumn = Extract(src.Float(i), 3) - Extract(src.Float(i), 1);
				SIMD::Float dpdy = SIMD::Float(firstColumn);
				dpdy = Insert(dpdy, secondColumn, 1);
				dpdy = Insert(dpdy, secondColumn, 3);
				dst.move(i, Abs(dpdx) + Abs(dpdy));
				break;
			}
			case spv::OpQuantizeToF16:
			{
				// Note: keep in sync with the specialization constant version in EvalSpecConstantUnaryOp
				auto abs = Abs(src.Float(i));
				auto sign = src.Int(i) & SIMD::Int(0x80000000);
				auto isZero = CmpLT(abs, SIMD::Float(0.000061035f));
				auto isInf  = CmpGT(abs, SIMD::Float(65504.0f));
				auto isNaN  = IsNan(abs);
				auto isInfOrNan = isInf | isNaN;
				SIMD::Int v = src.Int(i) & SIMD::Int(0xFFFFE000);
				v &= ~isZero | SIMD::Int(0x80000000);
				v = sign | (isInfOrNan & SIMD::Int(0x7F800000)) | (~isInfOrNan & v);
				v |= isNaN & SIMD::Int(0x400000);
				dst.move(i, v);
				break;
			}
			default:
				UNREACHABLE("%s", OpcodeName(insn.opcode()).c_str());
			}
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitBinaryOp(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto &lhsType = getType(getObject(insn.word(3)).type);
		auto lhs = GenericValue(this, routine, insn.word(3));
		auto rhs = GenericValue(this, routine, insn.word(4));

		for (auto i = 0u; i < lhsType.sizeInComponents; i++)
		{
			switch (insn.opcode())
			{
			case spv::OpIAdd:
				dst.move(i, lhs.Int(i) + rhs.Int(i));
				break;
			case spv::OpISub:
				dst.move(i, lhs.Int(i) - rhs.Int(i));
				break;
			case spv::OpIMul:
				dst.move(i, lhs.Int(i) * rhs.Int(i));
				break;
			case spv::OpSDiv:
			{
				SIMD::Int a = lhs.Int(i);
				SIMD::Int b = rhs.Int(i);
				b = b | CmpEQ(b, SIMD::Int(0)); // prevent divide-by-zero
				a = a | (CmpEQ(a, SIMD::Int(0x80000000)) & CmpEQ(b, SIMD::Int(-1))); // prevent integer overflow
				dst.move(i, a / b);
				break;
			}
			case spv::OpUDiv:
			{
				auto zeroMask = As<SIMD::UInt>(CmpEQ(rhs.Int(i), SIMD::Int(0)));
				dst.move(i, lhs.UInt(i) / (rhs.UInt(i) | zeroMask));
				break;
			}
			case spv::OpSRem:
			{
				SIMD::Int a = lhs.Int(i);
				SIMD::Int b = rhs.Int(i);
				b = b | CmpEQ(b, SIMD::Int(0)); // prevent divide-by-zero
				a = a | (CmpEQ(a, SIMD::Int(0x80000000)) & CmpEQ(b, SIMD::Int(-1))); // prevent integer overflow
				dst.move(i, a % b);
				break;
			}
			case spv::OpSMod:
			{
				SIMD::Int a = lhs.Int(i);
				SIMD::Int b = rhs.Int(i);
				b = b | CmpEQ(b, SIMD::Int(0)); // prevent divide-by-zero
				a = a | (CmpEQ(a, SIMD::Int(0x80000000)) & CmpEQ(b, SIMD::Int(-1))); // prevent integer overflow
				auto mod = a % b;
				// If a and b have opposite signs, the remainder operation takes
				// the sign from a but OpSMod is supposed to take the sign of b.
				// Adding b will ensure that the result has the correct sign and
				// that it is still congruent to a modulo b.
				//
				// See also http://mathforum.org/library/drmath/view/52343.html
				auto signDiff = CmpNEQ(CmpGE(a, SIMD::Int(0)), CmpGE(b, SIMD::Int(0)));
				auto fixedMod = mod + (b & CmpNEQ(mod, SIMD::Int(0)) & signDiff);
				dst.move(i, As<SIMD::Float>(fixedMod));
				break;
			}
			case spv::OpUMod:
			{
				auto zeroMask = As<SIMD::UInt>(CmpEQ(rhs.Int(i), SIMD::Int(0)));
				dst.move(i, lhs.UInt(i) % (rhs.UInt(i) | zeroMask));
				break;
			}
			case spv::OpIEqual:
			case spv::OpLogicalEqual:
				dst.move(i, CmpEQ(lhs.Int(i), rhs.Int(i)));
				break;
			case spv::OpINotEqual:
			case spv::OpLogicalNotEqual:
				dst.move(i, CmpNEQ(lhs.Int(i), rhs.Int(i)));
				break;
			case spv::OpUGreaterThan:
				dst.move(i, CmpGT(lhs.UInt(i), rhs.UInt(i)));
				break;
			case spv::OpSGreaterThan:
				dst.move(i, CmpGT(lhs.Int(i), rhs.Int(i)));
				break;
			case spv::OpUGreaterThanEqual:
				dst.move(i, CmpGE(lhs.UInt(i), rhs.UInt(i)));
				break;
			case spv::OpSGreaterThanEqual:
				dst.move(i, CmpGE(lhs.Int(i), rhs.Int(i)));
				break;
			case spv::OpULessThan:
				dst.move(i, CmpLT(lhs.UInt(i), rhs.UInt(i)));
				break;
			case spv::OpSLessThan:
				dst.move(i, CmpLT(lhs.Int(i), rhs.Int(i)));
				break;
			case spv::OpULessThanEqual:
				dst.move(i, CmpLE(lhs.UInt(i), rhs.UInt(i)));
				break;
			case spv::OpSLessThanEqual:
				dst.move(i, CmpLE(lhs.Int(i), rhs.Int(i)));
				break;
			case spv::OpFAdd:
				dst.move(i, lhs.Float(i) + rhs.Float(i));
				break;
			case spv::OpFSub:
				dst.move(i, lhs.Float(i) - rhs.Float(i));
				break;
			case spv::OpFMul:
				dst.move(i, lhs.Float(i) * rhs.Float(i));
				break;
			case spv::OpFDiv:
				dst.move(i, lhs.Float(i) / rhs.Float(i));
				break;
			case spv::OpFMod:
				// TODO(b/126873455): inaccurate for values greater than 2^24
				dst.move(i, lhs.Float(i) - rhs.Float(i) * Floor(lhs.Float(i) / rhs.Float(i)));
				break;
			case spv::OpFRem:
				dst.move(i, lhs.Float(i) % rhs.Float(i));
				break;
			case spv::OpFOrdEqual:
				dst.move(i, CmpEQ(lhs.Float(i), rhs.Float(i)));
				break;
			case spv::OpFUnordEqual:
				dst.move(i, CmpUEQ(lhs.Float(i), rhs.Float(i)));
				break;
			case spv::OpFOrdNotEqual:
				dst.move(i, CmpNEQ(lhs.Float(i), rhs.Float(i)));
				break;
			case spv::OpFUnordNotEqual:
				dst.move(i, CmpUNEQ(lhs.Float(i), rhs.Float(i)));
				break;
			case spv::OpFOrdLessThan:
				dst.move(i, CmpLT(lhs.Float(i), rhs.Float(i)));
				break;
			case spv::OpFUnordLessThan:
				dst.move(i, CmpULT(lhs.Float(i), rhs.Float(i)));
				break;
			case spv::OpFOrdGreaterThan:
				dst.move(i, CmpGT(lhs.Float(i), rhs.Float(i)));
				break;
			case spv::OpFUnordGreaterThan:
				dst.move(i, CmpUGT(lhs.Float(i), rhs.Float(i)));
				break;
			case spv::OpFOrdLessThanEqual:
				dst.move(i, CmpLE(lhs.Float(i), rhs.Float(i)));
				break;
			case spv::OpFUnordLessThanEqual:
				dst.move(i, CmpULE(lhs.Float(i), rhs.Float(i)));
				break;
			case spv::OpFOrdGreaterThanEqual:
				dst.move(i, CmpGE(lhs.Float(i), rhs.Float(i)));
				break;
			case spv::OpFUnordGreaterThanEqual:
				dst.move(i, CmpUGE(lhs.Float(i), rhs.Float(i)));
				break;
			case spv::OpShiftRightLogical:
				dst.move(i, lhs.UInt(i) >> rhs.UInt(i));
				break;
			case spv::OpShiftRightArithmetic:
				dst.move(i, lhs.Int(i) >> rhs.Int(i));
				break;
			case spv::OpShiftLeftLogical:
				dst.move(i, lhs.UInt(i) << rhs.UInt(i));
				break;
			case spv::OpBitwiseOr:
			case spv::OpLogicalOr:
				dst.move(i, lhs.UInt(i) | rhs.UInt(i));
				break;
			case spv::OpBitwiseXor:
				dst.move(i, lhs.UInt(i) ^ rhs.UInt(i));
				break;
			case spv::OpBitwiseAnd:
			case spv::OpLogicalAnd:
				dst.move(i, lhs.UInt(i) & rhs.UInt(i));
				break;
			case spv::OpSMulExtended:
				// Extended ops: result is a structure containing two members of the same type as lhs & rhs.
				// In our flat view then, component i is the i'th component of the first member;
				// component i + N is the i'th component of the second member.
				dst.move(i, lhs.Int(i) * rhs.Int(i));
				dst.move(i + lhsType.sizeInComponents, MulHigh(lhs.Int(i), rhs.Int(i)));
				break;
			case spv::OpUMulExtended:
				dst.move(i, lhs.UInt(i) * rhs.UInt(i));
				dst.move(i + lhsType.sizeInComponents, MulHigh(lhs.UInt(i), rhs.UInt(i)));
				break;
			case spv::OpIAddCarry:
				dst.move(i, lhs.UInt(i) + rhs.UInt(i));
				dst.move(i + lhsType.sizeInComponents, CmpLT(dst.UInt(i), lhs.UInt(i)) >> 31);
				break;
			case spv::OpISubBorrow:
				dst.move(i, lhs.UInt(i) - rhs.UInt(i));
				dst.move(i + lhsType.sizeInComponents, CmpLT(lhs.UInt(i), rhs.UInt(i)) >> 31);
				break;
			default:
				UNREACHABLE("%s", OpcodeName(insn.opcode()).c_str());
			}
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitDot(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		ASSERT(type.sizeInComponents == 1);
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto &lhsType = getType(getObject(insn.word(3)).type);
		auto lhs = GenericValue(this, routine, insn.word(3));
		auto rhs = GenericValue(this, routine, insn.word(4));

		dst.move(0, Dot(lhsType.sizeInComponents, lhs, rhs));
		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitSelect(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto cond = GenericValue(this, routine, insn.word(3));
		auto condIsScalar = (getType(cond.type).sizeInComponents == 1);
		auto lhs = GenericValue(this, routine, insn.word(4));
		auto rhs = GenericValue(this, routine, insn.word(5));

		for (auto i = 0u; i < type.sizeInComponents; i++)
		{
			auto sel = cond.Int(condIsScalar ? 0 : i);
			dst.move(i, (sel & lhs.Int(i)) | (~sel & rhs.Int(i)));   // TODO: IfThenElse()
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitExtendedInstruction(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto extInstIndex = static_cast<GLSLstd450>(insn.word(4));

		switch (extInstIndex)
		{
		case GLSLstd450FAbs:
		{
			auto src = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Abs(src.Float(i)));
			}
			break;
		}
		case GLSLstd450SAbs:
		{
			auto src = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Abs(src.Int(i)));
			}
			break;
		}
		case GLSLstd450Cross:
		{
			auto lhs = GenericValue(this, routine, insn.word(5));
			auto rhs = GenericValue(this, routine, insn.word(6));
			dst.move(0, lhs.Float(1) * rhs.Float(2) - rhs.Float(1) * lhs.Float(2));
			dst.move(1, lhs.Float(2) * rhs.Float(0) - rhs.Float(2) * lhs.Float(0));
			dst.move(2, lhs.Float(0) * rhs.Float(1) - rhs.Float(0) * lhs.Float(1));
			break;
		}
		case GLSLstd450Floor:
		{
			auto src = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Floor(src.Float(i)));
			}
			break;
		}
		case GLSLstd450Trunc:
		{
			auto src = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Trunc(src.Float(i)));
			}
			break;
		}
		case GLSLstd450Ceil:
		{
			auto src = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Ceil(src.Float(i)));
			}
			break;
		}
		case GLSLstd450Fract:
		{
			auto src = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Frac(src.Float(i)));
			}
			break;
		}
		case GLSLstd450Round:
		{
			auto src = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Round(src.Float(i)));
			}
			break;
		}
		case GLSLstd450RoundEven:
		{
			auto src = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				auto x = Round(src.Float(i));
				// dst = round(src) + ((round(src) < src) * 2 - 1) * (fract(src) == 0.5) * isOdd(round(src));
				dst.move(i, x + ((SIMD::Float(CmpLT(x, src.Float(i)) & SIMD::Int(1)) * SIMD::Float(2.0f)) - SIMD::Float(1.0f)) *
						SIMD::Float(CmpEQ(Frac(src.Float(i)), SIMD::Float(0.5f)) & SIMD::Int(1)) * SIMD::Float(Int4(x) & SIMD::Int(1)));
			}
			break;
		}
		case GLSLstd450FMin:
		{
			auto lhs = GenericValue(this, routine, insn.word(5));
			auto rhs = GenericValue(this, routine, insn.word(6));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Min(lhs.Float(i), rhs.Float(i)));
			}
			break;
		}
		case GLSLstd450FMax:
		{
			auto lhs = GenericValue(this, routine, insn.word(5));
			auto rhs = GenericValue(this, routine, insn.word(6));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Max(lhs.Float(i), rhs.Float(i)));
			}
			break;
		}
		case GLSLstd450SMin:
		{
			auto lhs = GenericValue(this, routine, insn.word(5));
			auto rhs = GenericValue(this, routine, insn.word(6));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Min(lhs.Int(i), rhs.Int(i)));
			}
			break;
		}
		case GLSLstd450SMax:
		{
			auto lhs = GenericValue(this, routine, insn.word(5));
			auto rhs = GenericValue(this, routine, insn.word(6));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Max(lhs.Int(i), rhs.Int(i)));
			}
			break;
		}
		case GLSLstd450UMin:
		{
			auto lhs = GenericValue(this, routine, insn.word(5));
			auto rhs = GenericValue(this, routine, insn.word(6));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Min(lhs.UInt(i), rhs.UInt(i)));
			}
			break;
		}
		case GLSLstd450UMax:
		{
			auto lhs = GenericValue(this, routine, insn.word(5));
			auto rhs = GenericValue(this, routine, insn.word(6));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Max(lhs.UInt(i), rhs.UInt(i)));
			}
			break;
		}
		case GLSLstd450Step:
		{
			auto edge = GenericValue(this, routine, insn.word(5));
			auto x = GenericValue(this, routine, insn.word(6));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, CmpNLT(x.Float(i), edge.Float(i)) & As<SIMD::Int>(SIMD::Float(1.0f)));
			}
			break;
		}
		case GLSLstd450SmoothStep:
		{
			auto edge0 = GenericValue(this, routine, insn.word(5));
			auto edge1 = GenericValue(this, routine, insn.word(6));
			auto x = GenericValue(this, routine, insn.word(7));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				auto tx = Min(Max((x.Float(i) - edge0.Float(i)) /
						(edge1.Float(i) - edge0.Float(i)), SIMD::Float(0.0f)), SIMD::Float(1.0f));
				dst.move(i, tx * tx * (Float4(3.0f) - Float4(2.0f) * tx));
			}
			break;
		}
		case GLSLstd450FMix:
		{
			auto x = GenericValue(this, routine, insn.word(5));
			auto y = GenericValue(this, routine, insn.word(6));
			auto a = GenericValue(this, routine, insn.word(7));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, a.Float(i) * (y.Float(i) - x.Float(i)) + x.Float(i));
			}
			break;
		}
		case GLSLstd450FClamp:
		{
			auto x = GenericValue(this, routine, insn.word(5));
			auto minVal = GenericValue(this, routine, insn.word(6));
			auto maxVal = GenericValue(this, routine, insn.word(7));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Min(Max(x.Float(i), minVal.Float(i)), maxVal.Float(i)));
			}
			break;
		}
		case GLSLstd450SClamp:
		{
			auto x = GenericValue(this, routine, insn.word(5));
			auto minVal = GenericValue(this, routine, insn.word(6));
			auto maxVal = GenericValue(this, routine, insn.word(7));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Min(Max(x.Int(i), minVal.Int(i)), maxVal.Int(i)));
			}
			break;
		}
		case GLSLstd450UClamp:
		{
			auto x = GenericValue(this, routine, insn.word(5));
			auto minVal = GenericValue(this, routine, insn.word(6));
			auto maxVal = GenericValue(this, routine, insn.word(7));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Min(Max(x.UInt(i), minVal.UInt(i)), maxVal.UInt(i)));
			}
			break;
		}
		case GLSLstd450FSign:
		{
			auto src = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				auto neg = As<SIMD::Int>(CmpLT(src.Float(i), SIMD::Float(-0.0f))) & As<SIMD::Int>(SIMD::Float(-1.0f));
				auto pos = As<SIMD::Int>(CmpNLE(src.Float(i), SIMD::Float(+0.0f))) & As<SIMD::Int>(SIMD::Float(1.0f));
				dst.move(i, neg | pos);
			}
			break;
		}
		case GLSLstd450SSign:
		{
			auto src = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				auto neg = CmpLT(src.Int(i), SIMD::Int(0)) & SIMD::Int(-1);
				auto pos = CmpNLE(src.Int(i), SIMD::Int(0)) & SIMD::Int(1);
				dst.move(i, neg | pos);
			}
			break;
		}
		case GLSLstd450Reflect:
		{
			auto I = GenericValue(this, routine, insn.word(5));
			auto N = GenericValue(this, routine, insn.word(6));

			SIMD::Float d = Dot(type.sizeInComponents, I, N);

			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, I.Float(i) - SIMD::Float(2.0f) * d * N.Float(i));
			}
			break;
		}
		case GLSLstd450Refract:
		{
			auto I = GenericValue(this, routine, insn.word(5));
			auto N = GenericValue(this, routine, insn.word(6));
			auto eta = GenericValue(this, routine, insn.word(7));

			SIMD::Float d = Dot(type.sizeInComponents, I, N);
			SIMD::Float k = SIMD::Float(1.0f) - eta.Float(0) * eta.Float(0) * (SIMD::Float(1.0f) - d * d);
			SIMD::Int pos = CmpNLT(k, SIMD::Float(0.0f));
			SIMD::Float t = (eta.Float(0) * d + Sqrt(k));

			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, pos & As<SIMD::Int>(eta.Float(0) * I.Float(i) - t * N.Float(i)));
			}
			break;
		}
		case GLSLstd450FaceForward:
		{
			auto N = GenericValue(this, routine, insn.word(5));
			auto I = GenericValue(this, routine, insn.word(6));
			auto Nref = GenericValue(this, routine, insn.word(7));

			SIMD::Float d = Dot(type.sizeInComponents, I, Nref);
			SIMD::Int neg = CmpLT(d, SIMD::Float(0.0f));

			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				auto n = N.Float(i);
				dst.move(i, (neg & As<SIMD::Int>(n)) | (~neg & As<SIMD::Int>(-n)));
			}
			break;
		}
		case GLSLstd450Length:
		{
			auto x = GenericValue(this, routine, insn.word(5));
			SIMD::Float d = Dot(getType(getObject(insn.word(5)).type).sizeInComponents, x, x);

			dst.move(0, Sqrt(d));
			break;
		}
		case GLSLstd450Normalize:
		{
			auto x = GenericValue(this, routine, insn.word(5));
			SIMD::Float d = Dot(getType(getObject(insn.word(5)).type).sizeInComponents, x, x);
			SIMD::Float invLength = SIMD::Float(1.0f) / Sqrt(d);

			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, invLength * x.Float(i));
			}
			break;
		}
		case GLSLstd450Distance:
		{
			auto p0 = GenericValue(this, routine, insn.word(5));
			auto p1 = GenericValue(this, routine, insn.word(6));
			auto p0Type = getType(p0.type);

			// sqrt(dot(p0-p1, p0-p1))
			SIMD::Float d = (p0.Float(0) - p1.Float(0)) * (p0.Float(0) - p1.Float(0));

			for (auto i = 1u; i < p0Type.sizeInComponents; i++)
			{
				d += (p0.Float(i) - p1.Float(i)) * (p0.Float(i) - p1.Float(i));
			}

			dst.move(0, Sqrt(d));
			break;
		}
		case GLSLstd450Modf:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			auto ptrId = Object::ID(insn.word(6));
			auto ptrTy = getType(getObject(ptrId).type);
			auto ptr = GetPointerToData(ptrId, 0, routine);
			bool interleavedByLane = IsStorageInterleavedByLane(ptrTy.storageClass);

			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				SIMD::Float whole, frac;
				std::tie(whole, frac) = Modf(val.Float(i));
				dst.move(i, frac);
				auto p = ptr + (i * sizeof(float));
				if (interleavedByLane) { p = interleaveByLane(p); }
				SIMD::Store(p, whole, state->activeLaneMask());
			}
			break;
		}
		case GLSLstd450ModfStruct:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			auto valTy = getType(val.type);

			for (auto i = 0u; i < valTy.sizeInComponents; i++)
			{
				SIMD::Float whole, frac;
				std::tie(whole, frac) = Modf(val.Float(i));
				dst.move(i, frac);
				dst.move(i + valTy.sizeInComponents, whole);
			}
			break;
		}
		case GLSLstd450PackSnorm4x8:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			dst.move(0, (SIMD::Int(Round(Min(Max(val.Float(0), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(127.0f))) &
						 SIMD::Int(0xFF)) |
						((SIMD::Int(Round(Min(Max(val.Float(1), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(127.0f))) &
						  SIMD::Int(0xFF)) << 8) |
						((SIMD::Int(Round(Min(Max(val.Float(2), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(127.0f))) &
						  SIMD::Int(0xFF)) << 16) |
						((SIMD::Int(Round(Min(Max(val.Float(3), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(127.0f))) &
						  SIMD::Int(0xFF)) << 24));
			break;
		}
		case GLSLstd450PackUnorm4x8:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			dst.move(0, (SIMD::UInt(Round(Min(Max(val.Float(0), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(255.0f)))) |
						((SIMD::UInt(Round(Min(Max(val.Float(1), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(255.0f)))) << 8) |
						((SIMD::UInt(Round(Min(Max(val.Float(2), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(255.0f)))) << 16) |
						((SIMD::UInt(Round(Min(Max(val.Float(3), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(255.0f)))) << 24));
			break;
		}
		case GLSLstd450PackSnorm2x16:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			dst.move(0, (SIMD::Int(Round(Min(Max(val.Float(0), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(32767.0f))) &
						 SIMD::Int(0xFFFF)) |
						((SIMD::Int(Round(Min(Max(val.Float(1), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(32767.0f))) &
						  SIMD::Int(0xFFFF)) << 16));
			break;
		}
		case GLSLstd450PackUnorm2x16:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			dst.move(0, (SIMD::UInt(Round(Min(Max(val.Float(0), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(65535.0f))) &
						 SIMD::UInt(0xFFFF)) |
						((SIMD::UInt(Round(Min(Max(val.Float(1), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(65535.0f))) &
						  SIMD::UInt(0xFFFF)) << 16));
			break;
		}
		case GLSLstd450PackHalf2x16:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			dst.move(0, FloatToHalfBits(val.UInt(0), false) | FloatToHalfBits(val.UInt(1), true));
			break;
		}
		case GLSLstd450UnpackSnorm4x8:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			dst.move(0, Min(Max(SIMD::Float(((val.Int(0)<<24) & SIMD::Int(0xFF000000))) * SIMD::Float(1.0f / float(0x7f000000)), SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
			dst.move(1, Min(Max(SIMD::Float(((val.Int(0)<<16) & SIMD::Int(0xFF000000))) * SIMD::Float(1.0f / float(0x7f000000)), SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
			dst.move(2, Min(Max(SIMD::Float(((val.Int(0)<<8) & SIMD::Int(0xFF000000))) * SIMD::Float(1.0f / float(0x7f000000)), SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
			dst.move(3, Min(Max(SIMD::Float(((val.Int(0)) & SIMD::Int(0xFF000000))) * SIMD::Float(1.0f / float(0x7f000000)), SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
			break;
		}
		case GLSLstd450UnpackUnorm4x8:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			dst.move(0, SIMD::Float((val.UInt(0) & SIMD::UInt(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(1, SIMD::Float(((val.UInt(0)>>8) & SIMD::UInt(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(2, SIMD::Float(((val.UInt(0)>>16) & SIMD::UInt(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(3, SIMD::Float(((val.UInt(0)>>24) & SIMD::UInt(0xFF))) * SIMD::Float(1.0f / 255.f));
			break;
		}
		case GLSLstd450UnpackSnorm2x16:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			// clamp(f / 32767.0, -1.0, 1.0)
			dst.move(0, Min(Max(SIMD::Float(As<SIMD::Int>((val.UInt(0) & SIMD::UInt(0x0000FFFF)) << 16)) *
								SIMD::Float(1.0f / float(0x7FFF0000)), SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
			dst.move(1, Min(Max(SIMD::Float(As<SIMD::Int>(val.UInt(0) & SIMD::UInt(0xFFFF0000))) * SIMD::Float(1.0f / float(0x7FFF0000)),
								SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
			break;
		}
		case GLSLstd450UnpackUnorm2x16:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			// f / 65535.0
			dst.move(0, SIMD::Float((val.UInt(0) & SIMD::UInt(0x0000FFFF)) << 16) * SIMD::Float(1.0f / float(0xFFFF0000)));
			dst.move(1, SIMD::Float(val.UInt(0) & SIMD::UInt(0xFFFF0000)) * SIMD::Float(1.0f / float(0xFFFF0000)));
			break;
		}
		case GLSLstd450UnpackHalf2x16:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			dst.move(0, halfToFloatBits(val.UInt(0) & SIMD::UInt(0x0000FFFF)));
			dst.move(1, halfToFloatBits((val.UInt(0) & SIMD::UInt(0xFFFF0000)) >> 16));
			break;
		}
		case GLSLstd450Fma:
		{
			auto a = GenericValue(this, routine, insn.word(5));
			auto b = GenericValue(this, routine, insn.word(6));
			auto c = GenericValue(this, routine, insn.word(7));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, FMA(a.Float(i), b.Float(i), c.Float(i)));
			}
			break;
		}
		case GLSLstd450Frexp:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			auto ptrId = Object::ID(insn.word(6));
			auto ptrTy = getType(getObject(ptrId).type);
			auto ptr = GetPointerToData(ptrId, 0, routine);
			bool interleavedByLane = IsStorageInterleavedByLane(ptrTy.storageClass);

			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				SIMD::Float significand;
				SIMD::Int exponent;
				std::tie(significand, exponent) = Frexp(val.Float(i));

				dst.move(i, significand);

				auto p = ptr + (i * sizeof(float));
				if (interleavedByLane) { p = interleaveByLane(p); }
				SIMD::Store(p, exponent, state->activeLaneMask());
			}
			break;
		}
		case GLSLstd450FrexpStruct:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			auto numComponents = getType(val.type).sizeInComponents;
			for (auto i = 0u; i < numComponents; i++)
			{
				auto significandAndExponent = Frexp(val.Float(i));
				dst.move(i, significandAndExponent.first);
				dst.move(i + numComponents, significandAndExponent.second);
			}
			break;
		}
		case GLSLstd450Ldexp:
		{
			auto significand = GenericValue(this, routine, insn.word(5));
			auto exponent = GenericValue(this, routine, insn.word(6));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				// Assumes IEEE 754
				auto in = significand.Float(i);
				auto significandExponent = Exponent(in);
				auto combinedExponent = exponent.Int(i) + significandExponent;
				auto isSignificandZero     = SIMD::UInt(CmpEQ(significand.Int(0), SIMD::Int(0)));
				auto isSignificandInf      = SIMD::UInt(IsInf(in));
				auto isSignificandNaN      = SIMD::UInt(IsNan(in));
				auto isExponentNotTooSmall = SIMD::UInt(CmpGE(combinedExponent, SIMD::Int(-126)));
				auto isExponentNotTooLarge = SIMD::UInt(CmpLE(combinedExponent, SIMD::Int(128)));
				auto isExponentInBounds    = isExponentNotTooSmall & isExponentNotTooLarge;

				SIMD::UInt v;
				v  = significand.UInt(i) & SIMD::UInt(0x7FFFFF); // Add significand.
				v |= (SIMD::UInt(combinedExponent + SIMD::Int(126)) << SIMD::UInt(23)); // Add exponent.
				v &= isExponentInBounds; // Clear v if the exponent is OOB.

				v |= significand.UInt(i) & SIMD::UInt(0x80000000); // Add sign bit.
				v |= ~isExponentNotTooLarge & SIMD::UInt(0x7F800000); // Mark as inf if the exponent is too great.

				// If the input significand is zero, inf or nan, just return the
				// input significand.
				auto passthrough = isSignificandZero | isSignificandInf | isSignificandNaN;
				v = (v & ~passthrough) | (significand.UInt(0) & passthrough);

				dst.move(i, As<SIMD::Float>(v));
			}
			break;
		}
		case GLSLstd450Radians:
		{
			auto degrees = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, degrees.Float(i) * SIMD::Float(PI / 180.0f));
			}
			break;
		}
		case GLSLstd450Degrees:
		{
			auto radians = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, radians.Float(i) * SIMD::Float(180.0f / PI));
			}
			break;
		}
		case GLSLstd450Sin:
		{
			auto radians = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Sin(radians.Float(i)));
			}
			break;
		}
		case GLSLstd450Cos:
		{
			auto radians = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Cos(radians.Float(i)));
			}
			break;
		}
		case GLSLstd450Tan:
		{
			auto radians = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Tan(radians.Float(i)));
			}
			break;
		}
		case GLSLstd450Asin:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Asin(val.Float(i)));
			}
			break;
		}
		case GLSLstd450Acos:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Acos(val.Float(i)));
			}
			break;
		}
		case GLSLstd450Atan:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Atan(val.Float(i)));
			}
			break;
		}
		case GLSLstd450Sinh:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Sinh(val.Float(i)));
			}
			break;
		}
		case GLSLstd450Cosh:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Cosh(val.Float(i)));
			}
			break;
		}
		case GLSLstd450Tanh:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Tanh(val.Float(i)));
			}
			break;
		}
		case GLSLstd450Asinh:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Asinh(val.Float(i)));
			}
			break;
		}
		case GLSLstd450Acosh:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Acosh(val.Float(i)));
			}
			break;
		}
		case GLSLstd450Atanh:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Atanh(val.Float(i)));
			}
			break;
		}
		case GLSLstd450Atan2:
		{
			auto x = GenericValue(this, routine, insn.word(5));
			auto y = GenericValue(this, routine, insn.word(6));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Atan2(x.Float(i), y.Float(i)));
			}
			break;
		}
		case GLSLstd450Pow:
		{
			auto x = GenericValue(this, routine, insn.word(5));
			auto y = GenericValue(this, routine, insn.word(6));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Pow(x.Float(i), y.Float(i)));
			}
			break;
		}
		case GLSLstd450Exp:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Exp(val.Float(i)));
			}
			break;
		}
		case GLSLstd450Log:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Log(val.Float(i)));
			}
			break;
		}
		case GLSLstd450Exp2:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Exp2(val.Float(i)));
			}
			break;
		}
		case GLSLstd450Log2:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Log2(val.Float(i)));
			}
			break;
		}
		case GLSLstd450Sqrt:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, Sqrt(val.Float(i)));
			}
			break;
		}
		case GLSLstd450InverseSqrt:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			Decorations d;
			ApplyDecorationsForId(&d, insn.word(5));
			if (d.RelaxedPrecision)
			{
				for (auto i = 0u; i < type.sizeInComponents; i++)
				{
					dst.move(i, RcpSqrt_pp(val.Float(i)));
				}
			}
			else
			{
				for (auto i = 0u; i < type.sizeInComponents; i++)
				{
					dst.move(i, SIMD::Float(1.0f) / Sqrt(val.Float(i)));
				}
			}
			break;
		}
		case GLSLstd450Determinant:
		{
			auto mat = GenericValue(this, routine, insn.word(5));
			auto numComponents = getType(mat.type).sizeInComponents;
			switch (numComponents)
			{
			case 4: // 2x2
				dst.move(0, Determinant(
					mat.Float(0), mat.Float(1),
					mat.Float(2), mat.Float(3)));
				break;
			case 9: // 3x3
				dst.move(0, Determinant(
					mat.Float(0), mat.Float(1), mat.Float(2),
					mat.Float(3), mat.Float(4), mat.Float(5),
					mat.Float(6), mat.Float(7), mat.Float(8)));
				break;
			case 16: // 4x4
				dst.move(0, Determinant(
					mat.Float(0),  mat.Float(1),  mat.Float(2),  mat.Float(3),
					mat.Float(4),  mat.Float(5),  mat.Float(6),  mat.Float(7),
					mat.Float(8),  mat.Float(9),  mat.Float(10), mat.Float(11),
					mat.Float(12), mat.Float(13), mat.Float(14), mat.Float(15)));
				break;
			default:
				UNREACHABLE("GLSLstd450Determinant can only operate with square matrices. Got %d elements", int(numComponents));
			}
			break;
		}
		case GLSLstd450MatrixInverse:
		{
			auto mat = GenericValue(this, routine, insn.word(5));
			auto numComponents = getType(mat.type).sizeInComponents;
			switch (numComponents)
			{
			case 4: // 2x2
			{
				auto inv = MatrixInverse(
					mat.Float(0), mat.Float(1),
					mat.Float(2), mat.Float(3));
				for (uint32_t i = 0; i < inv.size(); i++)
				{
					dst.move(i, inv[i]);
				}
				break;
			}
			case 9: // 3x3
			{
				auto inv = MatrixInverse(
					mat.Float(0), mat.Float(1), mat.Float(2),
					mat.Float(3), mat.Float(4), mat.Float(5),
					mat.Float(6), mat.Float(7), mat.Float(8));
				for (uint32_t i = 0; i < inv.size(); i++)
				{
					dst.move(i, inv[i]);
				}
				break;
			}
			case 16: // 4x4
			{
				auto inv = MatrixInverse(
					mat.Float(0),  mat.Float(1),  mat.Float(2),  mat.Float(3),
					mat.Float(4),  mat.Float(5),  mat.Float(6),  mat.Float(7),
					mat.Float(8),  mat.Float(9),  mat.Float(10), mat.Float(11),
					mat.Float(12), mat.Float(13), mat.Float(14), mat.Float(15));
				for (uint32_t i = 0; i < inv.size(); i++)
				{
					dst.move(i, inv[i]);
				}
				break;
			}
			default:
				UNREACHABLE("GLSLstd450MatrixInverse can only operate with square matrices. Got %d elements", int(numComponents));
			}
			break;
		}
		case GLSLstd450IMix:
		{
			UNREACHABLE("GLSLstd450IMix has been removed from the specification");
			break;
		}
		case GLSLstd450PackDouble2x32:
		{
			UNSUPPORTED("SPIR-V Float64 Capability (GLSLstd450PackDouble2x32)");
			break;
		}
		case GLSLstd450UnpackDouble2x32:
		{
			UNSUPPORTED("SPIR-V Float64 Capability (GLSLstd450UnpackDouble2x32)");
			break;
		}
		case GLSLstd450FindILsb:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				auto v = val.UInt(i);
				dst.move(i, Cttz(v, true) | CmpEQ(v, SIMD::UInt(0)));
			}
			break;
		}
		case GLSLstd450FindSMsb:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				auto v = val.UInt(i) ^ As<SIMD::UInt>(CmpLT(val.Int(i), SIMD::Int(0)));
				dst.move(i, SIMD::UInt(31) - Ctlz(v, false));
			}
			break;
		}
		case GLSLstd450FindUMsb:
		{
			auto val = GenericValue(this, routine, insn.word(5));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, SIMD::UInt(31) - Ctlz(val.UInt(i), false));
			}
			break;
		}
		case GLSLstd450InterpolateAtCentroid:
		{
			UNSUPPORTED("SPIR-V SampleRateShading Capability (GLSLstd450InterpolateAtCentroid)");
			break;
		}
		case GLSLstd450InterpolateAtSample:
		{
			UNSUPPORTED("SPIR-V SampleRateShading Capability (GLSLstd450InterpolateAtCentroid)");
			break;
		}
		case GLSLstd450InterpolateAtOffset:
		{
			UNSUPPORTED("SPIR-V SampleRateShading Capability (GLSLstd450InterpolateAtCentroid)");
			break;
		}
		case GLSLstd450NMin:
		{
			auto x = GenericValue(this, routine, insn.word(5));
			auto y = GenericValue(this, routine, insn.word(6));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, NMin(x.Float(i), y.Float(i)));
			}
			break;
		}
		case GLSLstd450NMax:
		{
			auto x = GenericValue(this, routine, insn.word(5));
			auto y = GenericValue(this, routine, insn.word(6));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				dst.move(i, NMax(x.Float(i), y.Float(i)));
			}
			break;
		}
		case GLSLstd450NClamp:
		{
			auto x = GenericValue(this, routine, insn.word(5));
			auto minVal = GenericValue(this, routine, insn.word(6));
			auto maxVal = GenericValue(this, routine, insn.word(7));
			for (auto i = 0u; i < type.sizeInComponents; i++)
			{
				auto clamp = NMin(NMax(x.Float(i), minVal.Float(i)), maxVal.Float(i));
				dst.move(i, clamp);
			}
			break;
		}
		default:
			UNREACHABLE("ExtInst %d", int(extInstIndex));
			break;
		}

		return EmitResult::Continue;
	}

	std::memory_order SpirvShader::MemoryOrder(spv::MemorySemanticsMask memorySemantics)
	{
		auto control = static_cast<uint32_t>(memorySemantics) & static_cast<uint32_t>(
			spv::MemorySemanticsAcquireMask |
			spv::MemorySemanticsReleaseMask |
			spv::MemorySemanticsAcquireReleaseMask |
			spv::MemorySemanticsSequentiallyConsistentMask
		);
		switch (control)
		{
		case spv::MemorySemanticsMaskNone:                   return std::memory_order_relaxed;
		case spv::MemorySemanticsAcquireMask:                return std::memory_order_acquire;
		case spv::MemorySemanticsReleaseMask:                return std::memory_order_release;
		case spv::MemorySemanticsAcquireReleaseMask:         return std::memory_order_acq_rel;
		case spv::MemorySemanticsSequentiallyConsistentMask: return std::memory_order_acq_rel;  // Vulkan 1.1: "SequentiallyConsistent is treated as AcquireRelease"
		default:
			// "it is invalid for more than one of these four bits to be set:
			// Acquire, Release, AcquireRelease, or SequentiallyConsistent."
			UNREACHABLE("MemorySemanticsMask: %x", int(control));
			return std::memory_order_acq_rel;
		}
	}

	SIMD::Float SpirvShader::Dot(unsigned numComponents, GenericValue const & x, GenericValue const & y) const
	{
		SIMD::Float d = x.Float(0) * y.Float(0);

		for (auto i = 1u; i < numComponents; i++)
		{
			d += x.Float(i) * y.Float(i);
		}

		return d;
	}

	SIMD::UInt SpirvShader::FloatToHalfBits(SIMD::UInt floatBits, bool storeInUpperBits) const
	{
		static const uint32_t mask_sign = 0x80000000u;
		static const uint32_t mask_round = ~0xfffu;
		static const uint32_t c_f32infty = 255 << 23;
		static const uint32_t c_magic = 15 << 23;
		static const uint32_t c_nanbit = 0x200;
		static const uint32_t c_infty_as_fp16 = 0x7c00;
		static const uint32_t c_clamp = (31 << 23) - 0x1000;

		SIMD::UInt justsign = SIMD::UInt(mask_sign) & floatBits;
		SIMD::UInt absf = floatBits ^ justsign;
		SIMD::UInt b_isnormal = CmpNLE(SIMD::UInt(c_f32infty), absf);

		// Note: this version doesn't round to the nearest even in case of a tie as defined by IEEE 754-2008, it rounds to +inf
		//       instead of nearest even, since that's fine for GLSL ES 3.0's needs (see section 2.1.1 Floating-Point Computation)
		SIMD::UInt joined = ((((As<SIMD::UInt>(Min(As<SIMD::Float>(absf & SIMD::UInt(mask_round)) * As<SIMD::Float>(SIMD::UInt(c_magic)),
										 As<SIMD::Float>(SIMD::UInt(c_clamp))))) - SIMD::UInt(mask_round)) >> 13) & b_isnormal) |
					   ((b_isnormal ^ SIMD::UInt(0xFFFFFFFF)) & ((CmpNLE(absf, SIMD::UInt(c_f32infty)) & SIMD::UInt(c_nanbit)) |
															SIMD::UInt(c_infty_as_fp16)));

		return storeInUpperBits ? ((joined << 16) | justsign) : joined | (justsign >> 16);
	}

	std::pair<SIMD::Float, SIMD::Int> SpirvShader::Frexp(RValue<SIMD::Float> val) const
	{
		// Assumes IEEE 754
		auto v = As<SIMD::UInt>(val);
		auto isNotZero = CmpNEQ(v & SIMD::UInt(0x7FFFFFFF), SIMD::UInt(0));
		auto zeroSign = v & SIMD::UInt(0x80000000) & ~isNotZero;
		auto significand = As<SIMD::Float>((((v & SIMD::UInt(0x807FFFFF)) | SIMD::UInt(0x3F000000)) & isNotZero) | zeroSign);
		auto exponent = Exponent(val) & SIMD::Int(isNotZero);
		return std::make_pair(significand, exponent);
	}

	SpirvShader::EmitResult SpirvShader::EmitAny(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		ASSERT(type.sizeInComponents == 1);
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto &srcType = getType(getObject(insn.word(3)).type);
		auto src = GenericValue(this, routine, insn.word(3));

		SIMD::UInt result = src.UInt(0);

		for (auto i = 1u; i < srcType.sizeInComponents; i++)
		{
			result |= src.UInt(i);
		}

		dst.move(0, result);
		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitAll(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto &type = getType(insn.word(1));
		ASSERT(type.sizeInComponents == 1);
		auto &dst = routine->createIntermediate(insn.word(2), type.sizeInComponents);
		auto &srcType = getType(getObject(insn.word(3)).type);
		auto src = GenericValue(this, routine, insn.word(3));

		SIMD::UInt result = src.UInt(0);

		for (auto i = 1u; i < srcType.sizeInComponents; i++)
		{
			result &= src.UInt(i);
		}

		dst.move(0, result);
		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitBranch(InsnIterator insn, EmitState *state) const
	{
		auto target = Block::ID(insn.word(1));
		state->addActiveLaneMaskEdge(state->currentBlock, target, state->activeLaneMask());
		return EmitResult::Terminator;
	}

	SpirvShader::EmitResult SpirvShader::EmitBranchConditional(InsnIterator insn, EmitState *state) const
	{
		auto block = getBlock(state->currentBlock);
		ASSERT(block.branchInstruction == insn);

		auto condId = Object::ID(block.branchInstruction.word(1));
		auto trueBlockId = Block::ID(block.branchInstruction.word(2));
		auto falseBlockId = Block::ID(block.branchInstruction.word(3));

		auto cond = GenericValue(this, state->routine, condId);
		ASSERT_MSG(getType(cond.type).sizeInComponents == 1, "Condition must be a Boolean type scalar");

		// TODO: Optimize for case where all lanes take same path.

		state->addOutputActiveLaneMaskEdge(trueBlockId, cond.Int(0));
		state->addOutputActiveLaneMaskEdge(falseBlockId, ~cond.Int(0));

		return EmitResult::Terminator;
	}

	SpirvShader::EmitResult SpirvShader::EmitSwitch(InsnIterator insn, EmitState *state) const
	{
		auto block = getBlock(state->currentBlock);
		ASSERT(block.branchInstruction == insn);

		auto selId = Object::ID(block.branchInstruction.word(1));

		auto sel = GenericValue(this, state->routine, selId);
		ASSERT_MSG(getType(sel.type).sizeInComponents == 1, "Selector must be a scalar");

		auto numCases = (block.branchInstruction.wordCount() - 3) / 2;

		// TODO: Optimize for case where all lanes take same path.

		SIMD::Int defaultLaneMask = state->activeLaneMask();

		// Gather up the case label matches and calculate defaultLaneMask.
		std::vector<RValue<SIMD::Int>> caseLabelMatches;
		caseLabelMatches.reserve(numCases);
		for (uint32_t i = 0; i < numCases; i++)
		{
			auto label = block.branchInstruction.word(i * 2 + 3);
			auto caseBlockId = Block::ID(block.branchInstruction.word(i * 2 + 4));
			auto caseLabelMatch = CmpEQ(sel.Int(0), SIMD::Int(label));
			state->addOutputActiveLaneMaskEdge(caseBlockId, caseLabelMatch);
			defaultLaneMask &= ~caseLabelMatch;
		}

		auto defaultBlockId = Block::ID(block.branchInstruction.word(2));
		state->addOutputActiveLaneMaskEdge(defaultBlockId, defaultLaneMask);

		return EmitResult::Terminator;
	}

	SpirvShader::EmitResult SpirvShader::EmitUnreachable(InsnIterator insn, EmitState *state) const
	{
		// TODO: Log something in this case?
		state->setActiveLaneMask(SIMD::Int(0));
		return EmitResult::Terminator;
	}

	SpirvShader::EmitResult SpirvShader::EmitReturn(InsnIterator insn, EmitState *state) const
	{
		state->setActiveLaneMask(SIMD::Int(0));
		return EmitResult::Terminator;
	}

	SpirvShader::EmitResult SpirvShader::EmitKill(InsnIterator insn, EmitState *state) const
	{
		state->routine->killMask |= SignMask(state->activeLaneMask());
		state->setActiveLaneMask(SIMD::Int(0));
		return EmitResult::Terminator;
	}

	SpirvShader::EmitResult SpirvShader::EmitPhi(InsnIterator insn, EmitState *state) const
	{
		auto currentBlock = getBlock(state->currentBlock);
		if (!currentBlock.isLoopMerge)
		{
			// If this is a loop merge block, then don't attempt to update the
			// phi values from the ins. EmitLoop() has had to take special care
			// of this phi in order to correctly deal with divergent lanes.
			StorePhi(state->currentBlock, insn, state, currentBlock.ins);
		}
		LoadPhi(insn, state);
		return EmitResult::Continue;
	}

	void SpirvShader::LoadPhi(InsnIterator insn, EmitState *state) const
	{
		auto routine = state->routine;
		auto typeId = Type::ID(insn.word(1));
		auto type = getType(typeId);
		auto objectId = Object::ID(insn.word(2));

		auto storageIt = state->routine->phis.find(objectId);
		ASSERT(storageIt != state->routine->phis.end());
		auto &storage = storageIt->second;

		auto &dst = routine->createIntermediate(objectId, type.sizeInComponents);
		for(uint32_t i = 0; i < type.sizeInComponents; i++)
		{
			dst.move(i, storage[i]);
		}
	}

	void SpirvShader::StorePhi(Block::ID currentBlock, InsnIterator insn, EmitState *state, std::unordered_set<SpirvShader::Block::ID> const& filter) const
	{
		auto routine = state->routine;
		auto typeId = Type::ID(insn.word(1));
		auto type = getType(typeId);
		auto objectId = Object::ID(insn.word(2));

		auto storageIt = state->routine->phis.find(objectId);
		ASSERT(storageIt != state->routine->phis.end());
		auto &storage = storageIt->second;

		for (uint32_t w = 3; w < insn.wordCount(); w += 2)
		{
			auto varId = Object::ID(insn.word(w + 0));
			auto blockId = Block::ID(insn.word(w + 1));

			if (filter.count(blockId) == 0)
			{
				continue;
			}

			auto mask = GetActiveLaneMaskEdge(state, blockId, currentBlock);
			auto in = GenericValue(this, routine, varId);

			for (uint32_t i = 0; i < type.sizeInComponents; i++)
			{
				storage[i] = As<SIMD::Float>((As<SIMD::Int>(storage[i]) & ~mask) | (in.Int(i) & mask));
			}
		}
	}

	SpirvShader::EmitResult SpirvShader::EmitImageSampleImplicitLod(Variant variant, InsnIterator insn, EmitState *state) const
	{
		return EmitImageSample({variant, Implicit}, insn, state);
	}

	SpirvShader::EmitResult SpirvShader::EmitImageGather(Variant variant, InsnIterator insn, EmitState *state) const
	{
		ImageInstruction instruction = {variant, Gather};
		instruction.gatherComponent = !instruction.isDref() ? getObject(insn.word(5)).constantValue[0] : 0;

		return EmitImageSample(instruction, insn, state);
	}

	SpirvShader::EmitResult SpirvShader::EmitImageSampleExplicitLod(Variant variant, InsnIterator insn, EmitState *state) const
	{
		auto isDref = (variant == Dref) || (variant == ProjDref);
		uint32_t imageOperands = static_cast<spv::ImageOperandsMask>(insn.word(isDref ? 6 : 5));
		imageOperands &= ~spv::ImageOperandsConstOffsetMask;  // Dealt with later.

		if((imageOperands & spv::ImageOperandsLodMask) == imageOperands)
		{
			return EmitImageSample({variant, Lod}, insn, state);
		}
		else if((imageOperands & spv::ImageOperandsGradMask) == imageOperands)
		{
			return EmitImageSample({variant, Grad}, insn, state);
		}
		else UNIMPLEMENTED("Image Operands %x", imageOperands);
		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitImageFetch(InsnIterator insn, EmitState *state) const
	{
		return EmitImageSample({None, Fetch}, insn, state);
	}

	SpirvShader::EmitResult SpirvShader::EmitImageSample(ImageInstruction instruction, InsnIterator insn, EmitState *state) const
	{
		Type::ID resultTypeId = insn.word(1);
		Object::ID resultId = insn.word(2);
		Object::ID sampledImageId = insn.word(3);
		Object::ID coordinateId = insn.word(4);
		auto &resultType = getType(resultTypeId);

		auto &result = state->routine->createIntermediate(resultId, resultType.sizeInComponents);
		auto imageDescriptor = state->routine->getPointer(sampledImageId).base; // vk::SampledImageDescriptor*

		// If using a separate sampler, look through the OpSampledImage instruction to find the sampler descriptor
		auto &sampledImage = getObject(sampledImageId);
		auto samplerDescriptor = (sampledImage.opcode() == spv::OpSampledImage) ?
				state->routine->getPointer(sampledImage.definition.word(4)).base : imageDescriptor;

		auto coordinate = GenericValue(this, state->routine, coordinateId);
		auto &coordinateType = getType(coordinate.type);

		Pointer<Byte> sampler = samplerDescriptor + OFFSET(vk::SampledImageDescriptor, sampler); // vk::Sampler*
		Pointer<Byte> texture = imageDescriptor + OFFSET(vk::SampledImageDescriptor, texture);  // sw::Texture*

		uint32_t imageOperands = spv::ImageOperandsMaskNone;
		bool lodOrBias = false;
		Object::ID lodOrBiasId = 0;
		bool grad = false;
		Object::ID gradDxId = 0;
		Object::ID gradDyId = 0;
		bool constOffset = false;
		Object::ID offsetId = 0;
		bool sample = false;

		uint32_t operand = (instruction.isDref() || instruction.samplerMethod == Gather) ? 6 : 5;

		if(insn.wordCount() > operand)
		{
			imageOperands = static_cast<spv::ImageOperandsMask>(insn.word(operand++));

			if(imageOperands & spv::ImageOperandsBiasMask)
			{
				lodOrBias = true;
				lodOrBiasId = insn.word(operand);
				operand++;
				imageOperands &= ~spv::ImageOperandsBiasMask;

				ASSERT(instruction.samplerMethod == Implicit);
				instruction.samplerMethod = Bias;
			}

			if(imageOperands & spv::ImageOperandsLodMask)
			{
				lodOrBias = true;
				lodOrBiasId = insn.word(operand);
				operand++;
				imageOperands &= ~spv::ImageOperandsLodMask;
			}

			if(imageOperands & spv::ImageOperandsGradMask)
			{
				ASSERT(!lodOrBias);  // SPIR-V 1.3: "It is invalid to set both the Lod and Grad bits." Bias is for ImplicitLod, Grad for ExplicitLod.
				grad = true;
				gradDxId = insn.word(operand + 0);
				gradDyId = insn.word(operand + 1);
				operand += 2;
				imageOperands &= ~spv::ImageOperandsGradMask;
			}

			if(imageOperands & spv::ImageOperandsConstOffsetMask)
			{
				constOffset = true;
				offsetId = insn.word(operand);
				operand++;
				imageOperands &= ~spv::ImageOperandsConstOffsetMask;
			}

			if(imageOperands & spv::ImageOperandsSampleMask)
			{
				UNIMPLEMENTED("Image operand %x", spv::ImageOperandsSampleMask); (void)sample;
				sample = true;
				imageOperands &= ~spv::ImageOperandsSampleMask;
			}

			if(imageOperands != 0)
			{
				UNIMPLEMENTED("Image operand %x", imageOperands);
			}
		}

		Array<SIMD::Float> in(16);  // Maximum 16 input parameter components.

		uint32_t coordinates = coordinateType.sizeInComponents - instruction.isProj();
		instruction.coordinates = coordinates;

		uint32_t i = 0;
		for( ; i < coordinates; i++)
		{
			if(instruction.isProj())
			{
				in[i] = coordinate.Float(i) / coordinate.Float(coordinates);  // TODO(b/129523279): Optimize using reciprocal.
			}
			else
			{
				in[i] = coordinate.Float(i);
			}
		}

		if(instruction.isDref())
		{
			auto drefValue = GenericValue(this, state->routine, insn.word(5));

			if(instruction.isProj())
			{
				in[i] = drefValue.Float(0) / coordinate.Float(coordinates);  // TODO(b/129523279): Optimize using reciprocal.
			}
			else
			{
				in[i] = drefValue.Float(0);
			}

			i++;
		}

		if(lodOrBias)
		{
			auto lodValue = GenericValue(this, state->routine, lodOrBiasId);
			in[i] = lodValue.Float(0);
			i++;
		}
		else if(grad)
		{
			auto dxValue = GenericValue(this, state->routine, gradDxId);
			auto dyValue = GenericValue(this, state->routine, gradDyId);
			auto &dxyType = getType(dxValue.type);
			ASSERT(dxyType.sizeInComponents == getType(dyValue.type).sizeInComponents);

			instruction.gradComponents = dxyType.sizeInComponents;

			for(uint32_t j = 0; j < dxyType.sizeInComponents; j++, i++)
			{
				in[i] = dxValue.Float(j);
			}

			for(uint32_t j = 0; j < dxyType.sizeInComponents; j++, i++)
			{
				in[i] = dyValue.Float(j);
			}
		}
		else if (instruction.samplerMethod == Fetch)
		{
			// The instruction didn't provide a lod operand, but the sampler's Fetch
			// function requires one to be present. If no lod is supplied, the default
			// is zero.
			in[i] = As<SIMD::Float>(SIMD::Int(0));
			i++;
		}

		if(constOffset)
		{
			auto offsetValue = GenericValue(this, state->routine, offsetId);
			auto &offsetType = getType(offsetValue.type);

			instruction.samplerOption = Offset;
			instruction.offsetComponents = offsetType.sizeInComponents;

			for(uint32_t j = 0; j < offsetType.sizeInComponents; j++, i++)
			{
				in[i] = offsetValue.Float(j);  // Integer values, but transfered as float.
			}
		}

		auto samplerFunc = Call(getImageSampler, instruction.parameters, imageDescriptor, sampler);

		Array<SIMD::Float> out(4);
		Call<ImageSampler>(samplerFunc, texture, sampler, &in[0], &out[0], state->routine->constants);

		for (auto i = 0u; i < resultType.sizeInComponents; i++) { result.move(i, out[i]); }

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitImageQuerySizeLod(InsnIterator insn, EmitState *state) const
	{
		auto &resultTy = getType(Type::ID(insn.word(1)));
		auto resultId = Object::ID(insn.word(2));
		auto imageId = Object::ID(insn.word(3));
		auto lodId = Object::ID(insn.word(4));

		auto &dst = state->routine->createIntermediate(resultId, resultTy.sizeInComponents);
		GetImageDimensions(state->routine, resultTy, imageId, lodId, dst);

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitImageQuerySize(InsnIterator insn, EmitState *state) const
	{
		auto &resultTy = getType(Type::ID(insn.word(1)));
		auto resultId = Object::ID(insn.word(2));
		auto imageId = Object::ID(insn.word(3));
		auto lodId = Object::ID(0);

		auto &dst = state->routine->createIntermediate(resultId, resultTy.sizeInComponents);
		GetImageDimensions(state->routine, resultTy, imageId, lodId, dst);

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitImageQueryLod(InsnIterator insn, EmitState *state) const
	{
		return EmitImageSample({None, Query}, insn, state);
	}

	void SpirvShader::GetImageDimensions(SpirvRoutine const *routine, Type const &resultTy, Object::ID imageId, Object::ID lodId, Intermediate &dst) const
	{
		auto &image = getObject(imageId);
		auto &imageType = getType(image.type);

		ASSERT(imageType.definition.opcode() == spv::OpTypeImage);
		bool isArrayed = imageType.definition.word(5) != 0;
		bool isCubeMap = imageType.definition.word(3) == spv::DimCube;

		const DescriptorDecorations &d = descriptorDecorations.at(imageId);
		auto setLayout = routine->pipelineLayout->getDescriptorSetLayout(d.DescriptorSet);
		auto &bindingLayout = setLayout->getBindingLayout(d.Binding);

		Pointer<Byte> descriptor = routine->getPointer(imageId).base;

		Pointer<Int> extent;
		Int arrayLayers;

		switch (bindingLayout.descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		{
			extent = descriptor + OFFSET(vk::StorageImageDescriptor, extent); // int[3]*
			arrayLayers = *Pointer<Int>(descriptor + OFFSET(vk::StorageImageDescriptor, arrayLayers)); // uint32_t
			break;
		}
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		{
			extent = descriptor + OFFSET(vk::SampledImageDescriptor, extent); // int[3]*
			arrayLayers = *Pointer<Int>(descriptor + OFFSET(vk::SampledImageDescriptor, arrayLayers)); // uint32_t
			break;
		}
		default:
			UNREACHABLE("Image descriptorType: %d", int(bindingLayout.descriptorType));
		}

		auto dimensions = resultTy.sizeInComponents - (isArrayed ? 1 : 0);
		std::vector<Int> out;
		if (lodId != 0)
		{
			auto lodVal = GenericValue(this, routine, lodId);
			ASSERT(getType(lodVal.type).sizeInComponents == 1);
			auto lod = lodVal.Int(0);
			auto one = SIMD::Int(1);
			for (uint32_t i = 0; i < dimensions; i++)
			{
				dst.move(i, Max(SIMD::Int(extent[i]) >> lod, one));
			}
		}
		else
		{
			for (uint32_t i = 0; i < dimensions; i++)
			{
				dst.move(i, SIMD::Int(extent[i]));
			}
		}

		if (isArrayed)
		{
			auto numElements = isCubeMap ? (arrayLayers / 6) : RValue<Int>(arrayLayers);
			dst.move(dimensions, SIMD::Int(numElements));
		}
	}

	SpirvShader::EmitResult SpirvShader::EmitImageQueryLevels(InsnIterator insn, EmitState *state) const
	{
		auto &resultTy = getType(Type::ID(insn.word(1)));
		ASSERT(resultTy.sizeInComponents == 1);
		auto resultId = Object::ID(insn.word(2));
		auto imageId = Object::ID(insn.word(3));

		const DescriptorDecorations &d = descriptorDecorations.at(imageId);
		auto setLayout = state->routine->pipelineLayout->getDescriptorSetLayout(d.DescriptorSet);
		auto &bindingLayout = setLayout->getBindingLayout(d.Binding);

		Pointer<Byte> descriptor = state->routine->getPointer(imageId).base;
		Int mipLevels = 0;
		switch (bindingLayout.descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			mipLevels = *Pointer<Int>(descriptor + OFFSET(vk::SampledImageDescriptor, mipLevels)); // uint32_t
			break;
		default:
			UNREACHABLE("Image descriptorType: %d", int(bindingLayout.descriptorType));
		}

		auto &dst = state->routine->createIntermediate(resultId, 1);
		dst.move(0, SIMD::Int(mipLevels));

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitImageQuerySamples(InsnIterator insn, EmitState *state) const
	{
		auto &resultTy = getType(Type::ID(insn.word(1)));
		ASSERT(resultTy.sizeInComponents == 1);
		auto resultId = Object::ID(insn.word(2));
		auto imageId = Object::ID(insn.word(3));
		auto imageTy = getType(getObject(imageId).type);
		ASSERT(imageTy.definition.opcode() == spv::OpTypeImage);
		ASSERT(imageTy.definition.word(3) == spv::Dim2D);
		ASSERT(imageTy.definition.word(6 /* MS */) == 1);

		const DescriptorDecorations &d = descriptorDecorations.at(imageId);
		auto setLayout = state->routine->pipelineLayout->getDescriptorSetLayout(d.DescriptorSet);
		auto &bindingLayout = setLayout->getBindingLayout(d.Binding);

		Pointer<Byte> descriptor = state->routine->getPointer(imageId).base;
		Int sampleCount = 0;
		switch (bindingLayout.descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			sampleCount = *Pointer<Int>(descriptor + OFFSET(vk::StorageImageDescriptor, sampleCount)); // uint32_t
			break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			sampleCount = *Pointer<Int>(descriptor + OFFSET(vk::SampledImageDescriptor, sampleCount)); // uint32_t
			break;
		default:
			UNREACHABLE("Image descriptorType: %d", int(bindingLayout.descriptorType));
		}

		auto &dst = state->routine->createIntermediate(resultId, 1);
		dst.move(0, SIMD::Int(sampleCount));

		return EmitResult::Continue;
	}

	SIMD::Pointer SpirvShader::GetTexelAddress(SpirvRoutine const *routine, SIMD::Pointer ptr, GenericValue const & coordinate, Type const & imageType, Pointer<Byte> descriptor, int texelSize, Object::ID sampleId, bool useStencilAspect) const
	{
		bool isArrayed = imageType.definition.word(5) != 0;
		auto dim = static_cast<spv::Dim>(imageType.definition.word(3));
		int dims = getType(coordinate.type).sizeInComponents - (isArrayed ? 1 : 0);

		SIMD::Int u = coordinate.Int(0);
		SIMD::Int v = SIMD::Int(0);

		if (getType(coordinate.type).sizeInComponents > 1)
		{
			v = coordinate.Int(1);
		}

		if (dim == spv::DimSubpassData)
		{
			u += routine->windowSpacePosition[0];
			v += routine->windowSpacePosition[1];
		}

		if (useStencilAspect)
		{
			// Adjust addressing for quad layout. Pitches are already correct for the stencil aspect.
			// In the quad-layout block, pixel order is [x0,y0   x1,y0   x0,y1   x1,y1]
			u = ((v & SIMD::Int(1)) << 1) | ((u << 1) - (u & SIMD::Int(1)));
			v &= SIMD::Int(~1);
		}

		auto rowPitch = SIMD::Int(*Pointer<Int>(descriptor + (useStencilAspect
															  ? OFFSET(vk::StorageImageDescriptor, stencilRowPitchBytes)
															  : OFFSET(vk::StorageImageDescriptor, rowPitchBytes))));
		auto slicePitch = SIMD::Int(
				*Pointer<Int>(descriptor + (useStencilAspect
											? OFFSET(vk::StorageImageDescriptor, stencilSlicePitchBytes)
											: OFFSET(vk::StorageImageDescriptor, slicePitchBytes))));
		auto samplePitch = SIMD::Int(
				*Pointer<Int>(descriptor + (useStencilAspect
											? OFFSET(vk::StorageImageDescriptor, stencilSamplePitchBytes)
											: OFFSET(vk::StorageImageDescriptor, samplePitchBytes))));

		ptr += u * SIMD::Int(texelSize);
		if (dims > 1)
		{
			ptr += v * rowPitch;
		}
		if (dims > 2)
		{
			ptr += coordinate.Int(2) * slicePitch;
		}
		if (isArrayed)
		{
			ptr += coordinate.Int(dims) * slicePitch;
		}

		if (sampleId.value())
		{
			GenericValue sample{this, routine, sampleId};
			ptr += sample.Int(0) * samplePitch;
		}

		return ptr;
	}

	void SpirvShader::Yield(YieldResult res) const
	{
		rr::Yield(RValue<Int>(int(res)));
	}

	SpirvShader::EmitResult SpirvShader::EmitImageRead(InsnIterator insn, EmitState *state) const
	{
		auto &resultType = getType(Type::ID(insn.word(1)));
		auto imageId = Object::ID(insn.word(3));
		auto &image = getObject(imageId);
		auto &imageType = getType(image.type);
		Object::ID resultId = insn.word(2);

		Object::ID sampleId = 0;

		if (insn.wordCount() > 5)
		{
			int operand = 6;
			auto imageOperands = insn.word(5);
			if (imageOperands & spv::ImageOperandsSampleMask)
			{
				sampleId = insn.word(operand++);
				imageOperands &= ~spv::ImageOperandsSampleMask;
			}

			// Should be no remaining image operands.
			ASSERT(!imageOperands);
		}

		ASSERT(imageType.definition.opcode() == spv::OpTypeImage);
		auto dim = static_cast<spv::Dim>(imageType.definition.word(3));

		auto coordinate = GenericValue(this, state->routine, insn.word(4));
		const DescriptorDecorations &d = descriptorDecorations.at(imageId);

		// For subpass data, format in the instruction is spv::ImageFormatUnknown. Get it from
		// the renderpass data instead. In all other cases, we can use the format in the instruction.
		auto vkFormat = (dim == spv::DimSubpassData)
						? inputAttachmentFormats[d.InputAttachmentIndex]
						: SpirvFormatToVulkanFormat(static_cast<spv::ImageFormat>(imageType.definition.word(8)));

		// Depth+Stencil image attachments select aspect based on the Sampled Type of the
		// OpTypeImage. If float, then we want the depth aspect. If int, we want the stencil aspect.
		auto useStencilAspect = (vkFormat == VK_FORMAT_D32_SFLOAT_S8_UINT &&
				getType(imageType.definition.word(2)).opcode() == spv::OpTypeInt);

		if (useStencilAspect)
		{
			vkFormat = VK_FORMAT_S8_UINT;
		}

		auto pointer = state->routine->getPointer(imageId);
		Pointer<Byte> binding = pointer.base;
		Pointer<Byte> imageBase = *Pointer<Pointer<Byte>>(binding + (useStencilAspect
				? OFFSET(vk::StorageImageDescriptor, stencilPtr)
				: OFFSET(vk::StorageImageDescriptor, ptr)));

		auto imageSizeInBytes = *Pointer<Int>(binding + OFFSET(vk::StorageImageDescriptor, sizeInBytes));

		auto &dst = state->routine->createIntermediate(resultId, resultType.sizeInComponents);

		auto texelSize = vk::Format(vkFormat).bytes();
		auto basePtr = SIMD::Pointer(imageBase, imageSizeInBytes);
		auto texelPtr = GetTexelAddress(state->routine, basePtr, coordinate, imageType, binding, texelSize, sampleId, useStencilAspect);

		SIMD::Int packed[4];
		// Round up texel size: for formats smaller than 32 bits per texel, we will emit a bunch
		// of (overlapping) 32b loads here, and each lane will pick out what it needs from the low bits.
		// TODO: specialize for small formats?
		for (auto i = 0; i < (texelSize + 3)/4; i++)
		{
			packed[i] = SIMD::Load<SIMD::Int>(texelPtr, state->activeLaneMask());
			texelPtr += sizeof(float);
		}

		// Format support requirements here come from two sources:
		// - Minimum required set of formats for loads from storage images
		// - Any format supported as a color or depth/stencil attachment, for input attachments
		switch(vkFormat)
		{
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_UINT:
			dst.move(0, packed[0]);
			dst.move(1, packed[1]);
			dst.move(2, packed[2]);
			dst.move(3, packed[3]);
			break;
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_UINT:
			dst.move(0, packed[0]);
			// Fill remaining channels with 0,0,1 (of the correct type)
			dst.move(1, SIMD::Int(0));
			dst.move(2, SIMD::Int(0));
			dst.move(3, SIMD::Int(1));
			break;
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			dst.move(0, packed[0]);
			// Fill remaining channels with 0,0,1 (of the correct type)
			dst.move(1, SIMD::Float(0));
			dst.move(2, SIMD::Float(0));
			dst.move(3, SIMD::Float(1));
			break;
		case VK_FORMAT_D16_UNORM:
			dst.move(0, SIMD::Float(packed[0] & SIMD::Int(0xffff)) * SIMD::Float(1.0f / 65535.0f));
			dst.move(1, SIMD::Float(0));
			dst.move(2, SIMD::Float(0));
			dst.move(3, SIMD::Float(1));
			break;
		case VK_FORMAT_R16G16B16A16_SINT:
			dst.move(0, (packed[0] << 16) >> 16);
			dst.move(1, (packed[0]) >> 16);
			dst.move(2, (packed[1] << 16) >> 16);
			dst.move(3, (packed[1]) >> 16);
			break;
		case VK_FORMAT_R16G16B16A16_UINT:
			dst.move(0, packed[0] & SIMD::Int(0xffff));
			dst.move(1, (packed[0] >> 16) & SIMD::Int(0xffff));
			dst.move(2, packed[1] & SIMD::Int(0xffff));
			dst.move(3, (packed[1] >> 16) & SIMD::Int(0xffff));
			break;
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			dst.move(0, halfToFloatBits(As<SIMD::UInt>(packed[0]) & SIMD::UInt(0x0000FFFF)));
			dst.move(1, halfToFloatBits((As<SIMD::UInt>(packed[0]) & SIMD::UInt(0xFFFF0000)) >> 16));
			dst.move(2, halfToFloatBits(As<SIMD::UInt>(packed[1]) & SIMD::UInt(0x0000FFFF)));
			dst.move(3, halfToFloatBits((As<SIMD::UInt>(packed[1]) & SIMD::UInt(0xFFFF0000)) >> 16));
			break;
		case VK_FORMAT_R8G8B8A8_SNORM:
			dst.move(0, Min(Max(SIMD::Float(((packed[0]<<24) & SIMD::Int(0xFF000000))) * SIMD::Float(1.0f / float(0x7f000000)), SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
			dst.move(1, Min(Max(SIMD::Float(((packed[0]<<16) & SIMD::Int(0xFF000000))) * SIMD::Float(1.0f / float(0x7f000000)), SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
			dst.move(2, Min(Max(SIMD::Float(((packed[0]<<8) & SIMD::Int(0xFF000000))) * SIMD::Float(1.0f / float(0x7f000000)), SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
			dst.move(3, Min(Max(SIMD::Float(((packed[0]) & SIMD::Int(0xFF000000))) * SIMD::Float(1.0f / float(0x7f000000)), SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
			break;
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
			dst.move(0, SIMD::Float((packed[0] & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(1, SIMD::Float(((packed[0]>>8) & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(2, SIMD::Float(((packed[0]>>16) & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(3, SIMD::Float(((packed[0]>>24) & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f));
			break;
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
			dst.move(0, ::sRGBtoLinear(SIMD::Float((packed[0] & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f)));
			dst.move(1, ::sRGBtoLinear(SIMD::Float(((packed[0]>>8) & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f)));
			dst.move(2, ::sRGBtoLinear(SIMD::Float(((packed[0]>>16) & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f)));
			dst.move(3, ::sRGBtoLinear(SIMD::Float(((packed[0]>>24) & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f)));
			break;
		case VK_FORMAT_B8G8R8A8_UNORM:
			dst.move(0, SIMD::Float(((packed[0]>>16) & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(1, SIMD::Float(((packed[0]>>8) & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(2, SIMD::Float((packed[0] & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(3, SIMD::Float(((packed[0]>>24) & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f));
			break;
		case VK_FORMAT_B8G8R8A8_SRGB:
			dst.move(0, ::sRGBtoLinear(SIMD::Float(((packed[0]>>16) & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f)));
			dst.move(1, ::sRGBtoLinear(SIMD::Float(((packed[0]>>8) & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f)));
			dst.move(2, ::sRGBtoLinear(SIMD::Float((packed[0] & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f)));
			dst.move(3, ::sRGBtoLinear(SIMD::Float(((packed[0]>>24) & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f)));
			break;
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_A8B8G8R8_UINT_PACK32:
			dst.move(0, (As<SIMD::UInt>(packed[0]) & SIMD::UInt(0xFF)));
			dst.move(1, ((As<SIMD::UInt>(packed[0])>>8) & SIMD::UInt(0xFF)));
			dst.move(2, ((As<SIMD::UInt>(packed[0])>>16) & SIMD::UInt(0xFF)));
			dst.move(3, ((As<SIMD::UInt>(packed[0])>>24) & SIMD::UInt(0xFF)));
			break;
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_A8B8G8R8_SINT_PACK32:
			dst.move(0, (packed[0] << 24) >> 24);
			dst.move(1, (packed[0] << 16) >> 24);
			dst.move(2, (packed[0] << 8) >> 24);
			dst.move(3, (packed[0]) >> 24);
			break;
		case VK_FORMAT_R8_UNORM:
			dst.move(0, SIMD::Float((packed[0] & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(1, SIMD::Float(0));
			dst.move(2, SIMD::Float(0));
			dst.move(3, SIMD::Float(1));
			break;
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_S8_UINT:
			dst.move(0, (As<SIMD::UInt>(packed[0]) & SIMD::UInt(0xFF)));
			dst.move(1, SIMD::UInt(0));
			dst.move(2, SIMD::UInt(0));
			dst.move(3, SIMD::UInt(1));
			break;
		case VK_FORMAT_R8_SINT:
			dst.move(0, (packed[0] << 24) >> 24);
			dst.move(1, SIMD::Int(0));
			dst.move(2, SIMD::Int(0));
			dst.move(3, SIMD::Int(1));
			break;
		case VK_FORMAT_R8G8_UNORM:
			dst.move(0, SIMD::Float((packed[0] & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(1, SIMD::Float(((packed[0]>>8) & SIMD::Int(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(2, SIMD::Float(0));
			dst.move(3, SIMD::Float(1));
			break;
		case VK_FORMAT_R8G8_UINT:
			dst.move(0, (As<SIMD::UInt>(packed[0]) & SIMD::UInt(0xFF)));
			dst.move(1, ((As<SIMD::UInt>(packed[0])>>8) & SIMD::UInt(0xFF)));
			dst.move(2, SIMD::UInt(0));
			dst.move(3, SIMD::UInt(1));
			break;
		case VK_FORMAT_R8G8_SINT:
			dst.move(0, (packed[0] << 24) >> 24);
			dst.move(1, (packed[0] << 16) >> 24);
			dst.move(2, SIMD::Int(0));
			dst.move(3, SIMD::Int(1));
			break;
		case VK_FORMAT_R16_SFLOAT:
			dst.move(0, halfToFloatBits(As<SIMD::UInt>(packed[0]) & SIMD::UInt(0x0000FFFF)));
			dst.move(1, SIMD::Float(0));
			dst.move(2, SIMD::Float(0));
			dst.move(3, SIMD::Float(1));
			break;
		case VK_FORMAT_R16_UINT:
			dst.move(0, packed[0] & SIMD::Int(0xffff));
			dst.move(1, SIMD::UInt(0));
			dst.move(2, SIMD::UInt(0));
			dst.move(3, SIMD::UInt(1));
			break;
		case VK_FORMAT_R16_SINT:
			dst.move(0, (packed[0] << 16) >> 16);
			dst.move(1, SIMD::Int(0));
			dst.move(2, SIMD::Int(0));
			dst.move(3, SIMD::Int(1));
			break;
		case VK_FORMAT_R16G16_SFLOAT:
			dst.move(0, halfToFloatBits(As<SIMD::UInt>(packed[0]) & SIMD::UInt(0x0000FFFF)));
			dst.move(1, halfToFloatBits((As<SIMD::UInt>(packed[0]) & SIMD::UInt(0xFFFF0000)) >> 16));
			dst.move(2, SIMD::Float(0));
			dst.move(3, SIMD::Float(1));
			break;
		case VK_FORMAT_R16G16_UINT:
			dst.move(0, packed[0] & SIMD::Int(0xffff));
			dst.move(1, (packed[0] >> 16) & SIMD::Int(0xffff));
			dst.move(2, SIMD::UInt(0));
			dst.move(3, SIMD::UInt(1));
			break;
		case VK_FORMAT_R16G16_SINT:
			dst.move(0, (packed[0] << 16) >> 16);
			dst.move(1, (packed[0]) >> 16);
			dst.move(2, SIMD::Int(0));
			dst.move(3, SIMD::Int(1));
			break;
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_UINT:
			dst.move(0, packed[0]);
			dst.move(1, packed[1]);
			dst.move(2, SIMD::Int(0));
			dst.move(3, SIMD::Int(1));
			break;
		case VK_FORMAT_R32G32_SFLOAT:
			dst.move(0, packed[0]);
			dst.move(1, packed[1]);
			dst.move(2, SIMD::Float(0));
			dst.move(3, SIMD::Float(1));
			break;
		case VK_FORMAT_A2B10G10R10_UINT_PACK32:
			dst.move(0, (packed[0]) & SIMD::Int(0x3FF));
			dst.move(1, (packed[0] >> 10) & SIMD::Int(0x3FF));
			dst.move(2, (packed[0] >> 20) & SIMD::Int(0x3FF));
			dst.move(3, (packed[0] >> 30) & SIMD::Int(0x3));
			break;
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
			dst.move(0, SIMD::Float((packed[0]) & SIMD::Int(0x3FF)) * SIMD::Float(1.0f / 0x3FF));
			dst.move(1, SIMD::Float((packed[0] >> 10) & SIMD::Int(0x3FF)) * SIMD::Float(1.0f / 0x3FF));
			dst.move(2, SIMD::Float((packed[0] >> 20) & SIMD::Int(0x3FF)) * SIMD::Float(1.0f / 0x3FF));
			dst.move(3, SIMD::Float((packed[0] >> 30) & SIMD::Int(0x3)) * SIMD::Float(1.0f / 0x3));
			break;
		case VK_FORMAT_R5G6B5_UNORM_PACK16:
			dst.move(0, SIMD::Float((packed[0] >> 11) & SIMD::Int(0x1F)) * SIMD::Float(1.0f / 0x1F));
			dst.move(1, SIMD::Float((packed[0] >> 5) & SIMD::Int(0x3F)) * SIMD::Float(1.0f / 0x3F));
			dst.move(2, SIMD::Float((packed[0]) & SIMD::Int(0x1F)) * SIMD::Float(1.0f / 0x1F));
			dst.move(3, SIMD::Float(1));
			break;
		default:
			UNIMPLEMENTED("spv::ImageFormat %d", int(vkFormat));
			break;
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitImageWrite(InsnIterator insn, EmitState *state) const
	{
		auto imageId = Object::ID(insn.word(1));
		auto &image = getObject(imageId);
		auto &imageType = getType(image.type);

		ASSERT(imageType.definition.opcode() == spv::OpTypeImage);

		// Not handling any image operands yet.
		ASSERT(insn.wordCount() == 4);

		auto coordinate = GenericValue(this, state->routine, insn.word(2));
		auto texel = GenericValue(this, state->routine, insn.word(3));

		Pointer<Byte> binding = state->routine->getPointer(imageId).base;
		Pointer<Byte> imageBase = *Pointer<Pointer<Byte>>(binding + OFFSET(vk::StorageImageDescriptor, ptr));
		auto imageSizeInBytes = *Pointer<Int>(binding + OFFSET(vk::StorageImageDescriptor, sizeInBytes));

		SIMD::Int packed[4];
		auto numPackedElements = 0u;
		int texelSize = 0;
		auto format = static_cast<spv::ImageFormat>(imageType.definition.word(8));
		switch (format)
		{
		case spv::ImageFormatRgba32f:
		case spv::ImageFormatRgba32i:
		case spv::ImageFormatRgba32ui:
			texelSize = 16;
			packed[0] = texel.Int(0);
			packed[1] = texel.Int(1);
			packed[2] = texel.Int(2);
			packed[3] = texel.Int(3);
			numPackedElements = 4;
			break;
		case spv::ImageFormatR32f:
		case spv::ImageFormatR32i:
		case spv::ImageFormatR32ui:
			texelSize = 4;
			packed[0] = texel.Int(0);
			numPackedElements = 1;
			break;
		case spv::ImageFormatRgba8:
			texelSize = 4;
			packed[0] = (SIMD::UInt(Round(Min(Max(texel.Float(0), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(255.0f)))) |
				((SIMD::UInt(Round(Min(Max(texel.Float(1), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(255.0f)))) << 8) |
				((SIMD::UInt(Round(Min(Max(texel.Float(2), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(255.0f)))) << 16) |
				((SIMD::UInt(Round(Min(Max(texel.Float(3), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(255.0f)))) << 24);
			numPackedElements = 1;
			break;
		case spv::ImageFormatRgba8Snorm:
			texelSize = 4;
			packed[0] = (SIMD::Int(Round(Min(Max(texel.Float(0), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(127.0f))) &
						 SIMD::Int(0xFF)) |
						((SIMD::Int(Round(Min(Max(texel.Float(1), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(127.0f))) &
						  SIMD::Int(0xFF)) << 8) |
						((SIMD::Int(Round(Min(Max(texel.Float(2), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(127.0f))) &
						  SIMD::Int(0xFF)) << 16) |
						((SIMD::Int(Round(Min(Max(texel.Float(3), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(127.0f))) &
						  SIMD::Int(0xFF)) << 24);
			numPackedElements = 1;
			break;
		case spv::ImageFormatRgba8i:
		case spv::ImageFormatRgba8ui:
			texelSize = 4;
			packed[0] = (SIMD::UInt(texel.UInt(0) & SIMD::UInt(0xff))) |
						(SIMD::UInt(texel.UInt(1) & SIMD::UInt(0xff)) << 8) |
						(SIMD::UInt(texel.UInt(2) & SIMD::UInt(0xff)) << 16) |
						(SIMD::UInt(texel.UInt(3) & SIMD::UInt(0xff)) << 24);
			numPackedElements = 1;
			break;
		case spv::ImageFormatRgba16f:
			texelSize = 8;
			packed[0] = FloatToHalfBits(texel.UInt(0), false) | FloatToHalfBits(texel.UInt(1), true);
			packed[1] = FloatToHalfBits(texel.UInt(2), false) | FloatToHalfBits(texel.UInt(3), true);
			numPackedElements = 2;
			break;
		case spv::ImageFormatRgba16i:
		case spv::ImageFormatRgba16ui:
			texelSize = 8;
			packed[0] = SIMD::UInt(texel.UInt(0) & SIMD::UInt(0xffff)) | (SIMD::UInt(texel.UInt(1) & SIMD::UInt(0xffff)) << 16);
			packed[1] = SIMD::UInt(texel.UInt(2) & SIMD::UInt(0xffff)) | (SIMD::UInt(texel.UInt(3) & SIMD::UInt(0xffff)) << 16);
			numPackedElements = 2;
			break;
		case spv::ImageFormatRg32f:
		case spv::ImageFormatRg16f:
		case spv::ImageFormatR11fG11fB10f:
		case spv::ImageFormatR16f:
		case spv::ImageFormatRgba16:
		case spv::ImageFormatRgb10A2:
		case spv::ImageFormatRg16:
		case spv::ImageFormatRg8:
		case spv::ImageFormatR16:
		case spv::ImageFormatR8:
		case spv::ImageFormatRgba16Snorm:
		case spv::ImageFormatRg16Snorm:
		case spv::ImageFormatRg8Snorm:
		case spv::ImageFormatR16Snorm:
		case spv::ImageFormatR8Snorm:
		case spv::ImageFormatRg32i:
		case spv::ImageFormatRg16i:
		case spv::ImageFormatRg8i:
		case spv::ImageFormatR16i:
		case spv::ImageFormatR8i:
		case spv::ImageFormatRgb10a2ui:
		case spv::ImageFormatRg32ui:
		case spv::ImageFormatRg16ui:
		case spv::ImageFormatRg8ui:
		case spv::ImageFormatR16ui:
		case spv::ImageFormatR8ui:
			UNIMPLEMENTED("spv::ImageFormat %d", int(format));
			break;

		default:
			UNREACHABLE("spv::ImageFormat %d", int(format));
			break;
		}

		auto basePtr = SIMD::Pointer(imageBase, imageSizeInBytes);
		auto texelPtr = GetTexelAddress(state->routine, basePtr, coordinate, imageType, binding, texelSize, 0, false);

		for (auto i = 0u; i < numPackedElements; i++)
		{
			SIMD::Store(texelPtr, packed[i], state->activeLaneMask());
			texelPtr += sizeof(float);
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitImageTexelPointer(InsnIterator insn, EmitState *state) const
	{
		auto &resultType = getType(Type::ID(insn.word(1)));
		auto imageId = Object::ID(insn.word(3));
		auto &image = getObject(imageId);
		// Note: OpImageTexelPointer is unusual in that the image is passed by pointer.
		// Look through to get the actual image type.
		auto &imageType = getType(getType(image.type).element);
		Object::ID resultId = insn.word(2);

		ASSERT(imageType.opcode() == spv::OpTypeImage);
		ASSERT(resultType.storageClass == spv::StorageClassImage);
		ASSERT(getType(resultType.element).opcode() == spv::OpTypeInt);

		auto coordinate = GenericValue(this, state->routine, insn.word(4));

		Pointer<Byte> binding = state->routine->getPointer(imageId).base;
		Pointer<Byte> imageBase = *Pointer<Pointer<Byte>>(binding + OFFSET(vk::StorageImageDescriptor, ptr));
		auto imageSizeInBytes = *Pointer<Int>(binding + OFFSET(vk::StorageImageDescriptor, sizeInBytes));

		auto basePtr = SIMD::Pointer(imageBase, imageSizeInBytes);
		auto ptr = GetTexelAddress(state->routine, basePtr, coordinate, imageType, binding, sizeof(uint32_t), 0, false);

		state->routine->createPointer(resultId, ptr);

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitSampledImageCombineOrSplit(InsnIterator insn, EmitState *state) const
	{
		// Propagate the image pointer in both cases.
		// Consumers of OpSampledImage will look through to find the sampler pointer.

		Object::ID resultId = insn.word(2);
		Object::ID imageId = insn.word(3);

		state->routine->createPointer(resultId, state->routine->getPointer(imageId));

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitAtomicOp(InsnIterator insn, EmitState *state) const
	{
		auto &resultType = getType(Type::ID(insn.word(1)));
		Object::ID resultId = insn.word(2);
		Object::ID semanticsId = insn.word(5);
		auto memorySemantics = static_cast<spv::MemorySemanticsMask>(getObject(semanticsId).constantValue[0]);
		auto memoryOrder = MemoryOrder(memorySemantics);
		// Where no value is provided (increment/decrement) use an implicit value of 1.
		auto value = (insn.wordCount() == 7) ? GenericValue(this, state->routine, insn.word(6)).UInt(0) : RValue<SIMD::UInt>(1);
		auto &dst = state->routine->createIntermediate(resultId, resultType.sizeInComponents);
		auto ptr = state->routine->getPointer(insn.word(3));
		auto ptrOffsets = ptr.offsets();

		SIMD::UInt x;
		for (int j = 0; j < SIMD::Width; j++)
		{
			If(Extract(state->activeLaneMask(), j) != 0)
			{
				auto offset = Extract(ptrOffsets, j);
				auto laneValue = Extract(value, j);
				UInt v;
				switch (insn.opcode())
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
					UNREACHABLE("%s", OpcodeName(insn.opcode()).c_str());
					break;
				}
				x = Insert(x, v, j);
			}
		}

		dst.move(0, x);
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

		auto value = GenericValue(this, state->routine, insn.word(7));
		auto comparator = GenericValue(this, state->routine, insn.word(8));
		auto &dst = state->routine->createIntermediate(resultId, resultType.sizeInComponents);
		auto ptr = state->routine->getPointer(insn.word(3));
		auto ptrOffsets = ptr.offsets();

		SIMD::UInt x;
		for (int j = 0; j < SIMD::Width; j++)
		{
			If(Extract(state->activeLaneMask(), j) != 0)
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
		auto ty = getType(insn.word(1));
		auto &dst = state->routine->createIntermediate(insn.word(2), ty.sizeInComponents);
		auto src = GenericValue(this, state->routine, insn.word(3));
		for (uint32_t i = 0; i < ty.sizeInComponents; i++)
		{
			dst.move(i, src.Int(i));
		}
		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitCopyMemory(InsnIterator insn, EmitState *state) const
	{
		Object::ID dstPtrId = insn.word(1);
		Object::ID srcPtrId = insn.word(2);
		auto &dstPtrTy = getType(getObject(dstPtrId).type);
		auto &srcPtrTy = getType(getObject(srcPtrId).type);
		ASSERT(dstPtrTy.element == srcPtrTy.element);

		bool dstInterleavedByLane = IsStorageInterleavedByLane(dstPtrTy.storageClass);
		bool srcInterleavedByLane = IsStorageInterleavedByLane(srcPtrTy.storageClass);
		auto dstPtr = GetPointerToData(dstPtrId, 0, state->routine);
		auto srcPtr = GetPointerToData(srcPtrId, 0, state->routine);

		std::unordered_map<uint32_t, uint32_t> srcOffsets;

		VisitMemoryObject(srcPtrId, [&](uint32_t i, uint32_t srcOffset) { srcOffsets[i] = srcOffset; });

		VisitMemoryObject(dstPtrId, [&](uint32_t i, uint32_t dstOffset)
		{
			auto it = srcOffsets.find(i);
			ASSERT(it != srcOffsets.end());
			auto srcOffset = it->second;

			auto dst = dstPtr + dstOffset;
			auto src = srcPtr + srcOffset;
			if (dstInterleavedByLane) { dst = interleaveByLane(dst); }
			if (srcInterleavedByLane) { src = interleaveByLane(src); }
			SIMD::Store(dst, SIMD::Load<SIMD::Float>(src, state->activeLaneMask()), state->activeLaneMask());
		});
		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitControlBarrier(InsnIterator insn, EmitState *state) const
	{
		auto executionScope = spv::Scope(GetConstScalarInt(insn.word(1)));
		auto semantics = spv::MemorySemanticsMask(GetConstScalarInt(insn.word(3)));
		// TODO: We probably want to consider the memory scope here. For now,
		// just always emit the full fence.
		Fence(semantics);

		switch (executionScope)
		{
		case spv::ScopeWorkgroup:
		case spv::ScopeSubgroup:
			Yield(YieldResult::ControlBarrier);
			break;
		default:
			// See Vulkan 1.1 spec, Appendix A, Validation Rules within a Module.
			UNREACHABLE("Scope for execution must be limited to Workgroup or Subgroup");
			break;
		}

		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitMemoryBarrier(InsnIterator insn, EmitState *state) const
	{
		auto semantics = spv::MemorySemanticsMask(GetConstScalarInt(insn.word(2)));
		// TODO: We probably want to consider the memory scope here. For now,
		// just always emit the full fence.
		Fence(semantics);
		return EmitResult::Continue;
	}

	void SpirvShader::Fence(spv::MemorySemanticsMask semantics) const
	{
		if (semantics == spv::MemorySemanticsMaskNone)
		{
			return; //no-op
		}
		rr::Fence(MemoryOrder(semantics));
	}

	SpirvShader::EmitResult SpirvShader::EmitGroupNonUniform(InsnIterator insn, EmitState *state) const
	{
		auto &type = getType(Type::ID(insn.word(1)));
		Object::ID resultId = insn.word(2);
		auto scope = spv::Scope(GetConstScalarInt(insn.word(3)));
		ASSERT_MSG(scope == spv::ScopeSubgroup, "Scope for Non Uniform Group Operations must be Subgroup for Vulkan 1.1");

		auto &dst = state->routine->createIntermediate(resultId, type.sizeInComponents);

		switch (insn.opcode())
		{
		case spv::OpGroupNonUniformElect:
		{
			// Result is true only in the active invocation with the lowest id
			// in the group, otherwise result is false.
			SIMD::Int active = state->activeLaneMask();
			// TODO: Would be nice if we could write this as:
			//   elect = active & ~(active.Oxyz | active.OOxy | active.OOOx)
			auto v0111 = SIMD::Int(0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
			auto elect = active & ~(v0111 & (active.xxyz | active.xxxy | active.xxxx));
			dst.move(0, elect);
			break;
		}
		default:
			UNIMPLEMENTED("EmitGroupNonUniform op: %s", OpcodeName(type.opcode()).c_str());
		}
		return EmitResult::Continue;
	}

	SpirvShader::EmitResult SpirvShader::EmitArrayLength(InsnIterator insn, EmitState *state) const
	{
		auto resultTyId = Type::ID(insn.word(1));
		auto resultId = Object::ID(insn.word(2));
		auto structPtrId = Object::ID(insn.word(3));
		auto arrayFieldIdx = insn.word(4);

		auto &resultType = getType(resultTyId);
		ASSERT(resultType.sizeInComponents == 1);
		ASSERT(resultType.definition.opcode() == spv::OpTypeInt);

		auto &structPtrTy = getType(getObject(structPtrId).type);
		auto &structTy = getType(structPtrTy.element);
		auto &arrayTy = getType(structTy.definition.word(2 + arrayFieldIdx));
		ASSERT(arrayTy.definition.opcode() == spv::OpTypeRuntimeArray);
		auto &arrayElTy = getType(arrayTy.element);

		auto &result = state->routine->createIntermediate(resultId, 1);
		auto structBase = GetPointerToData(structPtrId, 0, state->routine);

		Decorations d = {};
		ApplyDecorationsForIdMember(&d, structPtrTy.element, arrayFieldIdx);
		ASSERT(d.HasOffset);

		auto arrayBase = structBase + d.Offset;
		auto arraySizeInBytes = SIMD::Int(arrayBase.limit) - arrayBase.offsets();
		auto arrayLength = arraySizeInBytes / SIMD::Int(arrayElTy.sizeInComponents * sizeof(float));

		result.move(0, SIMD::Int(arrayLength));

		return EmitResult::Continue;
	}

	uint32_t SpirvShader::GetConstScalarInt(Object::ID id) const
	{
		auto &scopeObj = getObject(id);
		ASSERT(scopeObj.kind == Object::Kind::Constant);
		ASSERT(getType(scopeObj.type).sizeInComponents == 1);
		return scopeObj.constantValue[0];
	}

	void SpirvShader::EvalSpecConstantOp(InsnIterator insn)
	{
		auto opcode = static_cast<spv::Op>(insn.word(3));

		switch (opcode)
		{
		case spv::OpIAdd:
		case spv::OpISub:
		case spv::OpIMul:
		case spv::OpUDiv:
		case spv::OpSDiv:
		case spv::OpUMod:
		case spv::OpSMod:
		case spv::OpSRem:
		case spv::OpShiftRightLogical:
		case spv::OpShiftRightArithmetic:
		case spv::OpShiftLeftLogical:
		case spv::OpBitwiseOr:
		case spv::OpLogicalOr:
		case spv::OpBitwiseAnd:
		case spv::OpLogicalAnd:
		case spv::OpBitwiseXor:
		case spv::OpLogicalEqual:
		case spv::OpIEqual:
		case spv::OpLogicalNotEqual:
		case spv::OpINotEqual:
		case spv::OpULessThan:
		case spv::OpSLessThan:
		case spv::OpUGreaterThan:
		case spv::OpSGreaterThan:
		case spv::OpULessThanEqual:
		case spv::OpSLessThanEqual:
		case spv::OpUGreaterThanEqual:
		case spv::OpSGreaterThanEqual:
			EvalSpecConstantBinaryOp(insn);
			break;

		case spv::OpSConvert:
		case spv::OpFConvert:
		case spv::OpUConvert:
		case spv::OpSNegate:
		case spv::OpNot:
		case spv::OpLogicalNot:
		case spv::OpQuantizeToF16:
			EvalSpecConstantUnaryOp(insn);
			break;

		case spv::OpSelect:
		{
			auto &result = CreateConstant(insn);
			auto const &cond = getObject(insn.word(4));
			auto condIsScalar = (getType(cond.type).sizeInComponents == 1);
			auto const &left = getObject(insn.word(5));
			auto const &right = getObject(insn.word(6));

			for (auto i = 0u; i < getType(result.type).sizeInComponents; i++)
			{
				auto sel = cond.constantValue[condIsScalar ? 0 : i];
				result.constantValue[i] = sel ? left.constantValue[i] : right.constantValue[i];
			}
			break;
		}

		case spv::OpCompositeExtract:
		{
			auto &result = CreateConstant(insn);
			auto const &compositeObject = getObject(insn.word(4));
			auto firstComponent = WalkLiteralAccessChain(compositeObject.type, insn.wordCount() - 5, insn.wordPointer(5));

			for (auto i = 0u; i < getType(result.type).sizeInComponents; i++)
			{
				result.constantValue[i] = compositeObject.constantValue[firstComponent + i];
			}
			break;
		}

		case spv::OpCompositeInsert:
		{
			auto &result = CreateConstant(insn);
			auto const &newPart = getObject(insn.word(4));
			auto const &oldObject = getObject(insn.word(5));
			auto firstNewComponent = WalkLiteralAccessChain(result.type, insn.wordCount() - 6, insn.wordPointer(6));

			// old components before
			for (auto i = 0u; i < firstNewComponent; i++)
			{
				result.constantValue[i] = oldObject.constantValue[i];
			}
			// new part
			for (auto i = 0u; i < getType(newPart.type).sizeInComponents; i++)
			{
				result.constantValue[firstNewComponent + i] = newPart.constantValue[i];
			}
			// old components after
			for (auto i = firstNewComponent + getType(newPart.type).sizeInComponents; i < getType(result.type).sizeInComponents; i++)
			{
				result.constantValue[i] = oldObject.constantValue[i];
			}
			break;
		}

		case spv::OpVectorShuffle:
		{
			auto &result = CreateConstant(insn);
			auto const &firstHalf = getObject(insn.word(4));
			auto const &secondHalf = getObject(insn.word(5));

			for (auto i = 0u; i < getType(result.type).sizeInComponents; i++)
			{
				auto selector = insn.word(6 + i);
				if (selector == static_cast<uint32_t>(-1))
				{
					// Undefined value, we'll use zero
					result.constantValue[i] = 0;
				}
				else if (selector < getType(firstHalf.type).sizeInComponents)
				{
					result.constantValue[i] = firstHalf.constantValue[selector];
				}
				else
				{
					result.constantValue[i] = secondHalf.constantValue[selector - getType(firstHalf.type).sizeInComponents];
				}
			}
			break;
		}

		default:
			// Other spec constant ops are possible, but require capabilities that are
			// not exposed in our Vulkan implementation (eg Kernel), so we should never
			// get here for correct shaders.
			UNSUPPORTED("EvalSpecConstantOp op: %s", OpcodeName(opcode).c_str());
		}
	}

	void SpirvShader::EvalSpecConstantUnaryOp(InsnIterator insn)
	{
		auto &result = CreateConstant(insn);

		auto opcode = static_cast<spv::Op>(insn.word(3));
		auto const &lhs = getObject(insn.word(4));
		auto size = getType(lhs.type).sizeInComponents;

		for (auto i = 0u; i < size; i++)
		{
			auto &v = result.constantValue[i];
			auto l = lhs.constantValue[i];

			switch (opcode)
			{
			case spv::OpSConvert:
			case spv::OpFConvert:
			case spv::OpUConvert:
				UNREACHABLE("Not possible until we have multiple bit widths");
				break;

			case spv::OpSNegate:
				v = -(int)l;
				break;
			case spv::OpNot:
			case spv::OpLogicalNot:
				v = ~l;
				break;

			case spv::OpQuantizeToF16:
			{
				// Can do this nicer with host code, but want to perfectly mirror the reactor code we emit.
				auto abs = bit_cast<float>(l & 0x7FFFFFFF);
				auto sign = l & 0x80000000;
				auto isZero = abs < 0.000061035f ? ~0u : 0u;
				auto isInf = abs > 65504.0f ? ~0u : 0u;
				auto isNaN = (abs != abs) ? ~0u : 0u;
				auto isInfOrNan = isInf | isNaN;
				v = l & 0xFFFFE000;
				v &= ~isZero | 0x80000000;
				v = sign | (isInfOrNan & 0x7F800000) | (~isInfOrNan & v);
				v |= isNaN & 0x400000;
				break;
			}
			default:
				UNREACHABLE("EvalSpecConstantUnaryOp op: %s", OpcodeName(opcode).c_str());
			}
		}
	}

	void SpirvShader::EvalSpecConstantBinaryOp(InsnIterator insn)
	{
		auto &result = CreateConstant(insn);

		auto opcode = static_cast<spv::Op>(insn.word(3));
		auto const &lhs = getObject(insn.word(4));
		auto const &rhs = getObject(insn.word(5));
		auto size = getType(lhs.type).sizeInComponents;

		for (auto i = 0u; i < size; i++)
		{
			auto &v = result.constantValue[i];
			auto l = lhs.constantValue[i];
			auto r = rhs.constantValue[i];

			switch (opcode)
			{
			case spv::OpIAdd:
				v = l + r;
				break;
			case spv::OpISub:
				v = l - r;
				break;
			case spv::OpIMul:
				v = l * r;
				break;
			case spv::OpUDiv:
				v = (r == 0) ? 0 : l / r;
				break;
			case spv::OpUMod:
				v = (r == 0) ? 0 : l % r;
				break;
			case spv::OpSDiv:
				if (r == 0) r = UINT32_MAX;
				if (l == static_cast<uint32_t>(INT32_MIN)) l = UINT32_MAX;
				v = static_cast<int32_t>(l) / static_cast<int32_t>(r);
				break;
			case spv::OpSRem:
				if (r == 0) r = UINT32_MAX;
				if (l == static_cast<uint32_t>(INT32_MIN)) l = UINT32_MAX;
				v = static_cast<int32_t>(l) % static_cast<int32_t>(r);
				break;
			case spv::OpSMod:
				if (r == 0) r = UINT32_MAX;
				if (l == static_cast<uint32_t>(INT32_MIN)) l = UINT32_MAX;
				if (l * r < 0)
					v = static_cast<int32_t>(l) % static_cast<int32_t>(r) + r;
				else
					v = static_cast<int32_t>(l) % static_cast<int32_t>(r);
				break;
			case spv::OpShiftRightLogical:
				v = l >> r;
				break;
			case spv::OpShiftRightArithmetic:
				v = static_cast<int32_t>(l) >> r;
				break;
			case spv::OpShiftLeftLogical:
				v = l << r;
				break;
			case spv::OpBitwiseOr:
			case spv::OpLogicalOr:
				v = l | r;
				break;
			case spv::OpBitwiseAnd:
			case spv::OpLogicalAnd:
				v = l & r;
				break;
			case spv::OpBitwiseXor:
				v = l ^ r;
				break;
			case spv::OpLogicalEqual:
			case spv::OpIEqual:
				v = (l == r) ? ~0u : 0u;
				break;
			case spv::OpLogicalNotEqual:
			case spv::OpINotEqual:
				v = (l != r) ? ~0u : 0u;
				break;
			case spv::OpULessThan:
				v = l < r ? ~0u : 0u;
				break;
			case spv::OpSLessThan:
				v = static_cast<int32_t>(l) < static_cast<int32_t>(r) ? ~0u : 0u;
				break;
			case spv::OpUGreaterThan:
				v = l > r ? ~0u : 0u;
				break;
			case spv::OpSGreaterThan:
				v = static_cast<int32_t>(l) > static_cast<int32_t>(r) ? ~0u : 0u;
				break;
			case spv::OpULessThanEqual:
				v = l <= r ? ~0u : 0u;
				break;
			case spv::OpSLessThanEqual:
				v = static_cast<int32_t>(l) <= static_cast<int32_t>(r) ? ~0u : 0u;
				break;
			case spv::OpUGreaterThanEqual:
				v = l >= r ? ~0u : 0u;
				break;
			case spv::OpSGreaterThanEqual:
				v = static_cast<int32_t>(l) >= static_cast<int32_t>(r) ? ~0u : 0u;
				break;
			default:
				UNREACHABLE("EvalSpecConstantBinaryOp op: %s", OpcodeName(opcode).c_str());
			}
		}
	}

	void SpirvShader::emitEpilog(SpirvRoutine *routine) const
	{
		for (auto insn : *this)
		{
			switch (insn.opcode())
			{
			case spv::OpVariable:
			{
				Object::ID resultId = insn.word(2);
				auto &object = getObject(resultId);
				auto &objectTy = getType(object.type);
				if (object.kind == Object::Kind::InterfaceVariable && objectTy.storageClass == spv::StorageClassOutput)
				{
					auto &dst = routine->getVariable(resultId);
					int offset = 0;
					VisitInterface(resultId,
								   [&](Decorations const &d, AttribType type) {
									   auto scalarSlot = d.Location << 2 | d.Component;
									   routine->outputs[scalarSlot] = dst[offset++];
								   });
				}
				break;
			}
			default:
				break;
			}
		}
	}

	SpirvShader::Block::Block(InsnIterator begin, InsnIterator end) : begin_(begin), end_(end)
	{
		// Default to a Simple, this may change later.
		kind = Block::Simple;

		// Walk the instructions to find the last two of the block.
		InsnIterator insns[2];
		for (auto insn : *this)
		{
			insns[0] = insns[1];
			insns[1] = insn;
		}

		switch (insns[1].opcode())
		{
			case spv::OpBranch:
				branchInstruction = insns[1];
				outs.emplace(Block::ID(branchInstruction.word(1)));

				switch (insns[0].opcode())
				{
					case spv::OpLoopMerge:
						kind = Loop;
						mergeInstruction = insns[0];
						mergeBlock = Block::ID(mergeInstruction.word(1));
						continueTarget = Block::ID(mergeInstruction.word(2));
						break;

					default:
						kind = Block::Simple;
						break;
				}
				break;

			case spv::OpBranchConditional:
				branchInstruction = insns[1];
				outs.emplace(Block::ID(branchInstruction.word(2)));
				outs.emplace(Block::ID(branchInstruction.word(3)));

				switch (insns[0].opcode())
				{
					case spv::OpSelectionMerge:
						kind = StructuredBranchConditional;
						mergeInstruction = insns[0];
						mergeBlock = Block::ID(mergeInstruction.word(1));
						break;

					case spv::OpLoopMerge:
						kind = Loop;
						mergeInstruction = insns[0];
						mergeBlock = Block::ID(mergeInstruction.word(1));
						continueTarget = Block::ID(mergeInstruction.word(2));
						break;

					default:
						kind = UnstructuredBranchConditional;
						break;
				}
				break;

			case spv::OpSwitch:
				branchInstruction = insns[1];
				outs.emplace(Block::ID(branchInstruction.word(2)));
				for (uint32_t w = 4; w < branchInstruction.wordCount(); w += 2)
				{
					outs.emplace(Block::ID(branchInstruction.word(w)));
				}

				switch (insns[0].opcode())
				{
					case spv::OpSelectionMerge:
						kind = StructuredSwitch;
						mergeInstruction = insns[0];
						mergeBlock = Block::ID(mergeInstruction.word(1));
						break;

					default:
						kind = UnstructuredSwitch;
						break;
				}
				break;

			default:
				break;
		}
	}

	bool SpirvShader::existsPath(Block::ID from, Block::ID to, Block::ID notPassingThrough) const
	{
		// TODO: Optimize: This can be cached on the block.
		Block::Set seen;
		seen.emplace(notPassingThrough);

		std::queue<Block::ID> pending;
		pending.emplace(from);

		while (pending.size() > 0)
		{
			auto id = pending.front();
			pending.pop();
			for (auto out : getBlock(id).outs)
			{
				if (seen.count(out) != 0) { continue; }
				if (out == to) { return true; }
				pending.emplace(out);
			}
			seen.emplace(id);
		}

		return false;
	}

	void SpirvShader::EmitState::addOutputActiveLaneMaskEdge(Block::ID to, RValue<SIMD::Int> mask)
	{
		addActiveLaneMaskEdge(currentBlock, to, mask & activeLaneMask());
	}

	void SpirvShader::EmitState::addActiveLaneMaskEdge(Block::ID from, Block::ID to, RValue<SIMD::Int> mask)
	{
		auto edge = Block::Edge{from, to};
		auto it = edgeActiveLaneMasks.find(edge);
		if (it == edgeActiveLaneMasks.end())
		{
			edgeActiveLaneMasks.emplace(edge, mask);
		}
		else
		{
			auto combined = it->second | mask;
			edgeActiveLaneMasks.erase(edge);
			edgeActiveLaneMasks.emplace(edge, combined);
		}
	}

	RValue<SIMD::Int> SpirvShader::GetActiveLaneMaskEdge(EmitState *state, Block::ID from, Block::ID to) const
	{
		auto edge = Block::Edge{from, to};
		auto it = state->edgeActiveLaneMasks.find(edge);
		ASSERT_MSG(it != state->edgeActiveLaneMasks.end(), "Could not find edge %d -> %d", from.value(), to.value());
		return it->second;
	}

	VkShaderStageFlagBits SpirvShader::executionModelToStage(spv::ExecutionModel model)
	{
		switch (model)
		{
		case spv::ExecutionModelVertex:                 return VK_SHADER_STAGE_VERTEX_BIT;
		// case spv::ExecutionModelTessellationControl:    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		// case spv::ExecutionModelTessellationEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		// case spv::ExecutionModelGeometry:               return VK_SHADER_STAGE_GEOMETRY_BIT;
		case spv::ExecutionModelFragment:               return VK_SHADER_STAGE_FRAGMENT_BIT;
		case spv::ExecutionModelGLCompute:              return VK_SHADER_STAGE_COMPUTE_BIT;
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

	SpirvRoutine::SpirvRoutine(vk::PipelineLayout const *pipelineLayout) :
		pipelineLayout(pipelineLayout)
	{
	}

}
