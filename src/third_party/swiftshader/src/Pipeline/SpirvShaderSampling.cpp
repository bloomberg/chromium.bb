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

#include "SamplerCore.hpp" // TODO: Figure out what's needed.
#include "System/Math.hpp"
#include "Vulkan/VkBuffer.hpp"
#include "Vulkan/VkDebug.hpp"
#include "Vulkan/VkDescriptorSet.hpp"
#include "Vulkan/VkPipelineLayout.hpp"
#include "Vulkan/VkImageView.hpp"
#include "Vulkan/VkSampler.hpp"
#include "Vulkan/VkDescriptorSetLayout.hpp"
#include "Device/Config.hpp"

#include <spirv/unified1/spirv.hpp>
#include <spirv/unified1/GLSL.std.450.h>

#include <climits>
#include <mutex>

namespace
{

struct SamplingRoutineKey
{
	uint32_t instruction;
	uint32_t sampler;
	uint32_t imageView;

	bool operator==(const SamplingRoutineKey &rhs) const
	{
		return instruction == rhs.instruction && sampler == rhs.sampler && imageView == rhs.imageView;
	}

	struct Hash
	{
		std::size_t operator()(const SamplingRoutineKey &key) const noexcept
		{
			return (key.instruction << 16) ^ (key.sampler << 8) ^ key.imageView;
		}
	};
};

}

