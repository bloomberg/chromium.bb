// Copyright 2016 The SwiftShader Authors. All Rights Reserved.
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

#include "VertexProgram.hpp"

#include "SamplerCore.hpp"
#include "Device/Renderer.hpp"
#include "Device/Vertex.hpp"
#include "System/Debug.hpp"
#include "System/Half.hpp"

#include "Vulkan/VkPipelineLayout.hpp"

namespace sw {

VertexProgram::VertexProgram(
    const VertexProcessor::State &state,
    vk::PipelineLayout const *pipelineLayout,
    SpirvShader const *spirvShader,
    const vk::DescriptorSet::Bindings &descriptorSets)
    : VertexRoutine(state, pipelineLayout, spirvShader)
    , descriptorSets(descriptorSets)
{
	routine.setImmutableInputBuiltins(spirvShader);

	// TODO(b/146486064): Consider only assigning these to the SpirvRoutine iff
	// they are ever going to be read.
	routine.viewID = *Pointer<Int>(data + OFFSET(DrawData, viewID));
	routine.instanceID = *Pointer<Int>(data + OFFSET(DrawData, instanceID));

	routine.setInputBuiltin(spirvShader, spv::BuiltInViewIndex, [&](const SpirvShader::BuiltinMapping &builtin, Array<SIMD::Float> &value) {
		assert(builtin.SizeInComponents == 1);
		value[builtin.FirstComponent] = As<SIMD::Float>(SIMD::Int(routine.viewID));
	});

	routine.setInputBuiltin(spirvShader, spv::BuiltInInstanceIndex, [&](const SpirvShader::BuiltinMapping &builtin, Array<SIMD::Float> &value) {
		// TODO: we could do better here; we know InstanceIndex is uniform across all lanes
		assert(builtin.SizeInComponents == 1);
		value[builtin.FirstComponent] = As<SIMD::Float>(SIMD::Int(routine.instanceID));
	});

	routine.setInputBuiltin(spirvShader, spv::BuiltInSubgroupSize, [&](const SpirvShader::BuiltinMapping &builtin, Array<SIMD::Float> &value) {
		ASSERT(builtin.SizeInComponents == 1);
		value[builtin.FirstComponent] = As<SIMD::Float>(SIMD::Int(SIMD::Width));
	});

	routine.descriptorSets = data + OFFSET(DrawData, descriptorSets);
	routine.descriptorDynamicOffsets = data + OFFSET(DrawData, descriptorDynamicOffsets);
	routine.pushConstants = data + OFFSET(DrawData, pushConstants);
	routine.constants = *Pointer<Pointer<Byte>>(data + OFFSET(DrawData, constants));
}

VertexProgram::~VertexProgram()
{
}

void VertexProgram::program(Pointer<UInt> &batch, UInt &vertexCount)
{
	routine.vertexIndex = *Pointer<SIMD::Int>(As<Pointer<SIMD::Int>>(batch)) +
	                      SIMD::Int(*Pointer<Int>(data + OFFSET(DrawData, baseVertex)));

	auto it = spirvShader->inputBuiltins.find(spv::BuiltInVertexIndex);
	if(it != spirvShader->inputBuiltins.end())
	{
		assert(it->second.SizeInComponents == 1);
		routine.getVariable(it->second.Id)[it->second.FirstComponent] =
		    As<SIMD::Float>(routine.vertexIndex);
	}

	auto activeLaneMask = SIMD::Int(0xFFFFFFFF);
	Int4 storesAndAtomicsMask = CmpGE(UInt4(vertexCount), UInt4(1, 2, 3, 4));
	spirvShader->emit(&routine, activeLaneMask, storesAndAtomicsMask, descriptorSets);

	spirvShader->emitEpilog(&routine);
}

}  // namespace sw
