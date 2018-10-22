/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Compute Shader Based Test Case Utility Structs/Functions
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmComputeShaderTestUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{
namespace
{
bool verifyOutputWithEpsilon (const std::vector<AllocationSp>& outputAllocs, const std::vector<BufferSp>& expectedOutputs, tcu::TestLog& log, const float epsilon)
{
	DE_ASSERT(outputAllocs.size() != 0);
	DE_ASSERT(outputAllocs.size() == expectedOutputs.size());

	for (size_t outputNdx = 0; outputNdx < outputAllocs.size(); ++outputNdx)
	{
		std::vector<deUint8>	expectedBytes;
		expectedOutputs[outputNdx]->getBytes(expectedBytes);

		std::vector<float>	expectedFloats	(expectedBytes.size() / sizeof (float));
		std::vector<float>	actualFloats	(expectedBytes.size() / sizeof (float));

		memcpy(&expectedFloats[0], &expectedBytes.front(), expectedBytes.size());
		memcpy(&actualFloats[0], outputAllocs[outputNdx]->getHostPtr(), expectedBytes.size());
		for (size_t floatNdx = 0; floatNdx < actualFloats.size(); ++floatNdx)
		{
			// Use custom epsilon because of the float->string conversion
			if (fabs(expectedFloats[floatNdx] - actualFloats[floatNdx]) > epsilon)
			{
				log << tcu::TestLog::Message << "Error: The actual and expected values not matching."
					<< " Expected: " << expectedFloats[floatNdx] << " Actual: " << actualFloats[floatNdx] << " Epsilon: " << epsilon << tcu::TestLog::EndMessage;
				return false;
			}
		}
	}
	return true;
}
}

const char* getComputeAsmShaderPreamble (void)
{
	return
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n";
}

const char* getComputeAsmShaderPreambleWithoutLocalSize (void)
{
	return
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n";
}

std::string getComputeAsmCommonTypes (std::string blockStorageClass)
{
	return std::string(
		"%bool      = OpTypeBool\n"
		"%void      = OpTypeVoid\n"
		"%voidf     = OpTypeFunction %void\n"
		"%u32       = OpTypeInt 32 0\n"
		"%i32       = OpTypeInt 32 1\n"
		"%f32       = OpTypeFloat 32\n"
		"%uvec3     = OpTypeVector %u32 3\n"
		"%fvec3     = OpTypeVector %f32 3\n"
		"%uvec3ptr  = OpTypePointer Input %uvec3\n") +
		"%i32ptr    = OpTypePointer " + blockStorageClass + " %i32\n"
		"%f32ptr    = OpTypePointer " + blockStorageClass + " %f32\n"
		"%i32arr    = OpTypeRuntimeArray %i32\n"
		"%f32arr    = OpTypeRuntimeArray %f32\n";
}

const char* getComputeAsmCommonInt64Types (void)
{
	return
		"%i64       = OpTypeInt 64 1\n"
		"%i64ptr    = OpTypePointer Uniform %i64\n"
		"%i64arr    = OpTypeRuntimeArray %i64\n";
}

const char* getComputeAsmInputOutputBuffer (void)
{
	return
		"%buf     = OpTypeStruct %f32arr\n"
		"%bufptr  = OpTypePointer Uniform %buf\n"
		"%indata    = OpVariable %bufptr Uniform\n"
		"%outdata   = OpVariable %bufptr Uniform\n";
}

const char* getComputeAsmInputOutputBufferTraits (void)
{
	return
		"OpDecorate %buf BufferBlock\n"
		"OpDecorate %indata DescriptorSet 0\n"
		"OpDecorate %indata Binding 0\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 1\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpMemberDecorate %buf 0 Offset 0\n";
}

bool verifyOutput (const std::vector<BufferSp>&, const std::vector<AllocationSp>& outputAllocs, const std::vector<BufferSp>& expectedOutputs, tcu::TestLog& log)
{
	const float	epsilon	= 0.001f;
	return verifyOutputWithEpsilon(outputAllocs, expectedOutputs, log, epsilon);
}

} // SpirVAssembly
} // vkt