namespace sw {

SpirvShader::ImageSampler *SpirvShader::getImageSampler(uint32_t inst, vk::SampledImageDescriptor const *imageDescriptor, const vk::Sampler *sampler)
{
	ImageInstruction instruction(inst);
	ASSERT(imageDescriptor->imageViewId != 0 && (sampler->id != 0 || instruction.samplerMethod == Fetch));

	// TODO(b/129523279): Move somewhere sensible.
	static std::unordered_map<SamplingRoutineKey, ImageSampler*, SamplingRoutineKey::Hash> cache;
	static std::mutex mutex;

	SamplingRoutineKey key = {inst, imageDescriptor->imageViewId, sampler->id};

	std::unique_lock<std::mutex> lock(mutex);
	auto it = cache.find(key);
	if (it != cache.end()) { return it->second; }

	auto type = imageDescriptor->type;

	Sampler samplerState = {};
	samplerState.textureType = convertTextureType(type);
	samplerState.textureFormat = imageDescriptor->format;
	samplerState.textureFilter = (instruction.samplerMethod == Gather) ? FILTER_GATHER : convertFilterMode(sampler);
	samplerState.border = sampler->borderColor;

	samplerState.addressingModeU = convertAddressingMode(0, sampler->addressModeU, type);
	samplerState.addressingModeV = convertAddressingMode(1, sampler->addressModeV, type);
	samplerState.addressingModeW = convertAddressingMode(2, sampler->addressModeW, type);

	samplerState.mipmapFilter = convertMipmapMode(sampler);
	samplerState.swizzle = imageDescriptor->swizzle;
	samplerState.gatherComponent = instruction.gatherComponent;
	samplerState.highPrecisionFiltering = false;
	samplerState.compareEnable = (sampler->compareEnable == VK_TRUE);
	samplerState.compareOp = sampler->compareOp;
	samplerState.unnormalizedCoordinates = (sampler->unnormalizedCoordinates == VK_TRUE);
	samplerState.largeTexture = (imageDescriptor->extent.width  > SHRT_MAX) ||
	                            (imageDescriptor->extent.height > SHRT_MAX) ||
	                            (imageDescriptor->extent.depth  > SHRT_MAX);

	if(sampler->ycbcrConversion)
	{
		samplerState.ycbcrModel = sampler->ycbcrConversion->ycbcrModel;
		samplerState.studioSwing = (sampler->ycbcrConversion->ycbcrRange == VK_SAMPLER_YCBCR_RANGE_ITU_NARROW);
		samplerState.swappedChroma = (sampler->ycbcrConversion->components.r != VK_COMPONENT_SWIZZLE_R);
	}

	if(sampler->anisotropyEnable != VK_FALSE)
	{
		UNSUPPORTED("anisotropyEnable");
	}

	auto fptr = emitSamplerFunction(instruction, samplerState);

	cache.emplace(key, fptr);
	return fptr;
}

SpirvShader::ImageSampler *SpirvShader::emitSamplerFunction(ImageInstruction instruction, const Sampler &samplerState)
{
	// TODO(b/129523279): Hold a separate mutex lock for the sampler being built.
	Function<Void(Pointer<Byte>, Pointer<Byte>, Pointer<SIMD::Float>, Pointer<SIMD::Float>, Pointer<Byte>)> function;
	{
		Pointer<Byte> texture = function.Arg<0>();
		Pointer<Byte> sampler = function.Arg<1>();
		Pointer<SIMD::Float> in = function.Arg<2>();
		Pointer<SIMD::Float> out = function.Arg<3>();
		Pointer<Byte> constants = function.Arg<4>();

		SIMD::Float uvw[4];
		SIMD::Float q;
		SIMD::Float lodOrBias;  // Explicit level-of-detail, or bias added to the implicit level-of-detail (depending on samplerMethod).
		Vector4f dsx;
		Vector4f dsy;
		Vector4f offset;
		SamplerFunction samplerFunction = instruction.getSamplerFunction();

		uint32_t i = 0;
		for( ; i < instruction.coordinates; i++)
		{
			uvw[i] = in[i];
		}

		if (instruction.isDref())
		{
			q = in[i];
			i++;
		}

		// TODO(b/129523279): Currently 1D textures are treated as 2D by setting the second coordinate to 0.
		// Implement optimized 1D sampling.
		if(samplerState.textureType == TEXTURE_1D)
		{
			uvw[1] = SIMD::Float(0);
		}
		else if(samplerState.textureType == TEXTURE_1D_ARRAY)
		{
			uvw[1] = SIMD::Float(0);
			uvw[2] = in[1];  // Move 1D layer coordinate to 2D layer coordinate index.
		}

		if(instruction.samplerMethod == Lod || instruction.samplerMethod == Bias || instruction.samplerMethod == Fetch)
		{
			lodOrBias = in[i];
			i++;
		}
		else if(instruction.samplerMethod == Grad)
		{
			for(uint32_t j = 0; j < instruction.gradComponents; j++, i++)
			{
				dsx[j] = in[i];
			}

			for(uint32_t j = 0; j < instruction.gradComponents; j++, i++)
			{
				dsy[j] = in[i];
			}
		}

		if(instruction.samplerOption == Offset)
		{
			for(uint32_t j = 0; j < instruction.offsetComponents; j++, i++)
			{
				offset[j] = in[i];
			}
		}

		SamplerCore s(constants, samplerState);
		Vector4f sample = s.sampleTexture(texture, sampler, uvw[0], uvw[1], uvw[2], q, lodOrBias, dsx, dsy, offset, samplerFunction);

		Pointer<SIMD::Float> rgba = out;
		rgba[0] = sample.x;
		rgba[1] = sample.y;
		rgba[2] = sample.z;
		rgba[3] = sample.w;
	}

	return (ImageSampler*)function("sampler")->getEntry();
}

sw::TextureType SpirvShader::convertTextureType(VkImageViewType imageViewType)
{
	switch(imageViewType)
	{
	case VK_IMAGE_VIEW_TYPE_1D:         return TEXTURE_1D;
	case VK_IMAGE_VIEW_TYPE_2D:         return TEXTURE_2D;
	case VK_IMAGE_VIEW_TYPE_3D:         return TEXTURE_3D;
	case VK_IMAGE_VIEW_TYPE_CUBE:       return TEXTURE_CUBE;
	case VK_IMAGE_VIEW_TYPE_1D_ARRAY:   return TEXTURE_1D_ARRAY;
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:   return TEXTURE_2D_ARRAY;
//	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY: return TEXTURE_CUBE_ARRAY;
	default:
		UNIMPLEMENTED("imageViewType %d", imageViewType);
		return TEXTURE_2D;
	}
}

sw::FilterType SpirvShader::convertFilterMode(const vk::Sampler *sampler)
{
	switch(sampler->magFilter)
	{
	case VK_FILTER_NEAREST:
		switch(sampler->minFilter)
		{
		case VK_FILTER_NEAREST: return FILTER_POINT;
		case VK_FILTER_LINEAR:  return FILTER_MIN_LINEAR_MAG_POINT;
		default:
			UNIMPLEMENTED("minFilter %d", sampler->minFilter);
			return FILTER_POINT;
		}
		break;
	case VK_FILTER_LINEAR:
		switch(sampler->minFilter)
		{
		case VK_FILTER_NEAREST: return FILTER_MIN_POINT_MAG_LINEAR;
		case VK_FILTER_LINEAR:  return FILTER_LINEAR;
		default:
			UNIMPLEMENTED("minFilter %d", sampler->minFilter);
			return FILTER_POINT;
		}
		break;
	default:
		UNIMPLEMENTED("magFilter %d", sampler->magFilter);
		return FILTER_POINT;
	}
}

sw::MipmapType SpirvShader::convertMipmapMode(const vk::Sampler *sampler)
{
	if(sampler->ycbcrConversion)
	{
		return MIPMAP_NONE;  // YCbCr images can only have one mipmap level.
	}

	switch(sampler->mipmapMode)
	{
	case VK_SAMPLER_MIPMAP_MODE_NEAREST: return MIPMAP_POINT;
	case VK_SAMPLER_MIPMAP_MODE_LINEAR:  return MIPMAP_LINEAR;
	default:
		UNIMPLEMENTED("mipmapMode %d", sampler->mipmapMode);
		return MIPMAP_POINT;
	}
}

sw::AddressingMode SpirvShader::convertAddressingMode(int coordinateIndex, VkSamplerAddressMode addressMode, VkImageViewType imageViewType)
{
	switch(imageViewType)
	{
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		UNSUPPORTED("SPIR-V ImageCubeArray Capability (imageViewType: %d)", int(imageViewType));
		if(coordinateIndex == 3)
		{
			return ADDRESSING_LAYER;
		}
		// Fall through to CUBE case:
	case VK_IMAGE_VIEW_TYPE_CUBE:
		if(coordinateIndex >= 2)
		{
			// Cube faces are addressed as 2D images.
			return ADDRESSING_UNUSED;
		}
		else
		{
			// Vulkan 1.1 spec:
			// "Cube images ignore the wrap modes specified in the sampler. Instead, if VK_FILTER_NEAREST is used within a mip level then
			//  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE is used, and if VK_FILTER_LINEAR is used within a mip level then sampling at the edges
			//  is performed as described earlier in the Cube map edge handling section."
			// This corresponds with our 'SEAMLESS' addressing mode.
			return ADDRESSING_SEAMLESS;
		}
		break;

	case VK_IMAGE_VIEW_TYPE_1D:  // Treated as 2D texture with second coordinate 0.
		if(coordinateIndex == 1)
		{
			return ADDRESSING_WRAP;
		}
		else if(coordinateIndex >= 2)
		{
			return ADDRESSING_UNUSED;
		}
		break;

	case VK_IMAGE_VIEW_TYPE_3D:
		if(coordinateIndex >= 3)
		{
			return ADDRESSING_UNUSED;
		}
		break;

	case VK_IMAGE_VIEW_TYPE_1D_ARRAY:  // Treated as 2D texture with second coordinate 0.
		if(coordinateIndex == 1)
		{
			return ADDRESSING_WRAP;
		}
		// Fall through to 2D_ARRAY case:
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		if(coordinateIndex == 2)
		{
			return ADDRESSING_LAYER;
		}
		else if(coordinateIndex >= 3)
		{
			return ADDRESSING_UNUSED;
		}
		// Fall through to 2D case:
	case VK_IMAGE_VIEW_TYPE_2D:
		if(coordinateIndex >= 2)
		{
			return ADDRESSING_UNUSED;
		}
		break;

	default:
		UNIMPLEMENTED("imageViewType %d", imageViewType);
		return ADDRESSING_WRAP;
	}

	switch(addressMode)
	{
	case VK_SAMPLER_ADDRESS_MODE_REPEAT:               return ADDRESSING_WRAP;
	case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:      return ADDRESSING_MIRROR;
	case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:        return ADDRESSING_CLAMP;
	case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:      return ADDRESSING_BORDER;
	case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: return ADDRESSING_MIRRORONCE;
	default:
		UNIMPLEMENTED("addressMode %d", addressMode);
		return ADDRESSING_WRAP;
	}
}

} // namespace sw
