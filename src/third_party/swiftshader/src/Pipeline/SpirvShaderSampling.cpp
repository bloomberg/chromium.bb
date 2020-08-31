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

#include "SamplerCore.hpp"  // TODO: Figure out what's needed.
#include "Device/Config.hpp"
#include "System/Debug.hpp"
#include "System/Math.hpp"
#include "Vulkan/VkDescriptorSetLayout.hpp"
#include "Vulkan/VkDevice.hpp"
#include "Vulkan/VkImageView.hpp"
#include "Vulkan/VkSampler.hpp"

#include <spirv/unified1/spirv.hpp>

#include <climits>
#include <mutex>

namespace sw {

SpirvShader::ImageSampler *SpirvShader::getImageSampler(uint32_t inst, vk::SampledImageDescriptor const *imageDescriptor, const vk::Sampler *sampler)
{
	ImageInstruction instruction(inst);
	const auto samplerId = sampler ? sampler->id : 0;
	ASSERT(imageDescriptor->imageViewId != 0 && (samplerId != 0 || instruction.samplerMethod == Fetch));
	ASSERT(imageDescriptor->device);

	vk::Device::SamplingRoutineCache::Key key = { inst, imageDescriptor->imageViewId, samplerId };

	vk::Device::SamplingRoutineCache *cache = imageDescriptor->device->getSamplingRoutineCache();

	auto createSamplingRoutine = [&](const vk::Device::SamplingRoutineCache::Key &key) {
		auto type = imageDescriptor->type;

		Sampler samplerState = {};
		samplerState.textureType = type;
		samplerState.textureFormat = imageDescriptor->format;

		samplerState.addressingModeU = convertAddressingMode(0, sampler, type);
		samplerState.addressingModeV = convertAddressingMode(1, sampler, type);
		samplerState.addressingModeW = convertAddressingMode(2, sampler, type);

		samplerState.mipmapFilter = convertMipmapMode(sampler);
		samplerState.swizzle = imageDescriptor->swizzle;
		samplerState.gatherComponent = instruction.gatherComponent;

		if(sampler)
		{
			samplerState.textureFilter = convertFilterMode(sampler, type, instruction);
			samplerState.border = sampler->borderColor;

			samplerState.mipmapFilter = convertMipmapMode(sampler);
			samplerState.highPrecisionFiltering = (sampler->filteringPrecision == VK_SAMPLER_FILTERING_PRECISION_MODE_HIGH_GOOGLE);

			samplerState.compareEnable = (sampler->compareEnable != VK_FALSE);
			samplerState.compareOp = sampler->compareOp;
			samplerState.unnormalizedCoordinates = (sampler->unnormalizedCoordinates != VK_FALSE);

			samplerState.ycbcrModel = sampler->ycbcrModel;
			samplerState.studioSwing = sampler->studioSwing;
			samplerState.swappedChroma = sampler->swappedChroma;

			samplerState.mipLodBias = sampler->mipLodBias;
			samplerState.maxAnisotropy = sampler->maxAnisotropy;
			samplerState.minLod = sampler->minLod;
			samplerState.maxLod = sampler->maxLod;
		}
		else
		{
			// OpImageFetch does not take a sampler descriptor, but for VK_EXT_image_robustness
			// requires replacing invalid texels with zero.
			// TODO(b/162327166): Only perform bounds checks when VK_EXT_image_robustness is enabled.
			samplerState.border = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		}

		return emitSamplerRoutine(instruction, samplerState);
	};

	auto routine = cache->getOrCreate(key, createSamplingRoutine);

	return (ImageSampler *)(routine->getEntry());
}

std::shared_ptr<rr::Routine> SpirvShader::emitSamplerRoutine(ImageInstruction instruction, const Sampler &samplerState)
{
	// TODO(b/129523279): Hold a separate mutex lock for the sampler being built.
	rr::Function<Void(Pointer<Byte>, Pointer<SIMD::Float>, Pointer<SIMD::Float>, Pointer<Byte>)> function;
	{
		Pointer<Byte> texture = function.Arg<0>();
		Pointer<SIMD::Float> in = function.Arg<1>();
		Pointer<SIMD::Float> out = function.Arg<2>();
		Pointer<Byte> constants = function.Arg<3>();

		SIMD::Float uvwa[4] = { 0, 0, 0, 0 };
		SIMD::Float dRef = 0;
		SIMD::Float lodOrBias = 0;  // Explicit level-of-detail, or bias added to the implicit level-of-detail (depending on samplerMethod).
		Vector4f dsx = { 0, 0, 0, 0 };
		Vector4f dsy = { 0, 0, 0, 0 };
		Vector4i offset = { 0, 0, 0, 0 };
		SIMD::Int sampleId = 0;
		SamplerFunction samplerFunction = instruction.getSamplerFunction();

		uint32_t i = 0;
		for(; i < instruction.coordinates; i++)
		{
			uvwa[i] = in[i];
		}

		if(instruction.isDref())
		{
			dRef = in[i];
			i++;
		}

		if(instruction.samplerMethod == Lod || instruction.samplerMethod == Bias || instruction.samplerMethod == Fetch)
		{
			lodOrBias = in[i];
			i++;
		}
		else if(instruction.samplerMethod == Grad)
		{
			for(uint32_t j = 0; j < instruction.grad; j++, i++)
			{
				dsx[j] = in[i];
			}

			for(uint32_t j = 0; j < instruction.grad; j++, i++)
			{
				dsy[j] = in[i];
			}
		}

		for(uint32_t j = 0; j < instruction.offset; j++, i++)
		{
			offset[j] = As<SIMD::Int>(in[i]);
		}

		if(instruction.sample)
		{
			sampleId = As<SIMD::Int>(in[i]);
		}

		SamplerCore s(constants, samplerState);

		// For explicit-lod instructions the LOD can be different per SIMD lane. SamplerCore currently assumes
		// a single LOD per four elements, so we sample the image again for each LOD separately.
		if(samplerFunction.method == Lod || samplerFunction.method == Grad)  // TODO(b/133868964): Also handle divergent Bias and Fetch with Lod.
		{
			auto lod = Pointer<Float>(&lodOrBias);

			For(Int i = 0, i < SIMD::Width, i++)
			{
				SIMD::Float dPdx;
				SIMD::Float dPdy;

				dPdx.x = Pointer<Float>(&dsx.x)[i];
				dPdx.y = Pointer<Float>(&dsx.y)[i];
				dPdx.z = Pointer<Float>(&dsx.z)[i];

				dPdy.x = Pointer<Float>(&dsy.x)[i];
				dPdy.y = Pointer<Float>(&dsy.y)[i];
				dPdy.z = Pointer<Float>(&dsy.z)[i];

				Vector4f sample = s.sampleTexture(texture, uvwa, dRef, lod[i], dPdx, dPdy, offset, sampleId, samplerFunction);

				Pointer<Float> rgba = out;
				rgba[0 * SIMD::Width + i] = Pointer<Float>(&sample.x)[i];
				rgba[1 * SIMD::Width + i] = Pointer<Float>(&sample.y)[i];
				rgba[2 * SIMD::Width + i] = Pointer<Float>(&sample.z)[i];
				rgba[3 * SIMD::Width + i] = Pointer<Float>(&sample.w)[i];
			}
		}
		else
		{
			Vector4f sample = s.sampleTexture(texture, uvwa, dRef, lodOrBias.x, (dsx.x), (dsy.x), offset, sampleId, samplerFunction);

			Pointer<SIMD::Float> rgba = out;
			rgba[0] = sample.x;
			rgba[1] = sample.y;
			rgba[2] = sample.z;
			rgba[3] = sample.w;
		}
	}

	return function("sampler");
}

sw::FilterType SpirvShader::convertFilterMode(const vk::Sampler *sampler, VkImageViewType imageViewType, ImageInstruction instruction)
{
	if(instruction.samplerMethod == Gather)
	{
		return FILTER_GATHER;
	}

	if(instruction.samplerMethod == Fetch)
	{
		return FILTER_POINT;
	}

	if(sampler->anisotropyEnable != VK_FALSE)
	{
		if(imageViewType == VK_IMAGE_VIEW_TYPE_2D || imageViewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
		{
			if(instruction.samplerMethod != Lod)  // TODO(b/162926129): Support anisotropic filtering with explicit LOD.
			{
				return FILTER_ANISOTROPIC;
			}
		}
	}

	switch(sampler->magFilter)
	{
		case VK_FILTER_NEAREST:
			switch(sampler->minFilter)
			{
				case VK_FILTER_NEAREST: return FILTER_POINT;
				case VK_FILTER_LINEAR: return FILTER_MIN_LINEAR_MAG_POINT;
				default:
					UNSUPPORTED("minFilter %d", sampler->minFilter);
					return FILTER_POINT;
			}
			break;
		case VK_FILTER_LINEAR:
			switch(sampler->minFilter)
			{
				case VK_FILTER_NEAREST: return FILTER_MIN_POINT_MAG_LINEAR;
				case VK_FILTER_LINEAR: return FILTER_LINEAR;
				default:
					UNSUPPORTED("minFilter %d", sampler->minFilter);
					return FILTER_POINT;
			}
			break;
		default:
			break;
	}

	UNSUPPORTED("magFilter %d", sampler->magFilter);
	return FILTER_POINT;
}

sw::MipmapType SpirvShader::convertMipmapMode(const vk::Sampler *sampler)
{
	if(!sampler)
	{
		return MIPMAP_POINT;  // Samplerless operations (OpImageFetch) can take an integer Lod operand.
	}

	if(sampler->ycbcrModel != VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY)
	{
		// TODO(b/151263485): Check image view level count instead.
		return MIPMAP_NONE;
	}

	switch(sampler->mipmapMode)
	{
		case VK_SAMPLER_MIPMAP_MODE_NEAREST: return MIPMAP_POINT;
		case VK_SAMPLER_MIPMAP_MODE_LINEAR: return MIPMAP_LINEAR;
		default:
			UNSUPPORTED("mipmapMode %d", sampler->mipmapMode);
			return MIPMAP_POINT;
	}
}

sw::AddressingMode SpirvShader::convertAddressingMode(int coordinateIndex, const vk::Sampler *sampler, VkImageViewType imageViewType)
{
	switch(imageViewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
			if(coordinateIndex >= 1)
			{
				return ADDRESSING_UNUSED;
			}
			break;
		case VK_IMAGE_VIEW_TYPE_2D:
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
			if(coordinateIndex == 2)
			{
				return ADDRESSING_UNUSED;
			}
			break;

		case VK_IMAGE_VIEW_TYPE_3D:
			break;

		case VK_IMAGE_VIEW_TYPE_CUBE:
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			if(coordinateIndex <= 1)  // Cube faces themselves are addressed as 2D images.
			{
				// Vulkan 1.1 spec:
				// "Cube images ignore the wrap modes specified in the sampler. Instead, if VK_FILTER_NEAREST is used within a mip level then
				//  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE is used, and if VK_FILTER_LINEAR is used within a mip level then sampling at the edges
				//  is performed as described earlier in the Cube map edge handling section."
				// This corresponds with our 'SEAMLESS' addressing mode.
				return ADDRESSING_SEAMLESS;
			}
			else  // coordinateIndex == 2
			{
				// The cube face is an index into 2D array layers.
				return ADDRESSING_CUBEFACE;
			}
			break;

		default:
			UNSUPPORTED("imageViewType %d", imageViewType);
			return ADDRESSING_WRAP;
	}

	if(!sampler)
	{
		// OpImageFetch does not take a sampler descriptor, but still needs a valid
		// addressing mode that prevents out-of-bounds accesses:
		// "The value returned by a read of an invalid texel is undefined, unless that
		//  read operation is from a buffer resource and the robustBufferAccess feature
		//  is enabled. In that case, an invalid texel is replaced as described by the
		//  robustBufferAccess feature." - Vulkan 1.1

		// VK_EXT_image_robustness requires nullifying out-of-bounds accesses.
		// ADDRESSING_BORDER causes texel replacement to be performed.
		// TODO(b/162327166): Only perform bounds checks when VK_EXT_image_robustness is enabled.
		return ADDRESSING_BORDER;
	}

	VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	switch(coordinateIndex)
	{
		case 0: addressMode = sampler->addressModeU; break;
		case 1: addressMode = sampler->addressModeV; break;
		case 2: addressMode = sampler->addressModeW; break;
		default: UNSUPPORTED("coordinateIndex: %d", coordinateIndex);
	}

	switch(addressMode)
	{
		case VK_SAMPLER_ADDRESS_MODE_REPEAT: return ADDRESSING_WRAP;
		case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: return ADDRESSING_MIRROR;
		case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: return ADDRESSING_CLAMP;
		case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: return ADDRESSING_BORDER;
		case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: return ADDRESSING_MIRRORONCE;
		default:
			UNSUPPORTED("addressMode %d", addressMode);
			return ADDRESSING_WRAP;
	}
}

}  // namespace sw
