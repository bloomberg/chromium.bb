// Copyright 2019 The SwiftShader Authors. All Rights Reserved.
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
#include "SpirvShaderDebug.hpp"

#include "ShaderCore.hpp"
#include "Reactor/Assert.hpp"
#include "Vulkan/VkPipelineLayout.hpp"

#include <spirv/unified1/spirv.hpp>

namespace sw {

SpirvShader::EmitResult SpirvShader::EmitLoad(InsnIterator insn, EmitState *state) const
{
	bool atomic = (insn.opcode() == spv::OpAtomicLoad);
	Object::ID resultId = insn.word(2);
	Object::ID pointerId = insn.word(3);
	auto &result = getObject(resultId);
	auto &resultTy = getType(result);
	auto &pointer = getObject(pointerId);
	auto &pointerTy = getType(pointer);
	std::memory_order memoryOrder = std::memory_order_relaxed;

	ASSERT(getType(pointer).element == result.typeId());
	ASSERT(Type::ID(insn.word(1)) == result.typeId());
	ASSERT(!atomic || getType(getType(pointer).element).opcode() == spv::OpTypeInt);  // Vulkan 1.1: "Atomic instructions must declare a scalar 32-bit integer type, for the value pointed to by Pointer."

	if(pointerTy.storageClass == spv::StorageClassUniformConstant)
	{
		// Just propagate the pointer.
		auto &ptr = state->getPointer(pointerId);
		state->createPointer(resultId, ptr);
		return EmitResult::Continue;
	}

	if(atomic)
	{
		Object::ID semanticsId = insn.word(5);
		auto memorySemantics = static_cast<spv::MemorySemanticsMask>(getObject(semanticsId).constantValue[0]);
		memoryOrder = MemoryOrder(memorySemantics);
	}

	auto ptr = GetPointerToData(pointerId, 0, state);
	bool interleavedByLane = IsStorageInterleavedByLane(pointerTy.storageClass);
	auto &dst = state->createIntermediate(resultId, resultTy.componentCount);
	auto robustness = getOutOfBoundsBehavior(pointerId, state);

	VisitMemoryObject(pointerId, [&](const MemoryElement &el) {
		auto p = ptr + el.offset;
		if(interleavedByLane) { p = InterleaveByLane(p); }  // TODO: Interleave once, then add offset?
		dst.move(el.index, p.Load<SIMD::Float>(robustness, state->activeLaneMask(), atomic, memoryOrder));
	});

	SPIRV_SHADER_DBG("Load(atomic: {0}, order: {1}, ptr: {2}, val: {3}, mask: {4})", atomic, int(memoryOrder), ptr, dst, state->activeLaneMask());

	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitStore(InsnIterator insn, EmitState *state) const
{
	bool atomic = (insn.opcode() == spv::OpAtomicStore);
	Object::ID pointerId = insn.word(1);
	Object::ID objectId = insn.word(atomic ? 4 : 2);
	std::memory_order memoryOrder = std::memory_order_relaxed;

	if(atomic)
	{
		Object::ID semanticsId = insn.word(3);
		auto memorySemantics = static_cast<spv::MemorySemanticsMask>(getObject(semanticsId).constantValue[0]);
		memoryOrder = MemoryOrder(memorySemantics);
	}

	const auto &value = Operand(this, state, objectId);

	Store(pointerId, value, atomic, memoryOrder, state);

	return EmitResult::Continue;
}

void SpirvShader::Store(Object::ID pointerId, const Operand &value, bool atomic, std::memory_order memoryOrder, EmitState *state) const
{
	auto &pointer = getObject(pointerId);
	auto &pointerTy = getType(pointer);
	auto &elementTy = getType(pointerTy.element);

	ASSERT(!atomic || elementTy.opcode() == spv::OpTypeInt);  // Vulkan 1.1: "Atomic instructions must declare a scalar 32-bit integer type, for the value pointed to by Pointer."

	auto ptr = GetPointerToData(pointerId, 0, state);
	bool interleavedByLane = IsStorageInterleavedByLane(pointerTy.storageClass);
	auto robustness = getOutOfBoundsBehavior(pointerId, state);

	SIMD::Int mask = state->activeLaneMask();
	if(!StoresInHelperInvocation(pointerTy.storageClass))
	{
		mask = mask & state->storesAndAtomicsMask();
	}

	SPIRV_SHADER_DBG("Store(atomic: {0}, order: {1}, ptr: {2}, val: {3}, mask: {4}", atomic, int(memoryOrder), ptr, value, mask);

	VisitMemoryObject(pointerId, [&](const MemoryElement &el) {
		auto p = ptr + el.offset;
		if(interleavedByLane) { p = InterleaveByLane(p); }
		p.Store(value.Float(el.index), robustness, mask, atomic, memoryOrder);
	});
}

SpirvShader::EmitResult SpirvShader::EmitVariable(InsnIterator insn, EmitState *state) const
{
	auto routine = state->routine;
	Object::ID resultId = insn.word(2);
	auto &object = getObject(resultId);
	auto &objectTy = getType(object);

	switch(objectTy.storageClass)
	{
	case spv::StorageClassOutput:
	case spv::StorageClassPrivate:
	case spv::StorageClassFunction:
		{
			ASSERT(objectTy.opcode() == spv::OpTypePointer);
			auto base = &routine->getVariable(resultId)[0];
			auto elementTy = getType(objectTy.element);
			auto size = elementTy.componentCount * static_cast<uint32_t>(sizeof(float)) * SIMD::Width;
			state->createPointer(resultId, SIMD::Pointer(base, size));
		}
		break;
	case spv::StorageClassWorkgroup:
		{
			ASSERT(objectTy.opcode() == spv::OpTypePointer);
			auto base = &routine->workgroupMemory[0];
			auto size = workgroupMemory.size();
			state->createPointer(resultId, SIMD::Pointer(base, size, workgroupMemory.offsetOf(resultId)));
		}
		break;
	case spv::StorageClassInput:
		{
			if(object.kind == Object::Kind::InterfaceVariable)
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
			auto size = elementTy.componentCount * static_cast<uint32_t>(sizeof(float)) * SIMD::Width;
			state->createPointer(resultId, SIMD::Pointer(base, size));
		}
		break;
	case spv::StorageClassUniformConstant:
		{
			const auto &d = descriptorDecorations.at(resultId);
			ASSERT(d.DescriptorSet >= 0);
			ASSERT(d.Binding >= 0);

			uint32_t bindingOffset = routine->pipelineLayout->getBindingOffset(d.DescriptorSet, d.Binding);
			Pointer<Byte> set = routine->descriptorSets[d.DescriptorSet];  // DescriptorSet*
			Pointer<Byte> binding = Pointer<Byte>(set + bindingOffset);    // vk::SampledImageDescriptor*
			auto size = 0;                                                 // Not required as this pointer is not directly used by SIMD::Read or SIMD::Write.
			state->createPointer(resultId, SIMD::Pointer(binding, size));
		}
		break;
	case spv::StorageClassUniform:
	case spv::StorageClassStorageBuffer:
		{
			const auto &d = descriptorDecorations.at(resultId);
			ASSERT(d.DescriptorSet >= 0);
			auto size = 0;  // Not required as this pointer is not directly used by SIMD::Read or SIMD::Write.
			// Note: the module may contain descriptor set references that are not suitable for this implementation -- using a set index higher than the number
			// of descriptor set binding points we support. As long as the selected entrypoint doesn't actually touch the out of range binding points, this
			// is valid. In this case make the value nullptr to make it easier to diagnose an attempt to dereference it.
			if(static_cast<uint32_t>(d.DescriptorSet) < vk::MAX_BOUND_DESCRIPTOR_SETS)
			{
				state->createPointer(resultId, SIMD::Pointer(routine->descriptorSets[d.DescriptorSet], size));
			}
			else
			{
				state->createPointer(resultId, SIMD::Pointer(nullptr, 0));
			}
		}
		break;
	case spv::StorageClassPushConstant:
		{
			state->createPointer(resultId, SIMD::Pointer(routine->pushConstants, vk::MAX_PUSH_CONSTANT_SIZE));
		}
		break;
	default:
		UNREACHABLE("Storage class %d", objectTy.storageClass);
		break;
	}

	if(insn.wordCount() > 4)
	{
		Object::ID initializerId = insn.word(4);
		if(getObject(initializerId).kind != Object::Kind::Constant)
		{
			UNIMPLEMENTED("b/148241854: Non-constant initializers not yet implemented");  // FIXME(b/148241854)
		}

		switch(objectTy.storageClass)
		{
		case spv::StorageClassOutput:
		case spv::StorageClassPrivate:
		case spv::StorageClassFunction:
		case spv::StorageClassWorkgroup:
			{
				bool interleavedByLane = IsStorageInterleavedByLane(objectTy.storageClass);
				auto ptr = GetPointerToData(resultId, 0, state);
				Operand initialValue(this, state, initializerId);
				VisitMemoryObject(resultId, [&](const MemoryElement &el) {
					auto p = ptr + el.offset;
					if(interleavedByLane) { p = InterleaveByLane(p); }
					auto robustness = OutOfBoundsBehavior::UndefinedBehavior;  // Local variables are always within bounds.
					p.Store(initialValue.Float(el.index), robustness, state->activeLaneMask());
				});
				if(objectTy.storageClass == spv::StorageClassWorkgroup)
				{
					// Initialization of workgroup memory is done by each subgroup and requires waiting on a barrier.
					// TODO(b/221242292): Initialize just once per workgroup and eliminate the barrier.
					Yield(YieldResult::ControlBarrier);
				}
			}
			break;
		default:
			ASSERT_MSG(initializerId == 0, "Vulkan does not permit variables of storage class %d to have initializers", int(objectTy.storageClass));
		}
	}

	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitCopyMemory(InsnIterator insn, EmitState *state) const
{
	Object::ID dstPtrId = insn.word(1);
	Object::ID srcPtrId = insn.word(2);
	auto &dstPtrTy = getObjectType(dstPtrId);
	auto &srcPtrTy = getObjectType(srcPtrId);
	ASSERT(dstPtrTy.element == srcPtrTy.element);

	bool dstInterleavedByLane = IsStorageInterleavedByLane(dstPtrTy.storageClass);
	bool srcInterleavedByLane = IsStorageInterleavedByLane(srcPtrTy.storageClass);
	auto dstPtr = GetPointerToData(dstPtrId, 0, state);
	auto srcPtr = GetPointerToData(srcPtrId, 0, state);

	std::unordered_map<uint32_t, uint32_t> srcOffsets;

	VisitMemoryObject(srcPtrId, [&](const MemoryElement &el) { srcOffsets[el.index] = el.offset; });

	VisitMemoryObject(dstPtrId, [&](const MemoryElement &el) {
		auto it = srcOffsets.find(el.index);
		ASSERT(it != srcOffsets.end());
		auto srcOffset = it->second;
		auto dstOffset = el.offset;

		auto dst = dstPtr + dstOffset;
		auto src = srcPtr + srcOffset;
		if(dstInterleavedByLane) { dst = InterleaveByLane(dst); }
		if(srcInterleavedByLane) { src = InterleaveByLane(src); }

		// TODO(b/131224163): Optimize based on src/dst storage classes.
		auto robustness = OutOfBoundsBehavior::RobustBufferAccess;

		auto value = src.Load<SIMD::Float>(robustness, state->activeLaneMask());
		dst.Store(value, robustness, state->activeLaneMask());
	});
	return EmitResult::Continue;
}

SpirvShader::EmitResult SpirvShader::EmitMemoryBarrier(InsnIterator insn, EmitState *state) const
{
	auto semantics = spv::MemorySemanticsMask(GetConstScalarInt(insn.word(2)));
	// TODO(b/176819536): We probably want to consider the memory scope here.
	// For now, just always emit the full fence.
	Fence(semantics);
	return EmitResult::Continue;
}

void SpirvShader::VisitMemoryObjectInner(sw::SpirvShader::Type::ID id, sw::SpirvShader::Decorations d, uint32_t &index, uint32_t offset, const MemoryVisitor &f) const
{
	ApplyDecorationsForId(&d, id);
	auto const &type = getType(id);

	if(d.HasOffset)
	{
		offset += d.Offset;
		d.HasOffset = false;
	}

	switch(type.opcode())
	{
	case spv::OpTypePointer:
		VisitMemoryObjectInner(type.definition.word(3), d, index, offset, f);
		break;
	case spv::OpTypeInt:
	case spv::OpTypeFloat:
	case spv::OpTypeRuntimeArray:
		f(MemoryElement{ index++, offset, type });
		break;
	case spv::OpTypeVector:
		{
			auto elemStride = (d.InsideMatrix && d.HasRowMajor && d.RowMajor) ? d.MatrixStride : static_cast<int32_t>(sizeof(float));
			for(auto i = 0u; i < type.definition.word(3); i++)
			{
				VisitMemoryObjectInner(type.definition.word(2), d, index, offset + elemStride * i, f);
			}
		}
		break;
	case spv::OpTypeMatrix:
		{
			auto columnStride = (d.HasRowMajor && d.RowMajor) ? static_cast<int32_t>(sizeof(float)) : d.MatrixStride;
			d.InsideMatrix = true;
			for(auto i = 0u; i < type.definition.word(3); i++)
			{
				ASSERT(d.HasMatrixStride);
				VisitMemoryObjectInner(type.definition.word(2), d, index, offset + columnStride * i, f);
			}
		}
		break;
	case spv::OpTypeStruct:
		for(auto i = 0u; i < type.definition.wordCount() - 2; i++)
		{
			ApplyDecorationsForIdMember(&d, id, i);
			VisitMemoryObjectInner(type.definition.word(i + 2), d, index, offset, f);
		}
		break;
	case spv::OpTypeArray:
		{
			auto arraySize = GetConstScalarInt(type.definition.word(3));
			for(auto i = 0u; i < arraySize; i++)
			{
				ASSERT(d.HasArrayStride);
				VisitMemoryObjectInner(type.definition.word(2), d, index, offset + i * d.ArrayStride, f);
			}
		}
		break;
	default:
		UNREACHABLE("%s", OpcodeName(type.opcode()));
	}
}

void SpirvShader::VisitMemoryObject(Object::ID id, const MemoryVisitor &f) const
{
	auto typeId = getObject(id).typeId();
	auto const &type = getType(typeId);

	if(IsExplicitLayout(type.storageClass))
	{
		Decorations d = GetDecorationsForId(id);
		uint32_t index = 0;
		VisitMemoryObjectInner(typeId, d, index, 0, f);
	}
	else
	{
		// Objects without explicit layout are tightly packed.
		auto &elType = getType(type.element);
		for(auto index = 0u; index < elType.componentCount; index++)
		{
			auto offset = static_cast<uint32_t>(index * sizeof(float));
			f({ index, offset, elType });
		}
	}
}

SIMD::Pointer SpirvShader::GetPointerToData(Object::ID id, Int arrayIndex, EmitState const *state) const
{
	auto routine = state->routine;
	auto &object = getObject(id);
	switch(object.kind)
	{
	case Object::Kind::Pointer:
	case Object::Kind::InterfaceVariable:
		return state->getPointer(id);

	case Object::Kind::DescriptorSet:
		{
			const auto &d = descriptorDecorations.at(id);
			ASSERT(d.DescriptorSet >= 0 && static_cast<uint32_t>(d.DescriptorSet) < vk::MAX_BOUND_DESCRIPTOR_SETS);
			ASSERT(d.Binding >= 0);
			ASSERT(routine->pipelineLayout->getDescriptorCount(d.DescriptorSet, d.Binding) != 0);  // "If descriptorCount is zero this binding entry is reserved and the resource must not be accessed from any stage via this binding within any pipeline using the set layout."

			uint32_t bindingOffset = routine->pipelineLayout->getBindingOffset(d.DescriptorSet, d.Binding);
			uint32_t descriptorSize = routine->pipelineLayout->getDescriptorSize(d.DescriptorSet, d.Binding);
			Int descriptorOffset = bindingOffset + descriptorSize * arrayIndex;

			auto set = state->getPointer(id);
			Assert(set.base != Pointer<Byte>(nullptr));
			Pointer<Byte> descriptor = set.base + descriptorOffset;  // BufferDescriptor* or inline uniform block

			auto descriptorType = routine->pipelineLayout->getDescriptorType(d.DescriptorSet, d.Binding);
			if(descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
			{
				// Note: there is no bounds checking for inline uniform blocks.
				// MAX_INLINE_UNIFORM_BLOCK_SIZE represents the maximum size of
				// an inline uniform block, but this value should remain unused.
				return SIMD::Pointer(descriptor, vk::MAX_INLINE_UNIFORM_BLOCK_SIZE);
			}
			else
			{
				Pointer<Byte> data = *Pointer<Pointer<Byte>>(descriptor + OFFSET(vk::BufferDescriptor, ptr));  // void*
				Int size = *Pointer<Int>(descriptor + OFFSET(vk::BufferDescriptor, sizeInBytes));

				if(routine->pipelineLayout->isDescriptorDynamic(d.DescriptorSet, d.Binding))
				{
					Int dynamicOffsetIndex =
					    routine->pipelineLayout->getDynamicOffsetIndex(d.DescriptorSet, d.Binding) +
					    arrayIndex;
					Int offset = routine->descriptorDynamicOffsets[dynamicOffsetIndex];
					Int robustnessSize = *Pointer<Int>(descriptor + OFFSET(vk::BufferDescriptor, robustnessSize));

					return SIMD::Pointer(data + offset, Min(size, robustnessSize - offset));
				}
				else
				{
					return SIMD::Pointer(data, size);
				}
			}
		}

	default:
		UNREACHABLE("Invalid pointer kind %d", int(object.kind));
		return SIMD::Pointer(Pointer<Byte>(), 0);
	}
}

void SpirvShader::Fence(spv::MemorySemanticsMask semantics) const
{
	if(semantics != spv::MemorySemanticsMaskNone)
	{
		rr::Fence(MemoryOrder(semantics));
	}
}

std::memory_order SpirvShader::MemoryOrder(spv::MemorySemanticsMask memorySemantics)
{
	uint32_t control = static_cast<uint32_t>(memorySemantics) & static_cast<uint32_t>(
	                                                                spv::MemorySemanticsAcquireMask |
	                                                                spv::MemorySemanticsReleaseMask |
	                                                                spv::MemorySemanticsAcquireReleaseMask |
	                                                                spv::MemorySemanticsSequentiallyConsistentMask);
	switch(control)
	{
	case spv::MemorySemanticsMaskNone: return std::memory_order_relaxed;
	case spv::MemorySemanticsAcquireMask: return std::memory_order_acquire;
	case spv::MemorySemanticsReleaseMask: return std::memory_order_release;
	case spv::MemorySemanticsAcquireReleaseMask: return std::memory_order_acq_rel;
	case spv::MemorySemanticsSequentiallyConsistentMask: return std::memory_order_acq_rel;  // Vulkan 1.1: "SequentiallyConsistent is treated as AcquireRelease"
	default:
		// "it is invalid for more than one of these four bits to be set:
		//  Acquire, Release, AcquireRelease, or SequentiallyConsistent."
		UNREACHABLE("MemorySemanticsMask: %x", int(control));
		return std::memory_order_acq_rel;
	}
}

bool SpirvShader::StoresInHelperInvocation(spv::StorageClass storageClass)
{
	switch(storageClass)
	{
	case spv::StorageClassUniform:
	case spv::StorageClassStorageBuffer:
	case spv::StorageClassImage:
		return false;
	default:
		return true;
	}
}

bool SpirvShader::IsExplicitLayout(spv::StorageClass storageClass)
{
	switch(storageClass)
	{
	case spv::StorageClassUniform:
	case spv::StorageClassStorageBuffer:
	case spv::StorageClassPushConstant:
		return true;
	default:
		return false;
	}
}

sw::SIMD::Pointer SpirvShader::InterleaveByLane(sw::SIMD::Pointer p)
{
	p *= sw::SIMD::Width;
	p.staticOffsets[0] += 0 * sizeof(float);
	p.staticOffsets[1] += 1 * sizeof(float);
	p.staticOffsets[2] += 2 * sizeof(float);
	p.staticOffsets[3] += 3 * sizeof(float);
	return p;
}

bool SpirvShader::IsStorageInterleavedByLane(spv::StorageClass storageClass)
{
	switch(storageClass)
	{
	case spv::StorageClassUniform:
	case spv::StorageClassStorageBuffer:
	case spv::StorageClassPushConstant:
	case spv::StorageClassWorkgroup:
	case spv::StorageClassImage:
		return false;
	default:
		return true;
	}
}

}  // namespace sw