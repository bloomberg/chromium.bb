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

#include "PixelRoutine.hpp"

#include "SamplerCore.hpp"
#include "Constants.hpp"
#include "Device/Renderer.hpp"
#include "Device/QuadRasterizer.hpp"
#include "Device/Primitive.hpp"
#include "Vulkan/VkDebug.hpp"
#include "Vulkan/VkPipelineLayout.hpp"

namespace sw
{
	extern bool postBlendSRGB;
	extern bool exactColorRounding;
	extern bool forceClearRegisters;

	PixelRoutine::PixelRoutine(
			const PixelProcessor::State &state,
			vk::PipelineLayout const *pipelineLayout,
			SpirvShader const *spirvShader,
			const vk::DescriptorSet::Bindings &descriptorSets)
		: QuadRasterizer(state, spirvShader),
		  routine(pipelineLayout),
		  descriptorSets(descriptorSets)
	{
		if (spirvShader)
		{
			spirvShader->emitProlog(&routine);
			if (forceClearRegisters)
			{
				for (int i = 0; i < MAX_INTERFACE_COMPONENTS; i++)
				{
					routine.inputs[i] = Float4(0.0f);
				}
			}
		}
	}

	PixelRoutine::~PixelRoutine()
	{
	}

	void PixelRoutine::quad(Pointer<Byte> cBuffer[RENDERTARGETS], Pointer<Byte> &zBuffer, Pointer<Byte> &sBuffer, Int cMask[4], Int &x, Int &y)
	{
		#if PERF_PROFILE
			Long pipeTime = Ticks();
		#endif

		// TODO: consider shader which modifies sample mask in general
		const bool earlyDepthTest = !spirvShader || (!spirvShader->getModes().DepthReplacing && !state.alphaToCoverage);

		Int zMask[4];   // Depth mask
		Int sMask[4];   // Stencil mask

		for(unsigned int q = 0; q < state.multiSample; q++)
		{
			zMask[q] = cMask[q];
			sMask[q] = cMask[q];
		}

		for(unsigned int q = 0; q < state.multiSample; q++)
		{
			stencilTest(sBuffer, q, x, sMask[q], cMask[q]);
		}

		Float4 f;
		Float4 rhwCentroid;

		Float4 xxxx = Float4(Float(x)) + *Pointer<Float4>(primitive + OFFSET(Primitive,xQuad), 16);

		if(interpolateZ())
		{
			for(unsigned int q = 0; q < state.multiSample; q++)
			{
				Float4 x = xxxx;

				if(state.multiSample > 1)
				{
					x -= *Pointer<Float4>(constants + OFFSET(Constants,X) + q * sizeof(float4));
				}

				z[q] = interpolate(x, Dz[q], z[q], primitive + OFFSET(Primitive,z), false, false, state.depthClamp);
			}
		}

		Bool depthPass = false;

		if(earlyDepthTest)
		{
			for(unsigned int q = 0; q < state.multiSample; q++)
			{
				depthPass = depthPass || depthTest(zBuffer, q, x, z[q], sMask[q], zMask[q], cMask[q]);
			}
		}

		If(depthPass || Bool(!earlyDepthTest))
		{
			#if PERF_PROFILE
				Long interpTime = Ticks();
			#endif

			Float4 yyyy = Float4(Float(y)) + *Pointer<Float4>(primitive + OFFSET(Primitive,yQuad), 16);

			// Centroid locations
			Float4 XXXX = Float4(0.0f);
			Float4 YYYY = Float4(0.0f);

			if(state.centroid)
			{
				Float4 WWWW(1.0e-9f);

				for(unsigned int q = 0; q < state.multiSample; q++)
				{
					XXXX += *Pointer<Float4>(constants + OFFSET(Constants,sampleX[q]) + 16 * cMask[q]);
					YYYY += *Pointer<Float4>(constants + OFFSET(Constants,sampleY[q]) + 16 * cMask[q]);
					WWWW += *Pointer<Float4>(constants + OFFSET(Constants,weight) + 16 * cMask[q]);
				}

				WWWW = Rcp_pp(WWWW);
				XXXX *= WWWW;
				YYYY *= WWWW;

				XXXX += xxxx;
				YYYY += yyyy;
			}

			if(interpolateW())
			{
				w = interpolate(xxxx, Dw, rhw, primitive + OFFSET(Primitive,w), false, false, false);
				rhw = reciprocal(w, false, false, true);

				if(state.centroid)
				{
					rhwCentroid = reciprocal(interpolateCentroid(XXXX, YYYY, rhwCentroid, primitive + OFFSET(Primitive,w), false, false));
				}
			}

			if (spirvShader)
			{
				for (int interpolant = 0; interpolant < MAX_INTERFACE_COMPONENTS; interpolant++)
				{
					auto const &input = spirvShader->inputs[interpolant];
					if (input.Type != SpirvShader::ATTRIBTYPE_UNUSED)
					{
						if (input.Centroid && state.multiSample > 1)
						{
							routine.inputs[interpolant] =
									interpolateCentroid(XXXX, YYYY, rhwCentroid,
														primitive + OFFSET(Primitive, V[interpolant]),
														input.Flat, !input.NoPerspective);
						}
						else
						{
							routine.inputs[interpolant] =
									interpolate(xxxx, Dv[interpolant], rhw,
												primitive + OFFSET(Primitive, V[interpolant]),
												input.Flat, !input.NoPerspective, false);
						}
					}
				}

				setBuiltins(x, y, z, w);
			}

			#if PERF_PROFILE
				cycles[PERF_INTERP] += Ticks() - interpTime;
			#endif

			Bool alphaPass = true;

			#if PERF_PROFILE
				Long shaderTime = Ticks();
			#endif

			if (spirvShader)
			{
				applyShader(cMask);
			}

			#if PERF_PROFILE
				cycles[PERF_SHADER] += Ticks() - shaderTime;
			#endif

			alphaPass = alphaTest(cMask);

			if((spirvShader && spirvShader->getModes().ContainsKill) || state.alphaToCoverage)
			{
				for(unsigned int q = 0; q < state.multiSample; q++)
				{
					zMask[q] &= cMask[q];
					sMask[q] &= cMask[q];
				}
			}

			If(alphaPass)
			{
				if(!earlyDepthTest)
				{
					for(unsigned int q = 0; q < state.multiSample; q++)
					{
						depthPass = depthPass || depthTest(zBuffer, q, x, z[q], sMask[q], zMask[q], cMask[q]);
					}
				}

				#if PERF_PROFILE
					Long ropTime = Ticks();
				#endif

				If(depthPass || Bool(earlyDepthTest))
				{
					for(unsigned int q = 0; q < state.multiSample; q++)
					{
						if(state.multiSampleMask & (1 << q))
						{
							writeDepth(zBuffer, q, x, z[q], zMask[q]);

							if(state.occlusionEnabled)
							{
								occlusion += *Pointer<UInt>(constants + OFFSET(Constants,occlusionCount) + 4 * (zMask[q] & sMask[q]));
							}
						}
					}

					#if PERF_PROFILE
						AddAtomic(Pointer<Long>(&profiler.ropOperations), 4);
					#endif

					rasterOperation(cBuffer, x, sMask, zMask, cMask);
				}

				#if PERF_PROFILE
					cycles[PERF_ROP] += Ticks() - ropTime;
				#endif
			}
		}

		for(unsigned int q = 0; q < state.multiSample; q++)
		{
			if(state.multiSampleMask & (1 << q))
			{
				writeStencil(sBuffer, q, x, sMask[q], zMask[q], cMask[q]);
			}
		}

		#if PERF_PROFILE
			cycles[PERF_PIPE] += Ticks() - pipeTime;
		#endif
	}

	Float4 PixelRoutine::interpolateCentroid(Float4 &x, Float4 &y, Float4 &rhw, Pointer<Byte> planeEquation, bool flat, bool perspective)
	{
		Float4 interpolant = *Pointer<Float4>(planeEquation + OFFSET(PlaneEquation,C), 16);

		if(!flat)
		{
			interpolant += x * *Pointer<Float4>(planeEquation + OFFSET(PlaneEquation,A), 16) +
			               y * *Pointer<Float4>(planeEquation + OFFSET(PlaneEquation,B), 16);

			if(perspective)
			{
				interpolant *= rhw;
			}
		}

		return interpolant;
	}

	void PixelRoutine::stencilTest(Pointer<Byte> &sBuffer, int q, Int &x, Int &sMask, Int &cMask)
	{
		if(!state.stencilActive)
		{
			return;
		}

		// (StencilRef & StencilMask) CompFunc (StencilBufferValue & StencilMask)

		Pointer<Byte> buffer = sBuffer + 2 * x;

		if(q > 0)
		{
			buffer += q * *Pointer<Int>(data + OFFSET(DrawData,stencilSliceB));
		}

		Byte8 value = *Pointer<Byte8>(buffer);
		Byte8 valueBack = value;

		if(state.frontStencil.compareMask != 0xff)
		{
			value &= *Pointer<Byte8>(data + OFFSET(DrawData,stencil[0].testMaskQ));
		}

		stencilTest(value, state.frontStencil.compareOp, false);

		if(state.twoSidedStencil)
		{
			if(state.backStencil.compareMask != 0xff)
			{
				valueBack &= *Pointer<Byte8>(data + OFFSET(DrawData,stencil[1].testMaskQ));
			}

			stencilTest(valueBack, state.backStencil.compareOp, true);

			value &= *Pointer<Byte8>(primitive + OFFSET(Primitive,clockwiseMask));
			valueBack &= *Pointer<Byte8>(primitive + OFFSET(Primitive,invClockwiseMask));
			value |= valueBack;
		}

		sMask = SignMask(value) & cMask;
	}

	void PixelRoutine::stencilTest(Byte8 &value, VkCompareOp stencilCompareMode, bool isBack)
	{
		Byte8 equal;

		switch(stencilCompareMode)
		{
		case VK_COMPARE_OP_ALWAYS:
			value = Byte8(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
			break;
		case VK_COMPARE_OP_NEVER:
			value = Byte8(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
			break;
		case VK_COMPARE_OP_LESS:			// a < b ~ b > a
			value += Byte8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
			value = CmpGT(As<SByte8>(value), *Pointer<SByte8>(data + OFFSET(DrawData,stencil[isBack].referenceMaskedSignedQ)));
			break;
		case VK_COMPARE_OP_EQUAL:
			value = CmpEQ(value, *Pointer<Byte8>(data + OFFSET(DrawData,stencil[isBack].referenceMaskedQ)));
			break;
		case VK_COMPARE_OP_NOT_EQUAL:		// a != b ~ !(a == b)
			value = CmpEQ(value, *Pointer<Byte8>(data + OFFSET(DrawData,stencil[isBack].referenceMaskedQ)));
			value ^= Byte8(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
			break;
		case VK_COMPARE_OP_LESS_OR_EQUAL:	// a <= b ~ (b > a) || (a == b)
			equal = value;
			equal = CmpEQ(equal, *Pointer<Byte8>(data + OFFSET(DrawData,stencil[isBack].referenceMaskedQ)));
			value += Byte8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
			value = CmpGT(As<SByte8>(value), *Pointer<SByte8>(data + OFFSET(DrawData,stencil[isBack].referenceMaskedSignedQ)));
			value |= equal;
			break;
		case VK_COMPARE_OP_GREATER:		// a > b
			equal = *Pointer<Byte8>(data + OFFSET(DrawData,stencil[isBack].referenceMaskedSignedQ));
			value += Byte8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
			equal = CmpGT(As<SByte8>(equal), As<SByte8>(value));
			value = equal;
			break;
		case VK_COMPARE_OP_GREATER_OR_EQUAL:	// a >= b ~ !(a < b) ~ !(b > a)
			value += Byte8(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
			value = CmpGT(As<SByte8>(value), *Pointer<SByte8>(data + OFFSET(DrawData,stencil[isBack].referenceMaskedSignedQ)));
			value ^= Byte8(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
			break;
		default:
			UNIMPLEMENTED("VkCompareOp: %d", int(stencilCompareMode));
		}
	}

	Bool PixelRoutine::depthTest32F(Pointer<Byte> &zBuffer, int q, Int &x, Float4 &z, Int &sMask, Int &zMask, Int &cMask)
	{
		Float4 Z = z;

		if(spirvShader && spirvShader->getModes().DepthReplacing)
		{
			Z = oDepth;
		}

		Pointer<Byte> buffer;
		Int pitch;

		if(!state.quadLayoutDepthBuffer)
		{
			buffer = zBuffer + 4 * x;
			pitch = *Pointer<Int>(data + OFFSET(DrawData,depthPitchB));
		}
		else
		{
			buffer = zBuffer + 8 * x;
		}

		if(q > 0)
		{
			buffer += q * *Pointer<Int>(data + OFFSET(DrawData,depthSliceB));
		}

		Float4 zValue;

		if(state.depthCompareMode != VK_COMPARE_OP_NEVER || (state.depthCompareMode != VK_COMPARE_OP_ALWAYS && !state.depthWriteEnable))
		{
			if(!state.quadLayoutDepthBuffer)
			{
				// FIXME: Properly optimizes?
				zValue.xy = *Pointer<Float4>(buffer);
				zValue.zw = *Pointer<Float4>(buffer + pitch - 8);
			}
			else
			{
				zValue = *Pointer<Float4>(buffer, 16);
			}
		}

		Int4 zTest;

		switch(state.depthCompareMode)
		{
		case VK_COMPARE_OP_ALWAYS:
			// Optimized
			break;
		case VK_COMPARE_OP_NEVER:
			// Optimized
			break;
		case VK_COMPARE_OP_EQUAL:
			zTest = CmpEQ(zValue, Z);
			break;
		case VK_COMPARE_OP_NOT_EQUAL:
			zTest = CmpNEQ(zValue, Z);
			break;
		case VK_COMPARE_OP_LESS:
			zTest = CmpNLE(zValue, Z);
			break;
		case VK_COMPARE_OP_GREATER_OR_EQUAL:
			zTest = CmpLE(zValue, Z);
			break;
		case VK_COMPARE_OP_LESS_OR_EQUAL:
			zTest = CmpNLT(zValue, Z);
			break;
		case VK_COMPARE_OP_GREATER:
			zTest = CmpLT(zValue, Z);
			break;
		default:
			UNIMPLEMENTED("VkCompareOp: %d", int(state.depthCompareMode));
		}

		switch(state.depthCompareMode)
		{
		case VK_COMPARE_OP_ALWAYS:
			zMask = cMask;
			break;
		case VK_COMPARE_OP_NEVER:
			zMask = 0x0;
			break;
		default:
			zMask = SignMask(zTest) & cMask;
			break;
		}

		if(state.stencilActive)
		{
			zMask &= sMask;
		}

		return zMask != 0;
	}

	Bool PixelRoutine::depthTest16(Pointer<Byte> &zBuffer, int q, Int &x, Float4 &z, Int &sMask, Int &zMask, Int &cMask)
	{
		Short4 Z = convertFixed16(z, true);

		if(spirvShader && spirvShader->getModes().DepthReplacing)
		{
			Z = convertFixed16(oDepth, true);
		}

		Pointer<Byte> buffer;
		Int pitch;

		if(!state.quadLayoutDepthBuffer)
		{
			buffer = zBuffer + 2 * x;
			pitch = *Pointer<Int>(data + OFFSET(DrawData,depthPitchB));
		}
		else
		{
			buffer = zBuffer + 4 * x;
		}

		if(q > 0)
		{
			buffer += q * *Pointer<Int>(data + OFFSET(DrawData,depthSliceB));
		}

		Short4 zValue;

		if(state.depthCompareMode != VK_COMPARE_OP_NEVER || (state.depthCompareMode != VK_COMPARE_OP_ALWAYS && !state.depthWriteEnable))
		{
			if(!state.quadLayoutDepthBuffer)
			{
				// FIXME: Properly optimizes?
				zValue = *Pointer<Short4>(buffer) & Short4(-1, -1, 0, 0);
				zValue = zValue | (*Pointer<Short4>(buffer + pitch - 4) & Short4(0, 0, -1, -1));
			}
			else
			{
				zValue = *Pointer<Short4>(buffer, 8);
			}
		}

		Int4 zTest;

		// Bias values to make unsigned compares out of Reactor's (due SSE's) signed compares only
		zValue = zValue - Short4(0x8000u);
		Z = Z - Short4(0x8000u);

		switch(state.depthCompareMode)
		{
		case VK_COMPARE_OP_ALWAYS:
			// Optimized
			break;
		case VK_COMPARE_OP_NEVER:
			// Optimized
			break;
		case VK_COMPARE_OP_EQUAL:
			zTest = Int4(CmpEQ(zValue, Z));
			break;
		case VK_COMPARE_OP_NOT_EQUAL:
			zTest = ~Int4(CmpEQ(zValue, Z));
			break;
		case VK_COMPARE_OP_LESS:
			zTest = Int4(CmpGT(zValue, Z));
			break;
		case VK_COMPARE_OP_GREATER_OR_EQUAL:
			zTest = ~Int4(CmpGT(zValue, Z));
			break;
		case VK_COMPARE_OP_LESS_OR_EQUAL:
			zTest = ~Int4(CmpGT(Z, zValue));
			break;
		case VK_COMPARE_OP_GREATER:
			zTest = Int4(CmpGT(Z, zValue));
			break;
		default:
			UNIMPLEMENTED("VkCompareOp: %d", int(state.depthCompareMode));
		}

		switch(state.depthCompareMode)
		{
		case VK_COMPARE_OP_ALWAYS:
			zMask = cMask;
			break;
		case VK_COMPARE_OP_NEVER:
			zMask = 0x0;
			break;
		default:
			zMask = SignMask(zTest) & cMask;
			break;
		}

		if(state.stencilActive)
		{
			zMask &= sMask;
		}

		return zMask != 0;
	}

	Bool PixelRoutine::depthTest(Pointer<Byte> &zBuffer, int q, Int &x, Float4 &z, Int &sMask, Int &zMask, Int &cMask)
	{
		if(!state.depthTestActive)
		{
			return true;
		}

		if (state.depthFormat == VK_FORMAT_D16_UNORM)
			return depthTest16(zBuffer, q, x, z, sMask, zMask, cMask);
		else
			return depthTest32F(zBuffer, q, x, z, sMask, zMask, cMask);
	}

	void PixelRoutine::alphaToCoverage(Int cMask[4], Float4 &alpha)
	{
		Int4 coverage0 = CmpNLT(alpha, *Pointer<Float4>(data + OFFSET(DrawData,a2c0)));
		Int4 coverage1 = CmpNLT(alpha, *Pointer<Float4>(data + OFFSET(DrawData,a2c1)));
		Int4 coverage2 = CmpNLT(alpha, *Pointer<Float4>(data + OFFSET(DrawData,a2c2)));
		Int4 coverage3 = CmpNLT(alpha, *Pointer<Float4>(data + OFFSET(DrawData,a2c3)));

		Int aMask0 = SignMask(coverage0);
		Int aMask1 = SignMask(coverage1);
		Int aMask2 = SignMask(coverage2);
		Int aMask3 = SignMask(coverage3);

		cMask[0] &= aMask0;
		cMask[1] &= aMask1;
		cMask[2] &= aMask2;
		cMask[3] &= aMask3;
	}

	void PixelRoutine::writeDepth32F(Pointer<Byte> &zBuffer, int q, Int &x, Float4 &z, Int &zMask)
	{
		Float4 Z = z;

		if(spirvShader && spirvShader->getModes().DepthReplacing)
		{
			Z = oDepth;
		}

		Pointer<Byte> buffer;
		Int pitch;

		if(!state.quadLayoutDepthBuffer)
		{
			buffer = zBuffer + 4 * x;
			pitch = *Pointer<Int>(data + OFFSET(DrawData,depthPitchB));
		}
		else
		{
			buffer = zBuffer + 8 * x;
		}

		if(q > 0)
		{
			buffer += q * *Pointer<Int>(data + OFFSET(DrawData,depthSliceB));
		}

		Float4 zValue;

		if(state.depthCompareMode != VK_COMPARE_OP_NEVER || (state.depthCompareMode != VK_COMPARE_OP_ALWAYS && !state.depthWriteEnable))
		{
			if(!state.quadLayoutDepthBuffer)
			{
				// FIXME: Properly optimizes?
				zValue.xy = *Pointer<Float4>(buffer);
				zValue.zw = *Pointer<Float4>(buffer + pitch - 8);
			}
			else
			{
				zValue = *Pointer<Float4>(buffer, 16);
			}
		}

		Z = As<Float4>(As<Int4>(Z) & *Pointer<Int4>(constants + OFFSET(Constants,maskD4X) + zMask * 16, 16));
		zValue = As<Float4>(As<Int4>(zValue) & *Pointer<Int4>(constants + OFFSET(Constants,invMaskD4X) + zMask * 16, 16));
		Z = As<Float4>(As<Int4>(Z) | As<Int4>(zValue));

		if(!state.quadLayoutDepthBuffer)
		{
			// FIXME: Properly optimizes?
			*Pointer<Float2>(buffer) = Float2(Z.xy);
			*Pointer<Float2>(buffer + pitch) = Float2(Z.zw);
		}
		else
		{
			*Pointer<Float4>(buffer, 16) = Z;
		}
	}

	void PixelRoutine::writeDepth16(Pointer<Byte> &zBuffer, int q, Int &x, Float4 &z, Int &zMask)
	{
		Short4 Z = As<Short4>(convertFixed16(z, true));

		if(spirvShader && spirvShader->getModes().DepthReplacing)
		{
			Z = As<Short4>(convertFixed16(oDepth, true));
		}

		Pointer<Byte> buffer;
		Int pitch;

		if(!state.quadLayoutDepthBuffer)
		{
			buffer = zBuffer + 2 * x;
			pitch = *Pointer<Int>(data + OFFSET(DrawData,depthPitchB));
		}
		else
		{
			buffer = zBuffer + 4 * x;
		}

		if(q > 0)
		{
			buffer += q * *Pointer<Int>(data + OFFSET(DrawData,depthSliceB));
		}

		Short4 zValue;

		if(state.depthCompareMode != VK_COMPARE_OP_NEVER || (state.depthCompareMode != VK_COMPARE_OP_ALWAYS && !state.depthWriteEnable))
		{
			if(!state.quadLayoutDepthBuffer)
			{
				// FIXME: Properly optimizes?
				zValue = *Pointer<Short4>(buffer) & Short4(-1, -1, 0, 0);
				zValue = zValue | (*Pointer<Short4>(buffer + pitch - 4) & Short4(0, 0, -1, -1));
			}
			else
			{
				zValue = *Pointer<Short4>(buffer, 8);
			}
		}

		Z = Z & *Pointer<Short4>(constants + OFFSET(Constants,maskW4Q) + zMask * 8, 8);
		zValue = zValue & *Pointer<Short4>(constants + OFFSET(Constants,invMaskW4Q) + zMask * 8, 8);
		Z = Z | zValue;

		if(!state.quadLayoutDepthBuffer)
		{
			// FIXME: Properly optimizes?
			*Pointer<Short>(buffer) = Extract(Z, 0);
			*Pointer<Short>(buffer+2) = Extract(Z, 1);
			*Pointer<Short>(buffer+pitch) = Extract(Z, 2);
			*Pointer<Short>(buffer+pitch+2) = Extract(Z, 3);
		}
		else
		{
			*Pointer<Short4>(buffer, 8) = Z;
		}
	}

	void PixelRoutine::writeDepth(Pointer<Byte> &zBuffer, int q, Int &x, Float4 &z, Int &zMask)
	{
		if(!state.depthWriteEnable)
		{
			return;
		}

		if (state.depthFormat == VK_FORMAT_D16_UNORM)
			writeDepth16(zBuffer, q, x, z, zMask);
		else
			writeDepth32F(zBuffer, q, x, z, zMask);
	}

	void PixelRoutine::writeStencil(Pointer<Byte> &sBuffer, int q, Int &x, Int &sMask, Int &zMask, Int &cMask)
	{
		if(!state.stencilActive)
		{
			return;
		}

		if(state.frontStencil.passOp == VK_STENCIL_OP_KEEP && state.frontStencil.depthFailOp == VK_STENCIL_OP_KEEP && state.frontStencil.failOp == VK_STENCIL_OP_KEEP)
		{
			if(!state.twoSidedStencil || (state.backStencil.passOp == VK_STENCIL_OP_KEEP && state.backStencil.depthFailOp == VK_STENCIL_OP_KEEP && state.backStencil.failOp == VK_STENCIL_OP_KEEP))
			{
				return;
			}
		}

		if((state.frontStencil.writeMask == 0) && (!state.twoSidedStencil || (state.backStencil.writeMask == 0)))
		{
			return;
		}

		Pointer<Byte> buffer = sBuffer + 2 * x;

		if(q > 0)
		{
			buffer += q * *Pointer<Int>(data + OFFSET(DrawData,stencilSliceB));
		}

		Byte8 bufferValue = *Pointer<Byte8>(buffer);

		Byte8 newValue;
		stencilOperation(newValue, bufferValue, state.frontStencil, false, zMask, sMask);

		if(state.frontStencil.writeMask != 0)
		{
			Byte8 maskedValue = bufferValue;
			newValue &= *Pointer<Byte8>(data + OFFSET(DrawData,stencil[0].writeMaskQ));
			maskedValue &= *Pointer<Byte8>(data + OFFSET(DrawData,stencil[0].invWriteMaskQ));
			newValue |= maskedValue;
		}

		if(state.twoSidedStencil)
		{
			Byte8 newValueBack;

			stencilOperation(newValueBack, bufferValue, state.backStencil, true, zMask, sMask);

			if(state.backStencil.writeMask != 0)
			{
				Byte8 maskedValue = bufferValue;
				newValueBack &= *Pointer<Byte8>(data + OFFSET(DrawData,stencil[1].writeMaskQ));
				maskedValue &= *Pointer<Byte8>(data + OFFSET(DrawData,stencil[1].invWriteMaskQ));
				newValueBack |= maskedValue;
			}

			newValue &= *Pointer<Byte8>(primitive + OFFSET(Primitive,clockwiseMask));
			newValueBack &= *Pointer<Byte8>(primitive + OFFSET(Primitive,invClockwiseMask));
			newValue |= newValueBack;
		}

		newValue &= *Pointer<Byte8>(constants + OFFSET(Constants,maskB4Q) + 8 * cMask);
		bufferValue &= *Pointer<Byte8>(constants + OFFSET(Constants,invMaskB4Q) + 8 * cMask);
		newValue |= bufferValue;

		*Pointer<Byte4>(buffer) = Byte4(newValue);
	}

	void PixelRoutine::stencilOperation(Byte8 &newValue, Byte8 &bufferValue, VkStencilOpState const &ops, bool isBack, Int &zMask, Int &sMask)
	{
		Byte8 &pass = newValue;
		Byte8 fail;
		Byte8 zFail;

		stencilOperation(pass, bufferValue, ops.passOp, isBack);

		if(ops.depthFailOp != ops.passOp)
		{
			stencilOperation(zFail, bufferValue, ops.depthFailOp, isBack);
		}

		if(ops.failOp != ops.passOp || ops.failOp != ops.depthFailOp)
		{
			stencilOperation(fail, bufferValue, ops.failOp, isBack);
		}

		if(ops.failOp != ops.passOp || ops.failOp != ops.depthFailOp)
		{
			if(state.depthTestActive && ops.depthFailOp != ops.passOp)   // zMask valid and values not the same
			{
				pass &= *Pointer<Byte8>(constants + OFFSET(Constants,maskB4Q) + 8 * zMask);
				zFail &= *Pointer<Byte8>(constants + OFFSET(Constants,invMaskB4Q) + 8 * zMask);
				pass |= zFail;
			}

			pass &= *Pointer<Byte8>(constants + OFFSET(Constants,maskB4Q) + 8 * sMask);
			fail &= *Pointer<Byte8>(constants + OFFSET(Constants,invMaskB4Q) + 8 * sMask);
			pass |= fail;
		}
	}

	void PixelRoutine::stencilOperation(Byte8 &output, Byte8 &bufferValue, VkStencilOp operation, bool isBack)
	{
		switch(operation)
		{
		case VK_STENCIL_OP_KEEP:
			output = bufferValue;
			break;
		case VK_STENCIL_OP_ZERO:
			output = Byte8(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
			break;
		case VK_STENCIL_OP_REPLACE:
			output = *Pointer<Byte8>(data + OFFSET(DrawData,stencil[isBack].referenceQ));
			break;
		case VK_STENCIL_OP_INCREMENT_AND_CLAMP:
			output = AddSat(bufferValue, Byte8(1, 1, 1, 1, 1, 1, 1, 1));
			break;
		case VK_STENCIL_OP_DECREMENT_AND_CLAMP:
			output = SubSat(bufferValue, Byte8(1, 1, 1, 1, 1, 1, 1, 1));
			break;
		case VK_STENCIL_OP_INVERT:
			output = bufferValue ^ Byte8(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
			break;
		case VK_STENCIL_OP_INCREMENT_AND_WRAP:
			output = bufferValue + Byte8(1, 1, 1, 1, 1, 1, 1, 1);
			break;
		case VK_STENCIL_OP_DECREMENT_AND_WRAP:
			output = bufferValue - Byte8(1, 1, 1, 1, 1, 1, 1, 1);
			break;
		default:
			UNIMPLEMENTED("VkStencilOp: %d", int(operation));
		}
	}

	void PixelRoutine::blendFactor(Vector4s &blendFactor, const Vector4s &current, const Vector4s &pixel, VkBlendFactor blendFactorActive)
	{
		switch(blendFactorActive)
		{
		case VK_BLEND_FACTOR_ZERO:
			// Optimized
			break;
		case VK_BLEND_FACTOR_ONE:
			// Optimized
			break;
		case VK_BLEND_FACTOR_SRC_COLOR:
			blendFactor.x = current.x;
			blendFactor.y = current.y;
			blendFactor.z = current.z;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
			blendFactor.x = Short4(0xFFFFu) - current.x;
			blendFactor.y = Short4(0xFFFFu) - current.y;
			blendFactor.z = Short4(0xFFFFu) - current.z;
			break;
		case VK_BLEND_FACTOR_DST_COLOR:
			blendFactor.x = pixel.x;
			blendFactor.y = pixel.y;
			blendFactor.z = pixel.z;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
			blendFactor.x = Short4(0xFFFFu) - pixel.x;
			blendFactor.y = Short4(0xFFFFu) - pixel.y;
			blendFactor.z = Short4(0xFFFFu) - pixel.z;
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA:
			blendFactor.x = current.w;
			blendFactor.y = current.w;
			blendFactor.z = current.w;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
			blendFactor.x = Short4(0xFFFFu) - current.w;
			blendFactor.y = Short4(0xFFFFu) - current.w;
			blendFactor.z = Short4(0xFFFFu) - current.w;
			break;
		case VK_BLEND_FACTOR_DST_ALPHA:
			blendFactor.x = pixel.w;
			blendFactor.y = pixel.w;
			blendFactor.z = pixel.w;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
			blendFactor.x = Short4(0xFFFFu) - pixel.w;
			blendFactor.y = Short4(0xFFFFu) - pixel.w;
			blendFactor.z = Short4(0xFFFFu) - pixel.w;
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
			blendFactor.x = Short4(0xFFFFu) - pixel.w;
			blendFactor.x = Min(As<UShort4>(blendFactor.x), As<UShort4>(current.w));
			blendFactor.y = blendFactor.x;
			blendFactor.z = blendFactor.x;
			break;
		case VK_BLEND_FACTOR_CONSTANT_COLOR:
			blendFactor.x = *Pointer<Short4>(data + OFFSET(DrawData,factor.blendConstant4W[0]));
			blendFactor.y = *Pointer<Short4>(data + OFFSET(DrawData,factor.blendConstant4W[1]));
			blendFactor.z = *Pointer<Short4>(data + OFFSET(DrawData,factor.blendConstant4W[2]));
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
			blendFactor.x = *Pointer<Short4>(data + OFFSET(DrawData,factor.invBlendConstant4W[0]));
			blendFactor.y = *Pointer<Short4>(data + OFFSET(DrawData,factor.invBlendConstant4W[1]));
			blendFactor.z = *Pointer<Short4>(data + OFFSET(DrawData,factor.invBlendConstant4W[2]));
			break;
		case VK_BLEND_FACTOR_CONSTANT_ALPHA:
			blendFactor.x = *Pointer<Short4>(data + OFFSET(DrawData,factor.blendConstant4W[3]));
			blendFactor.y = *Pointer<Short4>(data + OFFSET(DrawData,factor.blendConstant4W[3]));
			blendFactor.z = *Pointer<Short4>(data + OFFSET(DrawData,factor.blendConstant4W[3]));
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
			blendFactor.x = *Pointer<Short4>(data + OFFSET(DrawData,factor.invBlendConstant4W[3]));
			blendFactor.y = *Pointer<Short4>(data + OFFSET(DrawData,factor.invBlendConstant4W[3]));
			blendFactor.z = *Pointer<Short4>(data + OFFSET(DrawData,factor.invBlendConstant4W[3]));
			break;
		default:
			UNIMPLEMENTED("VkBlendFactor: %d", int(blendFactorActive));
		}
	}

	void PixelRoutine::blendFactorAlpha(Vector4s &blendFactor, const Vector4s &current, const Vector4s &pixel, VkBlendFactor blendFactorAlphaActive)
	{
		switch(blendFactorAlphaActive)
		{
		case VK_BLEND_FACTOR_ZERO:
			// Optimized
			break;
		case VK_BLEND_FACTOR_ONE:
			// Optimized
			break;
		case VK_BLEND_FACTOR_SRC_COLOR:
			blendFactor.w = current.w;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
			blendFactor.w = Short4(0xFFFFu) - current.w;
			break;
		case VK_BLEND_FACTOR_DST_COLOR:
			blendFactor.w = pixel.w;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
			blendFactor.w = Short4(0xFFFFu) - pixel.w;
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA:
			blendFactor.w = current.w;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
			blendFactor.w = Short4(0xFFFFu) - current.w;
			break;
		case VK_BLEND_FACTOR_DST_ALPHA:
			blendFactor.w = pixel.w;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
			blendFactor.w = Short4(0xFFFFu) - pixel.w;
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
			blendFactor.w = Short4(0xFFFFu);
			break;
		case VK_BLEND_FACTOR_CONSTANT_COLOR:
		case VK_BLEND_FACTOR_CONSTANT_ALPHA:
			blendFactor.w = *Pointer<Short4>(data + OFFSET(DrawData,factor.blendConstant4W[3]));
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
		case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
			blendFactor.w = *Pointer<Short4>(data + OFFSET(DrawData,factor.invBlendConstant4W[3]));
			break;
		default:
			UNIMPLEMENTED("VkBlendFactor: %d", int(blendFactorAlphaActive));
		}
	}

	bool PixelRoutine::isSRGB(int index) const
	{
		return vk::Format(state.targetFormat[index]).isSRGBformat();
	}

	void PixelRoutine::readPixel(int index, Pointer<Byte> &cBuffer, Int &x, Vector4s &pixel)
	{
		Short4 c01;
		Short4 c23;
		Pointer<Byte> buffer;
		Pointer<Byte> buffer2;

		switch(state.targetFormat[index])
		{
		case VK_FORMAT_R5G6B5_UNORM_PACK16:
			buffer = cBuffer + 2 * x;
			buffer2 = buffer + *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));
			c01 = As<Short4>(Int2(*Pointer<Int>(buffer), *Pointer<Int>(buffer2)));

			pixel.x = c01 & Short4(0xF800u);
			pixel.y = (c01 & Short4(0x07E0u)) << 5;
			pixel.z = (c01 & Short4(0x001Fu)) << 11;
			pixel.w = Short4(0xFFFFu);
			break;
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SRGB:
			buffer = cBuffer + 4 * x;
			c01 = *Pointer<Short4>(buffer);
			buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));
			c23 = *Pointer<Short4>(buffer);
			pixel.z = c01;
			pixel.y = c01;
			pixel.z = UnpackLow(As<Byte8>(pixel.z), As<Byte8>(c23));
			pixel.y = UnpackHigh(As<Byte8>(pixel.y), As<Byte8>(c23));
			pixel.x = pixel.z;
			pixel.z = UnpackLow(As<Byte8>(pixel.z), As<Byte8>(pixel.y));
			pixel.x = UnpackHigh(As<Byte8>(pixel.x), As<Byte8>(pixel.y));
			pixel.y = pixel.z;
			pixel.w = pixel.x;
			pixel.x = UnpackLow(As<Byte8>(pixel.x), As<Byte8>(pixel.x));
			pixel.y = UnpackHigh(As<Byte8>(pixel.y), As<Byte8>(pixel.y));
			pixel.z = UnpackLow(As<Byte8>(pixel.z), As<Byte8>(pixel.z));
			pixel.w = UnpackHigh(As<Byte8>(pixel.w), As<Byte8>(pixel.w));
			break;
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:
			buffer = cBuffer + 4 * x;
			c01 = *Pointer<Short4>(buffer);
			buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));
			c23 = *Pointer<Short4>(buffer);
			pixel.z = c01;
			pixel.y = c01;
			pixel.z = UnpackLow(As<Byte8>(pixel.z), As<Byte8>(c23));
			pixel.y = UnpackHigh(As<Byte8>(pixel.y), As<Byte8>(c23));
			pixel.x = pixel.z;
			pixel.z = UnpackLow(As<Byte8>(pixel.z), As<Byte8>(pixel.y));
			pixel.x = UnpackHigh(As<Byte8>(pixel.x), As<Byte8>(pixel.y));
			pixel.y = pixel.z;
			pixel.w = pixel.x;
			pixel.x = UnpackLow(As<Byte8>(pixel.z), As<Byte8>(pixel.z));
			pixel.y = UnpackHigh(As<Byte8>(pixel.y), As<Byte8>(pixel.y));
			pixel.z = UnpackLow(As<Byte8>(pixel.w), As<Byte8>(pixel.w));
			pixel.w = UnpackHigh(As<Byte8>(pixel.w), As<Byte8>(pixel.w));
			break;
		case VK_FORMAT_R8_UNORM:
			buffer = cBuffer + 1 * x;
			pixel.x = Insert(pixel.x, *Pointer<Short>(buffer), 0);
			buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));
			pixel.x = Insert(pixel.x, *Pointer<Short>(buffer), 1);
			pixel.x = UnpackLow(As<Byte8>(pixel.x), As<Byte8>(pixel.x));
			pixel.y = Short4(0x0000);
			pixel.z = Short4(0x0000);
			pixel.w = Short4(0xFFFFu);
			break;
		case VK_FORMAT_R8G8_UNORM:
			buffer = cBuffer + 2 * x;
			c01 = As<Short4>(Insert(As<Int2>(c01), *Pointer<Int>(buffer), 0));
			buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));
			c01 = As<Short4>(Insert(As<Int2>(c01), *Pointer<Int>(buffer), 1));
			pixel.x = (c01 & Short4(0x00FFu)) | (c01 << 8);
			pixel.y = (c01 & Short4(0xFF00u)) | As<Short4>(As<UShort4>(c01) >> 8);
			pixel.z = Short4(0x0000u);
			pixel.w = Short4(0xFFFFu);
			break;
		case VK_FORMAT_R16G16B16A16_UNORM:
			buffer = cBuffer;
			pixel.x = *Pointer<Short4>(buffer + 8 * x);
			pixel.y = *Pointer<Short4>(buffer + 8 * x + 8);
			buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));
			pixel.z = *Pointer<Short4>(buffer + 8 * x);
			pixel.w = *Pointer<Short4>(buffer + 8 * x + 8);
			transpose4x4(pixel.x, pixel.y, pixel.z, pixel.w);
			break;
		case VK_FORMAT_R16G16_UNORM:
			buffer = cBuffer;
			pixel.x = *Pointer<Short4>(buffer + 4 * x);
			buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));
			pixel.y = *Pointer<Short4>(buffer + 4 * x);
			pixel.z = pixel.x;
			pixel.x = As<Short4>(UnpackLow(pixel.x, pixel.y));
			pixel.z = As<Short4>(UnpackHigh(pixel.z, pixel.y));
			pixel.y = pixel.z;
			pixel.x = As<Short4>(UnpackLow(pixel.x, pixel.z));
			pixel.y = As<Short4>(UnpackHigh(pixel.y, pixel.z));
			pixel.z = Short4(0xFFFFu);
			pixel.w = Short4(0xFFFFu);
			break;
		default:
			UNIMPLEMENTED("VkFormat %d", state.targetFormat[index]);
		}

		if(postBlendSRGB || isSRGB(index))
		{
			sRGBtoLinear16_12_16(pixel);
		}
	}

	void PixelRoutine::alphaBlend(int index, Pointer<Byte> &cBuffer, Vector4s &current, Int &x)
	{
		if(!state.alphaBlendActive)
		{
			return;
		}

		Vector4s pixel;
		readPixel(index, cBuffer, x, pixel);

		// Final Color = ObjectColor * SourceBlendFactor + PixelColor * DestinationBlendFactor
		Vector4s sourceFactor;
		Vector4s destFactor;

		blendFactor(sourceFactor, current, pixel, state.sourceBlendFactor);
		blendFactor(destFactor, current, pixel, state.destBlendFactor);

		if(state.sourceBlendFactor != VK_BLEND_FACTOR_ONE && state.sourceBlendFactor != VK_BLEND_FACTOR_ZERO)
		{
			current.x = MulHigh(As<UShort4>(current.x), As<UShort4>(sourceFactor.x));
			current.y = MulHigh(As<UShort4>(current.y), As<UShort4>(sourceFactor.y));
			current.z = MulHigh(As<UShort4>(current.z), As<UShort4>(sourceFactor.z));
		}

		if(state.destBlendFactor != VK_BLEND_FACTOR_ONE && state.destBlendFactor != VK_BLEND_FACTOR_ZERO)
		{
			pixel.x = MulHigh(As<UShort4>(pixel.x), As<UShort4>(destFactor.x));
			pixel.y = MulHigh(As<UShort4>(pixel.y), As<UShort4>(destFactor.y));
			pixel.z = MulHigh(As<UShort4>(pixel.z), As<UShort4>(destFactor.z));
		}

		switch(state.blendOperation)
		{
		case VK_BLEND_OP_ADD:
			current.x = AddSat(As<UShort4>(current.x), As<UShort4>(pixel.x));
			current.y = AddSat(As<UShort4>(current.y), As<UShort4>(pixel.y));
			current.z = AddSat(As<UShort4>(current.z), As<UShort4>(pixel.z));
			break;
		case VK_BLEND_OP_SUBTRACT:
			current.x = SubSat(As<UShort4>(current.x), As<UShort4>(pixel.x));
			current.y = SubSat(As<UShort4>(current.y), As<UShort4>(pixel.y));
			current.z = SubSat(As<UShort4>(current.z), As<UShort4>(pixel.z));
			break;
		case VK_BLEND_OP_REVERSE_SUBTRACT:
			current.x = SubSat(As<UShort4>(pixel.x), As<UShort4>(current.x));
			current.y = SubSat(As<UShort4>(pixel.y), As<UShort4>(current.y));
			current.z = SubSat(As<UShort4>(pixel.z), As<UShort4>(current.z));
			break;
		case VK_BLEND_OP_MIN:
			current.x = Min(As<UShort4>(current.x), As<UShort4>(pixel.x));
			current.y = Min(As<UShort4>(current.y), As<UShort4>(pixel.y));
			current.z = Min(As<UShort4>(current.z), As<UShort4>(pixel.z));
			break;
		case VK_BLEND_OP_MAX:
			current.x = Max(As<UShort4>(current.x), As<UShort4>(pixel.x));
			current.y = Max(As<UShort4>(current.y), As<UShort4>(pixel.y));
			current.z = Max(As<UShort4>(current.z), As<UShort4>(pixel.z));
			break;
		case VK_BLEND_OP_SRC_EXT:
			// No operation
			break;
		case VK_BLEND_OP_DST_EXT:
			current.x = pixel.x;
			current.y = pixel.y;
			current.z = pixel.z;
			break;
		case VK_BLEND_OP_ZERO_EXT:
			current.x = Short4(0x0000);
			current.y = Short4(0x0000);
			current.z = Short4(0x0000);
			break;
		default:
			UNIMPLEMENTED("VkBlendOp: %d", int(state.blendOperation));
		}

		blendFactorAlpha(sourceFactor, current, pixel, state.sourceBlendFactorAlpha);
		blendFactorAlpha(destFactor, current, pixel, state.destBlendFactorAlpha);

		if(state.sourceBlendFactorAlpha != VK_BLEND_FACTOR_ONE && state.sourceBlendFactorAlpha != VK_BLEND_FACTOR_ZERO)
		{
			current.w = MulHigh(As<UShort4>(current.w), As<UShort4>(sourceFactor.w));
		}

		if(state.destBlendFactorAlpha != VK_BLEND_FACTOR_ONE && state.destBlendFactorAlpha != VK_BLEND_FACTOR_ZERO)
		{
			pixel.w = MulHigh(As<UShort4>(pixel.w), As<UShort4>(destFactor.w));
		}

		switch(state.blendOperationAlpha)
		{
		case VK_BLEND_OP_ADD:
			current.w = AddSat(As<UShort4>(current.w), As<UShort4>(pixel.w));
			break;
		case VK_BLEND_OP_SUBTRACT:
			current.w = SubSat(As<UShort4>(current.w), As<UShort4>(pixel.w));
			break;
		case VK_BLEND_OP_REVERSE_SUBTRACT:
			current.w = SubSat(As<UShort4>(pixel.w), As<UShort4>(current.w));
			break;
		case VK_BLEND_OP_MIN:
			current.w = Min(As<UShort4>(current.w), As<UShort4>(pixel.w));
			break;
		case VK_BLEND_OP_MAX:
			current.w = Max(As<UShort4>(current.w), As<UShort4>(pixel.w));
			break;
		case VK_BLEND_OP_SRC_EXT:
			// No operation
			break;
		case VK_BLEND_OP_DST_EXT:
			current.w = pixel.w;
			break;
		case VK_BLEND_OP_ZERO_EXT:
			current.w = Short4(0x0000);
			break;
		default:
			UNIMPLEMENTED("VkBlendOp: %d", int(state.blendOperationAlpha));
		}
	}

	void PixelRoutine::writeColor(int index, Pointer<Byte> &cBuffer, Int &x, Vector4s &current, Int &sMask, Int &zMask, Int &cMask)
	{
		if(postBlendSRGB || isSRGB(index))
		{
			linearToSRGB16_12_16(current);
		}

		if(exactColorRounding)
		{
			switch(state.targetFormat[index])
			{
			case VK_FORMAT_R5G6B5_UNORM_PACK16:
				current.x = AddSat(As<UShort4>(current.x), UShort4(0x0400));
				current.y = AddSat(As<UShort4>(current.y), UShort4(0x0200));
				current.z = AddSat(As<UShort4>(current.z), UShort4(0x0400));
				break;
			case VK_FORMAT_B8G8R8A8_UNORM:
			case VK_FORMAT_B8G8R8A8_SRGB:
			case VK_FORMAT_R8G8B8A8_UNORM:
			case VK_FORMAT_R8G8B8A8_SRGB:
			case VK_FORMAT_R8G8_UNORM:
			case VK_FORMAT_R8_UNORM:
			case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
			case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
				current.x = current.x - As<Short4>(As<UShort4>(current.x) >> 8) + Short4(0x0080);
				current.y = current.y - As<Short4>(As<UShort4>(current.y) >> 8) + Short4(0x0080);
				current.z = current.z - As<Short4>(As<UShort4>(current.z) >> 8) + Short4(0x0080);
				current.w = current.w - As<Short4>(As<UShort4>(current.w) >> 8) + Short4(0x0080);
				break;
			default:
				break;
			}
		}

		int rgbaWriteMask = state.colorWriteActive(index);
		int bgraWriteMask = (rgbaWriteMask & 0x0000000A) | (rgbaWriteMask & 0x00000001) << 2 | (rgbaWriteMask & 0x00000004) >> 2;

		switch(state.targetFormat[index])
		{
		case VK_FORMAT_R5G6B5_UNORM_PACK16:
			{
				current.x = current.x & Short4(0xF800u);
				current.y = As<UShort4>(current.y & Short4(0xFC00u)) >> 5;
				current.z = As<UShort4>(current.z) >> 11;

				current.x = current.x | current.y | current.z;
			}
			break;
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SRGB:
			if(rgbaWriteMask == 0x7)
			{
				current.x = As<Short4>(As<UShort4>(current.x) >> 8);
				current.y = As<Short4>(As<UShort4>(current.y) >> 8);
				current.z = As<Short4>(As<UShort4>(current.z) >> 8);

				current.z = As<Short4>(PackUnsigned(current.z, current.x));
				current.y = As<Short4>(PackUnsigned(current.y, current.y));

				current.x = current.z;
				current.z = UnpackLow(As<Byte8>(current.z), As<Byte8>(current.y));
				current.x = UnpackHigh(As<Byte8>(current.x), As<Byte8>(current.y));
				current.y = current.z;
				current.z = As<Short4>(UnpackLow(current.z, current.x));
				current.y = As<Short4>(UnpackHigh(current.y, current.x));
			}
			else
			{
				current.x = As<Short4>(As<UShort4>(current.x) >> 8);
				current.y = As<Short4>(As<UShort4>(current.y) >> 8);
				current.z = As<Short4>(As<UShort4>(current.z) >> 8);
				current.w = As<Short4>(As<UShort4>(current.w) >> 8);

				current.z = As<Short4>(PackUnsigned(current.z, current.x));
				current.y = As<Short4>(PackUnsigned(current.y, current.w));

				current.x = current.z;
				current.z = UnpackLow(As<Byte8>(current.z), As<Byte8>(current.y));
				current.x = UnpackHigh(As<Byte8>(current.x), As<Byte8>(current.y));
				current.y = current.z;
				current.z = As<Short4>(UnpackLow(current.z, current.x));
				current.y = As<Short4>(UnpackHigh(current.y, current.x));
			}
			break;
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
			if(rgbaWriteMask == 0x7)
			{
				current.x = As<Short4>(As<UShort4>(current.x) >> 8);
				current.y = As<Short4>(As<UShort4>(current.y) >> 8);
				current.z = As<Short4>(As<UShort4>(current.z) >> 8);

				current.z = As<Short4>(PackUnsigned(current.x, current.z));
				current.y = As<Short4>(PackUnsigned(current.y, current.y));

				current.x = current.z;
				current.z = UnpackLow(As<Byte8>(current.z), As<Byte8>(current.y));
				current.x = UnpackHigh(As<Byte8>(current.x), As<Byte8>(current.y));
				current.y = current.z;
				current.z = As<Short4>(UnpackLow(current.z, current.x));
				current.y = As<Short4>(UnpackHigh(current.y, current.x));
			}
			else
			{
				current.x = As<Short4>(As<UShort4>(current.x) >> 8);
				current.y = As<Short4>(As<UShort4>(current.y) >> 8);
				current.z = As<Short4>(As<UShort4>(current.z) >> 8);
				current.w = As<Short4>(As<UShort4>(current.w) >> 8);

				current.z = As<Short4>(PackUnsigned(current.x, current.z));
				current.y = As<Short4>(PackUnsigned(current.y, current.w));

				current.x = current.z;
				current.z = UnpackLow(As<Byte8>(current.z), As<Byte8>(current.y));
				current.x = UnpackHigh(As<Byte8>(current.x), As<Byte8>(current.y));
				current.y = current.z;
				current.z = As<Short4>(UnpackLow(current.z, current.x));
				current.y = As<Short4>(UnpackHigh(current.y, current.x));
			}
			break;
		case VK_FORMAT_R8G8_UNORM:
			current.x = As<Short4>(As<UShort4>(current.x) >> 8);
			current.y = As<Short4>(As<UShort4>(current.y) >> 8);
			current.x = As<Short4>(PackUnsigned(current.x, current.x));
			current.y = As<Short4>(PackUnsigned(current.y, current.y));
			current.x = UnpackLow(As<Byte8>(current.x), As<Byte8>(current.y));
			break;
		case VK_FORMAT_R8_UNORM:
			current.x = As<Short4>(As<UShort4>(current.x) >> 8);
			current.x = As<Short4>(PackUnsigned(current.x, current.x));
			break;
		case VK_FORMAT_R16G16_UNORM:
			current.z = current.x;
			current.x = As<Short4>(UnpackLow(current.x, current.y));
			current.z = As<Short4>(UnpackHigh(current.z, current.y));
			current.y = current.z;
			break;
		case VK_FORMAT_R16G16B16A16_UNORM:
			transpose4x4(current.x, current.y, current.z, current.w);
			break;
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		{
			auto r = Int4(current.x) & Int4(0x3ff);
			auto g = Int4(current.y) & Int4(0x3ff);
			auto b = Int4(current.z) & Int4(0x3ff);
			auto a = Int4(current.w) & Int4(0x3);
			Int4 packed = (a << 30) | (b << 20) | (g << 10) | r;
			auto c02 = As<Int2>(Int4(packed.xzzz)); // TODO: auto c02 = packed.xz;
			auto c13 = As<Int2>(Int4(packed.ywww)); // TODO: auto c13 = packed.yw;
			current.x = UnpackLow(c02, c13);
			current.y = UnpackHigh(c02, c13);
			break;
		}
		default:
			UNIMPLEMENTED("VkFormat: %d", int(state.targetFormat[index]));
		}

		Short4 c01 = current.z;
		Short4 c23 = current.y;

		Int xMask;   // Combination of all masks

		if(state.depthTestActive)
		{
			xMask = zMask;
		}
		else
		{
			xMask = cMask;
		}

		if(state.stencilActive)
		{
			xMask &= sMask;
		}

		switch(state.targetFormat[index])
		{
		case VK_FORMAT_R5G6B5_UNORM_PACK16:
			{
				Pointer<Byte> buffer = cBuffer + 2 * x;
				Int value = *Pointer<Int>(buffer);

				Int c01 = Extract(As<Int2>(current.x), 0);

				if((bgraWriteMask & 0x00000007) != 0x00000007)
				{
					Int masked = value;
					c01 &= *Pointer<Int>(constants + OFFSET(Constants,mask565Q[bgraWriteMask & 0x7][0]));
					masked &= *Pointer<Int>(constants + OFFSET(Constants,mask565Q[~bgraWriteMask & 0x7][0]));
					c01 |= masked;
				}

				c01 &= *Pointer<Int>(constants + OFFSET(Constants,maskW4Q[0][0]) + xMask * 8);
				value &= *Pointer<Int>(constants + OFFSET(Constants,invMaskW4Q[0][0]) + xMask * 8);
				c01 |= value;
				*Pointer<Int>(buffer) = c01;

				buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));
				value = *Pointer<Int>(buffer);

				Int c23 = Extract(As<Int2>(current.x), 1);

				if((bgraWriteMask & 0x00000007) != 0x00000007)
				{
					Int masked = value;
					c23 &= *Pointer<Int>(constants + OFFSET(Constants,mask565Q[bgraWriteMask & 0x7][0]));
					masked &= *Pointer<Int>(constants + OFFSET(Constants,mask565Q[~bgraWriteMask & 0x7][0]));
					c23 |= masked;
				}

				c23 &= *Pointer<Int>(constants + OFFSET(Constants,maskW4Q[0][2]) + xMask * 8);
				value &= *Pointer<Int>(constants + OFFSET(Constants,invMaskW4Q[0][2]) + xMask * 8);
				c23 |= value;
				*Pointer<Int>(buffer) = c23;
			}
			break;
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SRGB:
			{
				Pointer<Byte> buffer = cBuffer + x * 4;
				Short4 value = *Pointer<Short4>(buffer);

				if(bgraWriteMask != 0x0000000F)   // FIXME: Need for masking when XRGB && Fh?
				{
					Short4 masked = value;
					c01 &= *Pointer<Short4>(constants + OFFSET(Constants,maskB4Q[bgraWriteMask][0]));
					masked &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskB4Q[bgraWriteMask][0]));
					c01 |= masked;
				}

				c01 &= *Pointer<Short4>(constants + OFFSET(Constants,maskD01Q) + xMask * 8);
				value &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskD01Q) + xMask * 8);
				c01 |= value;
				*Pointer<Short4>(buffer) = c01;

				buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));
				value = *Pointer<Short4>(buffer);

				if(bgraWriteMask != 0x0000000F)   // FIXME: Need for masking when XRGB && Fh?
				{
					Short4 masked = value;
					c23 &= *Pointer<Short4>(constants + OFFSET(Constants,maskB4Q[bgraWriteMask][0]));
					masked &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskB4Q[bgraWriteMask][0]));
					c23 |= masked;
				}

				c23 &= *Pointer<Short4>(constants + OFFSET(Constants,maskD23Q) + xMask * 8);
				value &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskD23Q) + xMask * 8);
				c23 |= value;
				*Pointer<Short4>(buffer) = c23;
			}
			break;
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
			{
				Pointer<Byte> buffer = cBuffer + x * 4;
				Short4 value = *Pointer<Short4>(buffer);

				bool masked = (rgbaWriteMask != 0x0000000F); // FIXME: Need for masking when XBGR && Fh?

				if(masked)
				{
					Short4 masked = value;
					c01 &= *Pointer<Short4>(constants + OFFSET(Constants,maskB4Q[rgbaWriteMask][0]));
					masked &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskB4Q[rgbaWriteMask][0]));
					c01 |= masked;
				}

				c01 &= *Pointer<Short4>(constants + OFFSET(Constants,maskD01Q) + xMask * 8);
				value &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskD01Q) + xMask * 8);
				c01 |= value;
				*Pointer<Short4>(buffer) = c01;

				buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));
				value = *Pointer<Short4>(buffer);

				if(masked)
				{
					Short4 masked = value;
					c23 &= *Pointer<Short4>(constants + OFFSET(Constants,maskB4Q[rgbaWriteMask][0]));
					masked &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskB4Q[rgbaWriteMask][0]));
					c23 |= masked;
				}

				c23 &= *Pointer<Short4>(constants + OFFSET(Constants,maskD23Q) + xMask * 8);
				value &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskD23Q) + xMask * 8);
				c23 |= value;
				*Pointer<Short4>(buffer) = c23;
			}
			break;
		case VK_FORMAT_R8G8_UNORM:
			if((rgbaWriteMask & 0x00000003) != 0x0)
			{
				Pointer<Byte> buffer = cBuffer + 2 * x;
				Int2 value;
				value = Insert(value, *Pointer<Int>(buffer), 0);
				Int pitch = *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));
				value = Insert(value, *Pointer<Int>(buffer + pitch), 1);

				Int2 packedCol = As<Int2>(current.x);

				UInt2 mergedMask = *Pointer<UInt2>(constants + OFFSET(Constants, maskW4Q) + xMask * 8);
				if((rgbaWriteMask & 0x3) != 0x3)
				{
					Int tmpMask = *Pointer<Int>(constants + OFFSET(Constants, maskB4Q[5 * (rgbaWriteMask & 0x3)][0]));
					UInt2 rgbaMask = As<UInt2>(Int2(tmpMask, tmpMask));
					mergedMask &= rgbaMask;
				}

				packedCol = As<Int2>((As<UInt2>(packedCol) & mergedMask) | (As<UInt2>(value) & ~mergedMask));

				*Pointer<UInt>(buffer) = As<UInt>(Extract(packedCol, 0));
				*Pointer<UInt>(buffer + pitch) = As<UInt>(Extract(packedCol, 1));
			}
			break;
		case VK_FORMAT_R8_UNORM:
			if(rgbaWriteMask & 0x00000001)
			{
				Pointer<Byte> buffer = cBuffer + 1 * x;
				Short4 value;
				value = Insert(value, *Pointer<Short>(buffer), 0);
				Int pitch = *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));
				value = Insert(value, *Pointer<Short>(buffer + pitch), 1);

				current.x &= *Pointer<Short4>(constants + OFFSET(Constants, maskB4Q) + 8 * xMask);
				value &= *Pointer<Short4>(constants + OFFSET(Constants, invMaskB4Q) + 8 * xMask);
				current.x |= value;

				*Pointer<Short>(buffer) = Extract(current.x, 0);
				*Pointer<Short>(buffer + pitch) = Extract(current.x, 1);
			}
			break;
		case VK_FORMAT_R16G16_UNORM:
			{
				Pointer<Byte> buffer = cBuffer + 4 * x;

				Short4 value = *Pointer<Short4>(buffer);

				if((rgbaWriteMask & 0x00000003) != 0x00000003)
				{
					Short4 masked = value;
					current.x &= *Pointer<Short4>(constants + OFFSET(Constants,maskW01Q[rgbaWriteMask & 0x3][0]));
					masked &= *Pointer<Short4>(constants + OFFSET(Constants,maskW01Q[~rgbaWriteMask & 0x3][0]));
					current.x |= masked;
				}

				current.x &= *Pointer<Short4>(constants + OFFSET(Constants,maskD01Q) + xMask * 8);
				value &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskD01Q) + xMask * 8);
				current.x |= value;
				*Pointer<Short4>(buffer) = current.x;

				buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));

				value = *Pointer<Short4>(buffer);

				if((rgbaWriteMask & 0x00000003) != 0x00000003)
				{
					Short4 masked = value;
					current.y &= *Pointer<Short4>(constants + OFFSET(Constants,maskW01Q[rgbaWriteMask & 0x3][0]));
					masked &= *Pointer<Short4>(constants + OFFSET(Constants,maskW01Q[~rgbaWriteMask & 0x3][0]));
					current.y |= masked;
				}

				current.y &= *Pointer<Short4>(constants + OFFSET(Constants,maskD23Q) + xMask * 8);
				value &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskD23Q) + xMask * 8);
				current.y |= value;
				*Pointer<Short4>(buffer) = current.y;
			}
			break;
		case VK_FORMAT_R16G16B16A16_UNORM:
			{
				Pointer<Byte> buffer = cBuffer + 8 * x;

				{
					Short4 value = *Pointer<Short4>(buffer);

					if(rgbaWriteMask != 0x0000000F)
					{
						Short4 masked = value;
						current.x &= *Pointer<Short4>(constants + OFFSET(Constants,maskW4Q[rgbaWriteMask][0]));
						masked &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskW4Q[rgbaWriteMask][0]));
						current.x |= masked;
					}

					current.x &= *Pointer<Short4>(constants + OFFSET(Constants,maskQ0Q) + xMask * 8);
					value &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskQ0Q) + xMask * 8);
					current.x |= value;
					*Pointer<Short4>(buffer) = current.x;
				}

				{
					Short4 value = *Pointer<Short4>(buffer + 8);

					if(rgbaWriteMask != 0x0000000F)
					{
						Short4 masked = value;
						current.y &= *Pointer<Short4>(constants + OFFSET(Constants,maskW4Q[rgbaWriteMask][0]));
						masked &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskW4Q[rgbaWriteMask][0]));
						current.y |= masked;
					}

					current.y &= *Pointer<Short4>(constants + OFFSET(Constants,maskQ1Q) + xMask * 8);
					value &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskQ1Q) + xMask * 8);
					current.y |= value;
					*Pointer<Short4>(buffer + 8) = current.y;
				}

				buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));

				{
					Short4 value = *Pointer<Short4>(buffer);

					if(rgbaWriteMask != 0x0000000F)
					{
						Short4 masked = value;
						current.z &= *Pointer<Short4>(constants + OFFSET(Constants,maskW4Q[rgbaWriteMask][0]));
						masked &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskW4Q[rgbaWriteMask][0]));
						current.z |= masked;
					}

					current.z &= *Pointer<Short4>(constants + OFFSET(Constants,maskQ2Q) + xMask * 8);
					value &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskQ2Q) + xMask * 8);
					current.z |= value;
					*Pointer<Short4>(buffer) = current.z;
				}

				{
					Short4 value = *Pointer<Short4>(buffer + 8);

					if(rgbaWriteMask != 0x0000000F)
					{
						Short4 masked = value;
						current.w &= *Pointer<Short4>(constants + OFFSET(Constants,maskW4Q[rgbaWriteMask][0]));
						masked &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskW4Q[rgbaWriteMask][0]));
						current.w |= masked;
					}

					current.w &= *Pointer<Short4>(constants + OFFSET(Constants,maskQ3Q) + xMask * 8);
					value &= *Pointer<Short4>(constants + OFFSET(Constants,invMaskQ3Q) + xMask * 8);
					current.w |= value;
					*Pointer<Short4>(buffer + 8) = current.w;
				}
			}
			break;
			case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
			{
				Pointer<Byte> buffer = cBuffer + 4 * x;

				buffer = cBuffer + 4 * x;
				Int2 value = *Pointer<Int2>(buffer, 16);
				Int2 mergedMask = *Pointer<Int2>(constants + OFFSET(Constants, maskD01Q) + xMask * 8);
				if (rgbaWriteMask != 0xF)
				{
					mergedMask &= *Pointer<Int2>(constants + OFFSET(Constants, mask10Q[rgbaWriteMask][0]));
				}
				*Pointer<Int2>(buffer) = (As<Int2>(current.x) & mergedMask) | (value & ~mergedMask);

				buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));

				value = *Pointer<Int2>(buffer, 16);
				mergedMask = *Pointer<Int2>(constants + OFFSET(Constants, maskD23Q) + xMask * 8);
				if (rgbaWriteMask != 0xF)
				{
					mergedMask &= *Pointer<Int2>(constants + OFFSET(Constants, mask10Q[rgbaWriteMask][0]));
				}
				*Pointer<Int2>(buffer) = (As<Int2>(current.y) & mergedMask) | (value & ~mergedMask);
			}
			break;
		default:
			UNIMPLEMENTED("VkFormat: %d", int(state.targetFormat[index]));
		}
	}

	void PixelRoutine::blendFactor(Vector4f &blendFactor, const Vector4f &oC, const Vector4f &pixel, VkBlendFactor blendFactorActive)
	{
		switch(blendFactorActive)
		{
		case VK_BLEND_FACTOR_ZERO:
			blendFactor.x = Float4(0);
			blendFactor.y = Float4(0);
			blendFactor.z = Float4(0);
			break;
		case VK_BLEND_FACTOR_ONE:
			blendFactor.x = Float4(1);
			blendFactor.y = Float4(1);
			blendFactor.z = Float4(1);
			break;
		case VK_BLEND_FACTOR_SRC_COLOR:
			blendFactor.x = oC.x;
			blendFactor.y = oC.y;
			blendFactor.z = oC.z;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
			blendFactor.x = Float4(1.0f) - oC.x;
			blendFactor.y = Float4(1.0f) - oC.y;
			blendFactor.z = Float4(1.0f) - oC.z;
			break;
		case VK_BLEND_FACTOR_DST_COLOR:
			blendFactor.x = pixel.x;
			blendFactor.y = pixel.y;
			blendFactor.z = pixel.z;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
			blendFactor.x = Float4(1.0f) - pixel.x;
			blendFactor.y = Float4(1.0f) - pixel.y;
			blendFactor.z = Float4(1.0f) - pixel.z;
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA:
			blendFactor.x = oC.w;
			blendFactor.y = oC.w;
			blendFactor.z = oC.w;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
			blendFactor.x = Float4(1.0f) - oC.w;
			blendFactor.y = Float4(1.0f) - oC.w;
			blendFactor.z = Float4(1.0f) - oC.w;
			break;
		case VK_BLEND_FACTOR_DST_ALPHA:
			blendFactor.x = pixel.w;
			blendFactor.y = pixel.w;
			blendFactor.z = pixel.w;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
			blendFactor.x = Float4(1.0f) - pixel.w;
			blendFactor.y = Float4(1.0f) - pixel.w;
			blendFactor.z = Float4(1.0f) - pixel.w;
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
			blendFactor.x = Float4(1.0f) - pixel.w;
			blendFactor.x = Min(blendFactor.x, oC.w);
			blendFactor.y = blendFactor.x;
			blendFactor.z = blendFactor.x;
			break;
		case VK_BLEND_FACTOR_CONSTANT_COLOR:
			blendFactor.x = *Pointer<Float4>(data + OFFSET(DrawData,factor.blendConstant4F[0]));
			blendFactor.y = *Pointer<Float4>(data + OFFSET(DrawData,factor.blendConstant4F[1]));
			blendFactor.z = *Pointer<Float4>(data + OFFSET(DrawData,factor.blendConstant4F[2]));
			break;
		case VK_BLEND_FACTOR_CONSTANT_ALPHA:
			blendFactor.x = *Pointer<Float4>(data + OFFSET(DrawData,factor.blendConstant4F[3]));
			blendFactor.y = *Pointer<Float4>(data + OFFSET(DrawData,factor.blendConstant4F[3]));
			blendFactor.z = *Pointer<Float4>(data + OFFSET(DrawData,factor.blendConstant4F[3]));
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
			blendFactor.x = *Pointer<Float4>(data + OFFSET(DrawData,factor.invBlendConstant4F[0]));
			blendFactor.y = *Pointer<Float4>(data + OFFSET(DrawData,factor.invBlendConstant4F[1]));
			blendFactor.z = *Pointer<Float4>(data + OFFSET(DrawData,factor.invBlendConstant4F[2]));
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
			blendFactor.x = *Pointer<Float4>(data + OFFSET(DrawData,factor.invBlendConstant4F[3]));
			blendFactor.y = *Pointer<Float4>(data + OFFSET(DrawData,factor.invBlendConstant4F[3]));
			blendFactor.z = *Pointer<Float4>(data + OFFSET(DrawData,factor.invBlendConstant4F[3]));
			break;

		default:
			UNIMPLEMENTED("VkBlendFactor: %d", int(blendFactorActive));
		}
	}

	void PixelRoutine::blendFactorAlpha(Vector4f &blendFactor, const Vector4f &oC, const Vector4f &pixel, VkBlendFactor blendFactorAlphaActive)
	{
		switch(blendFactorAlphaActive)
		{
		case VK_BLEND_FACTOR_ZERO:
			blendFactor.w = Float4(0);
			break;
		case VK_BLEND_FACTOR_ONE:
			blendFactor.w = Float4(1);
			break;
		case VK_BLEND_FACTOR_SRC_COLOR:
			blendFactor.w = oC.w;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
			blendFactor.w = Float4(1.0f) - oC.w;
			break;
		case VK_BLEND_FACTOR_DST_COLOR:
			blendFactor.w = pixel.w;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
			blendFactor.w = Float4(1.0f) - pixel.w;
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA:
			blendFactor.w = oC.w;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
			blendFactor.w = Float4(1.0f) - oC.w;
			break;
		case VK_BLEND_FACTOR_DST_ALPHA:
			blendFactor.w = pixel.w;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
			blendFactor.w = Float4(1.0f) - pixel.w;
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
			blendFactor.w = Float4(1.0f);
			break;
		case VK_BLEND_FACTOR_CONSTANT_COLOR:
		case VK_BLEND_FACTOR_CONSTANT_ALPHA:
			blendFactor.w = *Pointer<Float4>(data + OFFSET(DrawData,factor.blendConstant4F[3]));
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
		case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
			blendFactor.w = *Pointer<Float4>(data + OFFSET(DrawData,factor.invBlendConstant4F[3]));
			break;
		default:
			UNIMPLEMENTED("VkBlendFactor: %d", int(blendFactorAlphaActive));
		}
	}

	void PixelRoutine::alphaBlend(int index, Pointer<Byte> &cBuffer, Vector4f &oC, Int &x)
	{
		if(!state.alphaBlendActive)
		{
			return;
		}

		Pointer<Byte> buffer;

		// pixel holds four texel color values.
		// Note: Despite the type being Vector4f, the colors may be stored as
		// integers. Half-floats are stored as full 32-bit floats.
		// Non-float and non-fixed point formats are not alpha blended.
		Vector4f pixel;

		Vector4s color;
		Short4 c01;
		Short4 c23;

		Float4 one;
		vk::Format format(state.targetFormat[index]);
		if(format.isFloatFormat())
		{
			one = Float4(1.0f);
		}
		else if(format.isNonNormalizedInteger())
		{
			one = As<Float4>(format.isUnsignedComponent(0) ? Int4(0xFFFFFFFF) : Int4(0x7FFFFFFF));
		}

		switch(state.targetFormat[index])
		{
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SFLOAT:
			buffer = cBuffer;
			// FIXME: movlps
			pixel.x.x = *Pointer<Float>(buffer + 4 * x + 0);
			pixel.x.y = *Pointer<Float>(buffer + 4 * x + 4);
			buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));
			// FIXME: movhps
			pixel.x.z = *Pointer<Float>(buffer + 4 * x + 0);
			pixel.x.w = *Pointer<Float>(buffer + 4 * x + 4);
			pixel.y = pixel.z = pixel.w = one;
			break;
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R32G32_SFLOAT:
			buffer = cBuffer;
			pixel.x = *Pointer<Float4>(buffer + 8 * x, 16);
			buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));
			pixel.y = *Pointer<Float4>(buffer + 8 * x, 16);
			pixel.z = pixel.x;
			pixel.x = ShuffleLowHigh(pixel.x, pixel.y, 0x88);
			pixel.z = ShuffleLowHigh(pixel.z, pixel.y, 0xDD);
			pixel.y = pixel.z;
			pixel.z = pixel.w = one;
			break;
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_UINT:
			buffer = cBuffer;
			pixel.x = *Pointer<Float4>(buffer + 16 * x, 16);
			pixel.y = *Pointer<Float4>(buffer + 16 * x + 16, 16);
			buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));
			pixel.z = *Pointer<Float4>(buffer + 16 * x, 16);
			pixel.w = *Pointer<Float4>(buffer + 16 * x + 16, 16);
			transpose4x4(pixel.x, pixel.y, pixel.z, pixel.w);
			break;
		case VK_FORMAT_R16_SFLOAT:
			buffer = cBuffer;
			pixel.x.x = Float(*Pointer<Half>(buffer + 2 * x + 0));
			pixel.x.y = Float(*Pointer<Half>(buffer + 2 * x + 2));
			buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));
			pixel.x.z = Float(*Pointer<Half>(buffer + 2 * x + 0));
			pixel.x.w = Float(*Pointer<Half>(buffer + 2 * x + 2));
			pixel.y = pixel.z = pixel.w = one;
			break;
		case VK_FORMAT_R16G16_SFLOAT:
			buffer = cBuffer;
			pixel.x.x = Float(*Pointer<Half>(buffer + 4 * x + 0));
			pixel.y.x = Float(*Pointer<Half>(buffer + 4 * x + 2));
			pixel.x.y = Float(*Pointer<Half>(buffer + 4 * x + 4));
			pixel.y.y = Float(*Pointer<Half>(buffer + 4 * x + 6));
			buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));
			pixel.x.z = Float(*Pointer<Half>(buffer + 4 * x + 0));
			pixel.y.z = Float(*Pointer<Half>(buffer + 4 * x + 2));
			pixel.x.w = Float(*Pointer<Half>(buffer + 4 * x + 4));
			pixel.y.w = Float(*Pointer<Half>(buffer + 4 * x + 6));
			pixel.z = pixel.w = one;
			break;
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			buffer = cBuffer;
			pixel.x.x = Float(*Pointer<Half>(buffer + 8 * x + 0x0));
			pixel.y.x = Float(*Pointer<Half>(buffer + 8 * x + 0x2));
			pixel.z.x = Float(*Pointer<Half>(buffer + 8 * x + 0x4));
			pixel.w.x = Float(*Pointer<Half>(buffer + 8 * x + 0x6));
			pixel.x.y = Float(*Pointer<Half>(buffer + 8 * x + 0x8));
			pixel.y.y = Float(*Pointer<Half>(buffer + 8 * x + 0xa));
			pixel.z.y = Float(*Pointer<Half>(buffer + 8 * x + 0xc));
			pixel.w.y = Float(*Pointer<Half>(buffer + 8 * x + 0xe));
			buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));
			pixel.x.z = Float(*Pointer<Half>(buffer + 8 * x + 0x0));
			pixel.y.z = Float(*Pointer<Half>(buffer + 8 * x + 0x2));
			pixel.z.z = Float(*Pointer<Half>(buffer + 8 * x + 0x4));
			pixel.w.z = Float(*Pointer<Half>(buffer + 8 * x + 0x6));
			pixel.x.w = Float(*Pointer<Half>(buffer + 8 * x + 0x8));
			pixel.y.w = Float(*Pointer<Half>(buffer + 8 * x + 0xa));
			pixel.z.w = Float(*Pointer<Half>(buffer + 8 * x + 0xc));
			pixel.w.w = Float(*Pointer<Half>(buffer + 8 * x + 0xe));
			break;
		default:
			UNIMPLEMENTED("VkFormat: %d", int(state.targetFormat[index]));
		}

		if(postBlendSRGB || isSRGB(index))
		{
			sRGBtoLinear(pixel.x);
			sRGBtoLinear(pixel.y);
			sRGBtoLinear(pixel.z);
		}

		// Final Color = ObjectColor * SourceBlendFactor + PixelColor * DestinationBlendFactor
		Vector4f sourceFactor;
		Vector4f destFactor;

		blendFactor(sourceFactor, oC, pixel, state.sourceBlendFactor);
		blendFactor(destFactor, oC, pixel, state.destBlendFactor);

		oC.x *= sourceFactor.x;
		oC.y *= sourceFactor.y;
		oC.z *= sourceFactor.z;

		pixel.x *= destFactor.x;
		pixel.y *= destFactor.y;
		pixel.z *= destFactor.z;

		switch(state.blendOperation)
		{
		case VK_BLEND_OP_ADD:
			oC.x += pixel.x;
			oC.y += pixel.y;
			oC.z += pixel.z;
			break;
		case VK_BLEND_OP_SUBTRACT:
			oC.x -= pixel.x;
			oC.y -= pixel.y;
			oC.z -= pixel.z;
			break;
		case VK_BLEND_OP_REVERSE_SUBTRACT:
			oC.x = pixel.x - oC.x;
			oC.y = pixel.y - oC.y;
			oC.z = pixel.z - oC.z;
			break;
		case VK_BLEND_OP_MIN:
			oC.x = Min(oC.x, pixel.x);
			oC.y = Min(oC.y, pixel.y);
			oC.z = Min(oC.z, pixel.z);
			break;
		case VK_BLEND_OP_MAX:
			oC.x = Max(oC.x, pixel.x);
			oC.y = Max(oC.y, pixel.y);
			oC.z = Max(oC.z, pixel.z);
			break;
		case VK_BLEND_OP_SRC_EXT:
			// No operation
			break;
		case VK_BLEND_OP_DST_EXT:
			oC.x = pixel.x;
			oC.y = pixel.y;
			oC.z = pixel.z;
			break;
		case VK_BLEND_OP_ZERO_EXT:
			oC.x = Float4(0.0f);
			oC.y = Float4(0.0f);
			oC.z = Float4(0.0f);
			break;
		default:
			UNIMPLEMENTED("VkBlendOp: %d", int(state.blendOperation));
		}

		blendFactorAlpha(sourceFactor, oC, pixel, state.sourceBlendFactorAlpha);
		blendFactorAlpha(destFactor, oC, pixel, state.destBlendFactorAlpha);

		oC.w *= sourceFactor.w;
		pixel.w *= destFactor.w;

		switch(state.blendOperationAlpha)
		{
		case VK_BLEND_OP_ADD:
			oC.w += pixel.w;
			break;
		case VK_BLEND_OP_SUBTRACT:
			oC.w -= pixel.w;
			break;
		case VK_BLEND_OP_REVERSE_SUBTRACT:
			pixel.w -= oC.w;
			oC.w = pixel.w;
			break;
		case VK_BLEND_OP_MIN:
			oC.w = Min(oC.w, pixel.w);
			break;
		case VK_BLEND_OP_MAX:
			oC.w = Max(oC.w, pixel.w);
			break;
		case VK_BLEND_OP_SRC_EXT:
			// No operation
			break;
		case VK_BLEND_OP_DST_EXT:
			oC.w = pixel.w;
			break;
		case VK_BLEND_OP_ZERO_EXT:
			oC.w = Float4(0.0f);
			break;
		default:
			UNIMPLEMENTED("VkBlendOp: %d", int(state.blendOperationAlpha));
		}
	}

	void PixelRoutine::writeColor(int index, Pointer<Byte> &cBuffer, Int &x, Vector4f &oC, Int &sMask, Int &zMask, Int &cMask)
	{
		switch(state.targetFormat[index])
		{
		case VK_FORMAT_R16_SFLOAT:
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_A2B10G10R10_UINT_PACK32:
			break;
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R8G8_UINT:
			oC.z = oC.x;
			oC.x = UnpackLow(oC.x, oC.y);
			oC.z = UnpackHigh(oC.z, oC.y);
			oC.y = oC.z;
			break;
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_UINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R16G16B16A16_UINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_A8B8G8R8_UINT_PACK32:
		case VK_FORMAT_A8B8G8R8_SINT_PACK32:
			transpose4x4(oC.x, oC.y, oC.z, oC.w);
			break;
		default:
			UNIMPLEMENTED("VkFormat: %d", int(state.targetFormat[index]));
		}

		int rgbaWriteMask = state.colorWriteActive(index);

		Int xMask;   // Combination of all masks

		if(state.depthTestActive)
		{
			xMask = zMask;
		}
		else
		{
			xMask = cMask;
		}

		if(state.stencilActive)
		{
			xMask &= sMask;
		}

		auto targetFormat = state.targetFormat[index];

		Pointer<Byte> buffer;
		Float4 value;

		switch(targetFormat)
		{
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_UINT:
			if(rgbaWriteMask & 0x00000001)
			{
				buffer = cBuffer + 4 * x;

				// FIXME: movlps
				value.x = *Pointer<Float>(buffer + 0);
				value.y = *Pointer<Float>(buffer + 4);

				buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));

				// FIXME: movhps
				value.z = *Pointer<Float>(buffer + 0);
				value.w = *Pointer<Float>(buffer + 4);

				oC.x = As<Float4>(As<Int4>(oC.x) & *Pointer<Int4>(constants + OFFSET(Constants,maskD4X) + xMask * 16, 16));
				value = As<Float4>(As<Int4>(value) & *Pointer<Int4>(constants + OFFSET(Constants,invMaskD4X) + xMask * 16, 16));
				oC.x = As<Float4>(As<Int4>(oC.x) | As<Int4>(value));

				// FIXME: movhps
				*Pointer<Float>(buffer + 0) = oC.x.z;
				*Pointer<Float>(buffer + 4) = oC.x.w;

				buffer -= *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));

				// FIXME: movlps
				*Pointer<Float>(buffer + 0) = oC.x.x;
				*Pointer<Float>(buffer + 4) = oC.x.y;
			}
			break;
		case VK_FORMAT_R16_SFLOAT:
			if(rgbaWriteMask & 0x00000001)
			{
				buffer = cBuffer + 2 * x;

				value = Insert(value, Float(*Pointer<Half>(buffer + 0)), 0);
				value = Insert(value, Float(*Pointer<Half>(buffer + 2)), 1);

				buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));

				value = Insert(value, Float(*Pointer<Half>(buffer + 0)), 2);
				value = Insert(value, Float(*Pointer<Half>(buffer + 2)), 3);

				oC.x = As<Float4>(As<Int4>(oC.x) & *Pointer<Int4>(constants + OFFSET(Constants, maskD4X) + xMask * 16, 16));
				value = As<Float4>(As<Int4>(value) & *Pointer<Int4>(constants + OFFSET(Constants, invMaskD4X) + xMask * 16, 16));
				oC.x = As<Float4>(As<Int4>(oC.x) | As<Int4>(value));

				*Pointer<Half>(buffer + 0) = Half(oC.x.z);
				*Pointer<Half>(buffer + 2) = Half(oC.x.w);

				buffer -= *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));

				*Pointer<Half>(buffer + 0) = Half(oC.x.x);
				*Pointer<Half>(buffer + 2) = Half(oC.x.y);
			}
			break;
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16_UINT:
			if(rgbaWriteMask & 0x00000001)
			{
				buffer = cBuffer + 2 * x;

				UShort4 xyzw;
				xyzw = As<UShort4>(Insert(As<Int2>(xyzw), *Pointer<Int>(buffer), 0));

				buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));

				xyzw = As<UShort4>(Insert(As<Int2>(xyzw), *Pointer<Int>(buffer), 1));
				value = As<Float4>(Int4(xyzw));

				oC.x = As<Float4>(As<Int4>(oC.x) & *Pointer<Int4>(constants + OFFSET(Constants, maskD4X) + xMask * 16, 16));
				value = As<Float4>(As<Int4>(value) & *Pointer<Int4>(constants + OFFSET(Constants, invMaskD4X) + xMask * 16, 16));
				oC.x = As<Float4>(As<Int4>(oC.x) | As<Int4>(value));

				if(targetFormat == VK_FORMAT_R16_SINT)
				{
					Float component = oC.x.z;
					*Pointer<Short>(buffer + 0) = Short(As<Int>(component));
					component = oC.x.w;
					*Pointer<Short>(buffer + 2) = Short(As<Int>(component));

					buffer -= *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));

					component = oC.x.x;
					*Pointer<Short>(buffer + 0) = Short(As<Int>(component));
					component = oC.x.y;
					*Pointer<Short>(buffer + 2) = Short(As<Int>(component));
				}
				else // VK_FORMAT_R16_UINT
				{
					Float component = oC.x.z;
					*Pointer<UShort>(buffer + 0) = UShort(As<Int>(component));
					component = oC.x.w;
					*Pointer<UShort>(buffer + 2) = UShort(As<Int>(component));

					buffer -= *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));

					component = oC.x.x;
					*Pointer<UShort>(buffer + 0) = UShort(As<Int>(component));
					component = oC.x.y;
					*Pointer<UShort>(buffer + 2) = UShort(As<Int>(component));
				}
			}
			break;
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8_UINT:
			if(rgbaWriteMask & 0x00000001)
			{
				buffer = cBuffer + x;

				UInt xyzw, packedCol;

				xyzw = UInt(*Pointer<UShort>(buffer)) & 0xFFFF;
				buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));
				xyzw |= UInt(*Pointer<UShort>(buffer)) << 16;

				Short4 tmpCol = Short4(As<Int4>(oC.x));
				if(targetFormat == VK_FORMAT_R8_SINT)
				{
					tmpCol = As<Short4>(PackSigned(tmpCol, tmpCol));
				}
				else
				{
					tmpCol = As<Short4>(PackUnsigned(tmpCol, tmpCol));
				}
				packedCol = Extract(As<Int2>(tmpCol), 0);

				packedCol = (packedCol & *Pointer<UInt>(constants + OFFSET(Constants, maskB4Q) + 8 * xMask)) |
				            (xyzw & *Pointer<UInt>(constants + OFFSET(Constants, invMaskB4Q) + 8 * xMask));

				*Pointer<UShort>(buffer) = UShort(packedCol >> 16);
				buffer -= *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));
				*Pointer<UShort>(buffer) = UShort(packedCol);
			}
			break;
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_UINT:
			buffer = cBuffer + 8 * x;

			value = *Pointer<Float4>(buffer);

			if((rgbaWriteMask & 0x00000003) != 0x00000003)
			{
				Float4 masked = value;
				oC.x = As<Float4>(As<Int4>(oC.x) & *Pointer<Int4>(constants + OFFSET(Constants,maskD01X[rgbaWriteMask & 0x3][0])));
				masked = As<Float4>(As<Int4>(masked) & *Pointer<Int4>(constants + OFFSET(Constants,maskD01X[~rgbaWriteMask & 0x3][0])));
				oC.x = As<Float4>(As<Int4>(oC.x) | As<Int4>(masked));
			}

			oC.x = As<Float4>(As<Int4>(oC.x) & *Pointer<Int4>(constants + OFFSET(Constants,maskQ01X) + xMask * 16, 16));
			value = As<Float4>(As<Int4>(value) & *Pointer<Int4>(constants + OFFSET(Constants,invMaskQ01X) + xMask * 16, 16));
			oC.x = As<Float4>(As<Int4>(oC.x) | As<Int4>(value));
			*Pointer<Float4>(buffer) = oC.x;

			buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));

			value = *Pointer<Float4>(buffer);

			if((rgbaWriteMask & 0x00000003) != 0x00000003)
			{
				Float4 masked;

				masked = value;
				oC.y = As<Float4>(As<Int4>(oC.y) & *Pointer<Int4>(constants + OFFSET(Constants,maskD01X[rgbaWriteMask & 0x3][0])));
				masked = As<Float4>(As<Int4>(masked) & *Pointer<Int4>(constants + OFFSET(Constants,maskD01X[~rgbaWriteMask & 0x3][0])));
				oC.y = As<Float4>(As<Int4>(oC.y) | As<Int4>(masked));
			}

			oC.y = As<Float4>(As<Int4>(oC.y) & *Pointer<Int4>(constants + OFFSET(Constants,maskQ23X) + xMask * 16, 16));
			value = As<Float4>(As<Int4>(value) & *Pointer<Int4>(constants + OFFSET(Constants,invMaskQ23X) + xMask * 16, 16));
			oC.y = As<Float4>(As<Int4>(oC.y) | As<Int4>(value));
			*Pointer<Float4>(buffer) = oC.y;
			break;
		case VK_FORMAT_R16G16_SFLOAT:
			if((rgbaWriteMask & 0x00000003) != 0x0)
			{
				buffer = cBuffer + 4 * x;

				UInt2 rgbaMask;
				UInt2 packedCol;
				packedCol = Insert(packedCol, (UInt(As<UShort>(Half(oC.x.y))) << 16) | UInt(As<UShort>(Half(oC.x.x))), 0);
				packedCol = Insert(packedCol, (UInt(As<UShort>(Half(oC.x.w))) << 16) | UInt(As<UShort>(Half(oC.x.z))), 1);

				UShort4 value = *Pointer<UShort4>(buffer);
				UInt2 mergedMask = *Pointer<UInt2>(constants + OFFSET(Constants, maskD01Q) + xMask * 8);
				if((rgbaWriteMask & 0x3) != 0x3)
				{
					Int tmpMask = *Pointer<Int>(constants + OFFSET(Constants, maskW4Q[rgbaWriteMask & 0x3][0]));
					rgbaMask = As<UInt2>(Int2(tmpMask, tmpMask));
					mergedMask &= rgbaMask;
				}
				*Pointer<UInt2>(buffer) = (packedCol & mergedMask) | (As<UInt2>(value) & ~mergedMask);

				buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));

				packedCol = Insert(packedCol, (UInt(As<UShort>(Half(oC.y.y))) << 16) | UInt(As<UShort>(Half(oC.y.x))), 0);
				packedCol = Insert(packedCol, (UInt(As<UShort>(Half(oC.y.w))) << 16) | UInt(As<UShort>(Half(oC.y.z))), 1);
				value = *Pointer<UShort4>(buffer);
				mergedMask = *Pointer<UInt2>(constants + OFFSET(Constants, maskD23Q) + xMask * 8);
				if((rgbaWriteMask & 0x3) != 0x3)
				{
					mergedMask &= rgbaMask;
				}
				*Pointer<UInt2>(buffer) = (packedCol & mergedMask) | (As<UInt2>(value) & ~mergedMask);
			}
			break;
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16_UINT:
			if((rgbaWriteMask & 0x00000003) != 0x0)
			{
				buffer = cBuffer + 4 * x;

				UInt2 rgbaMask;
				UShort4 packedCol = UShort4(As<Int4>(oC.x));
				UShort4 value = *Pointer<UShort4>(buffer);
				UInt2 mergedMask = *Pointer<UInt2>(constants + OFFSET(Constants, maskD01Q) + xMask * 8);
				if((rgbaWriteMask & 0x3) != 0x3)
				{
					Int tmpMask = *Pointer<Int>(constants + OFFSET(Constants, maskW4Q[rgbaWriteMask & 0x3][0]));
					rgbaMask = As<UInt2>(Int2(tmpMask, tmpMask));
					mergedMask &= rgbaMask;
				}
				*Pointer<UInt2>(buffer) = (As<UInt2>(packedCol) & mergedMask) | (As<UInt2>(value) & ~mergedMask);

				buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));

				packedCol = UShort4(As<Int4>(oC.y));
				value = *Pointer<UShort4>(buffer);
				mergedMask = *Pointer<UInt2>(constants + OFFSET(Constants, maskD23Q) + xMask * 8);
				if((rgbaWriteMask & 0x3) != 0x3)
				{
					mergedMask &= rgbaMask;
				}
				*Pointer<UInt2>(buffer) = (As<UInt2>(packedCol) & mergedMask) | (As<UInt2>(value) & ~mergedMask);
			}
			break;
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R8G8_UINT:
			if((rgbaWriteMask & 0x00000003) != 0x0)
			{
				buffer = cBuffer + 2 * x;

				Int2 xyzw, packedCol;

				xyzw = Insert(xyzw, *Pointer<Int>(buffer), 0);
				buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));
				xyzw = Insert(xyzw, *Pointer<Int>(buffer), 1);

				if(targetFormat == VK_FORMAT_R8G8_SINT)
				{
					packedCol = As<Int2>(PackSigned(Short4(As<Int4>(oC.x)), Short4(As<Int4>(oC.y))));
				}
				else
				{
					packedCol = As<Int2>(PackUnsigned(Short4(As<Int4>(oC.x)), Short4(As<Int4>(oC.y))));
				}

				UInt2 mergedMask = *Pointer<UInt2>(constants + OFFSET(Constants, maskW4Q) + xMask * 8);
				if((rgbaWriteMask & 0x3) != 0x3)
				{
					Int tmpMask = *Pointer<Int>(constants + OFFSET(Constants, maskB4Q[5 * (rgbaWriteMask & 0x3)][0]));
					UInt2 rgbaMask = As<UInt2>(Int2(tmpMask, tmpMask));
					mergedMask &= rgbaMask;
				}

				packedCol = As<Int2>((As<UInt2>(packedCol) & mergedMask) | (As<UInt2>(xyzw) & ~mergedMask));

				*Pointer<UInt>(buffer) = As<UInt>(Extract(packedCol, 1));
				buffer -= *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));
				*Pointer<UInt>(buffer) = As<UInt>(Extract(packedCol, 0));
			}
			break;
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_UINT:
			buffer = cBuffer + 16 * x;

			{
				value = *Pointer<Float4>(buffer, 16);

				if(rgbaWriteMask != 0x0000000F)
				{
					Float4 masked = value;
					oC.x = As<Float4>(As<Int4>(oC.x) & *Pointer<Int4>(constants + OFFSET(Constants,maskD4X[rgbaWriteMask][0])));
					masked = As<Float4>(As<Int4>(masked) & *Pointer<Int4>(constants + OFFSET(Constants,invMaskD4X[rgbaWriteMask][0])));
					oC.x = As<Float4>(As<Int4>(oC.x) | As<Int4>(masked));
				}

				oC.x = As<Float4>(As<Int4>(oC.x) & *Pointer<Int4>(constants + OFFSET(Constants,maskX0X) + xMask * 16, 16));
				value = As<Float4>(As<Int4>(value) & *Pointer<Int4>(constants + OFFSET(Constants,invMaskX0X) + xMask * 16, 16));
				oC.x = As<Float4>(As<Int4>(oC.x) | As<Int4>(value));
				*Pointer<Float4>(buffer, 16) = oC.x;
			}

			{
				value = *Pointer<Float4>(buffer + 16, 16);

				if(rgbaWriteMask != 0x0000000F)
				{
					Float4 masked = value;
					oC.y = As<Float4>(As<Int4>(oC.y) & *Pointer<Int4>(constants + OFFSET(Constants,maskD4X[rgbaWriteMask][0])));
					masked = As<Float4>(As<Int4>(masked) & *Pointer<Int4>(constants + OFFSET(Constants,invMaskD4X[rgbaWriteMask][0])));
					oC.y = As<Float4>(As<Int4>(oC.y) | As<Int4>(masked));
				}

				oC.y = As<Float4>(As<Int4>(oC.y) & *Pointer<Int4>(constants + OFFSET(Constants,maskX1X) + xMask * 16, 16));
				value = As<Float4>(As<Int4>(value) & *Pointer<Int4>(constants + OFFSET(Constants,invMaskX1X) + xMask * 16, 16));
				oC.y = As<Float4>(As<Int4>(oC.y) | As<Int4>(value));
				*Pointer<Float4>(buffer + 16, 16) = oC.y;
			}

			buffer += *Pointer<Int>(data + OFFSET(DrawData,colorPitchB[index]));

			{
				value = *Pointer<Float4>(buffer, 16);

				if(rgbaWriteMask != 0x0000000F)
				{
					Float4 masked = value;
					oC.z = As<Float4>(As<Int4>(oC.z) & *Pointer<Int4>(constants + OFFSET(Constants,maskD4X[rgbaWriteMask][0])));
					masked = As<Float4>(As<Int4>(masked) & *Pointer<Int4>(constants + OFFSET(Constants,invMaskD4X[rgbaWriteMask][0])));
					oC.z = As<Float4>(As<Int4>(oC.z) | As<Int4>(masked));
				}

				oC.z = As<Float4>(As<Int4>(oC.z) & *Pointer<Int4>(constants + OFFSET(Constants,maskX2X) + xMask * 16, 16));
				value = As<Float4>(As<Int4>(value) & *Pointer<Int4>(constants + OFFSET(Constants,invMaskX2X) + xMask * 16, 16));
				oC.z = As<Float4>(As<Int4>(oC.z) | As<Int4>(value));
				*Pointer<Float4>(buffer, 16) = oC.z;
			}

			{
				value = *Pointer<Float4>(buffer + 16, 16);

				if(rgbaWriteMask != 0x0000000F)
				{
					Float4 masked = value;
					oC.w = As<Float4>(As<Int4>(oC.w) & *Pointer<Int4>(constants + OFFSET(Constants,maskD4X[rgbaWriteMask][0])));
					masked = As<Float4>(As<Int4>(masked) & *Pointer<Int4>(constants + OFFSET(Constants,invMaskD4X[rgbaWriteMask][0])));
					oC.w = As<Float4>(As<Int4>(oC.w) | As<Int4>(masked));
				}

				oC.w = As<Float4>(As<Int4>(oC.w) & *Pointer<Int4>(constants + OFFSET(Constants,maskX3X) + xMask * 16, 16));
				value = As<Float4>(As<Int4>(value) & *Pointer<Int4>(constants + OFFSET(Constants,invMaskX3X) + xMask * 16, 16));
				oC.w = As<Float4>(As<Int4>(oC.w) | As<Int4>(value));
				*Pointer<Float4>(buffer + 16, 16) = oC.w;
			}
			break;
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			if((rgbaWriteMask & 0x0000000F) != 0x0)
			{
				buffer = cBuffer + 8 * x;

				UInt4 rgbaMask;
				UInt4 value = *Pointer<UInt4>(buffer);
				UInt4 packedCol;
				packedCol = Insert(packedCol, (UInt(As<UShort>(Half(oC.x.y))) << 16) | UInt(As<UShort>(Half(oC.x.x))), 0);
				packedCol = Insert(packedCol, (UInt(As<UShort>(Half(oC.x.w))) << 16) | UInt(As<UShort>(Half(oC.x.z))), 1);
				packedCol = Insert(packedCol, (UInt(As<UShort>(Half(oC.y.y))) << 16) | UInt(As<UShort>(Half(oC.y.x))), 2);
				packedCol = Insert(packedCol, (UInt(As<UShort>(Half(oC.y.w))) << 16) | UInt(As<UShort>(Half(oC.y.z))), 3);
				UInt4 mergedMask = *Pointer<UInt4>(constants + OFFSET(Constants, maskQ01X) + xMask * 16);
				if((rgbaWriteMask & 0xF) != 0xF)
				{
					UInt2 tmpMask = *Pointer<UInt2>(constants + OFFSET(Constants, maskW4Q[rgbaWriteMask][0]));
					rgbaMask = UInt4(tmpMask, tmpMask);
					mergedMask &= rgbaMask;
				}
				*Pointer<UInt4>(buffer) = (packedCol & mergedMask) | (As<UInt4>(value) & ~mergedMask);

				buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));

				value = *Pointer<UInt4>(buffer);
				packedCol = Insert(packedCol, (UInt(As<UShort>(Half(oC.z.y))) << 16) | UInt(As<UShort>(Half(oC.z.x))), 0);
				packedCol = Insert(packedCol, (UInt(As<UShort>(Half(oC.z.w))) << 16) | UInt(As<UShort>(Half(oC.z.z))), 1);
				packedCol = Insert(packedCol, (UInt(As<UShort>(Half(oC.w.y))) << 16) | UInt(As<UShort>(Half(oC.w.x))), 2);
				packedCol = Insert(packedCol, (UInt(As<UShort>(Half(oC.w.w))) << 16) | UInt(As<UShort>(Half(oC.w.z))), 3);
				mergedMask = *Pointer<UInt4>(constants + OFFSET(Constants, maskQ23X) + xMask * 16);
				if((rgbaWriteMask & 0xF) != 0xF)
				{
					mergedMask &= rgbaMask;
				}
				*Pointer<UInt4>(buffer) = (packedCol & mergedMask) | (As<UInt4>(value) & ~mergedMask);
			}
			break;
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R16G16B16A16_UINT:
			if((rgbaWriteMask & 0x0000000F) != 0x0)
			{
				buffer = cBuffer + 8 * x;

				UInt4 rgbaMask;
				UShort8 value = *Pointer<UShort8>(buffer);
				UShort8 packedCol = UShort8(UShort4(As<Int4>(oC.x)), UShort4(As<Int4>(oC.y)));
				UInt4 mergedMask = *Pointer<UInt4>(constants + OFFSET(Constants, maskQ01X) + xMask * 16);
				if((rgbaWriteMask & 0xF) != 0xF)
				{
					UInt2 tmpMask = *Pointer<UInt2>(constants + OFFSET(Constants, maskW4Q[rgbaWriteMask][0]));
					rgbaMask = UInt4(tmpMask, tmpMask);
					mergedMask &= rgbaMask;
				}
				*Pointer<UInt4>(buffer) = (As<UInt4>(packedCol) & mergedMask) | (As<UInt4>(value) & ~mergedMask);

				buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));

				value = *Pointer<UShort8>(buffer);
				packedCol = UShort8(UShort4(As<Int4>(oC.z)), UShort4(As<Int4>(oC.w)));
				mergedMask = *Pointer<UInt4>(constants + OFFSET(Constants, maskQ23X) + xMask * 16);
				if((rgbaWriteMask & 0xF) != 0xF)
				{
					mergedMask &= rgbaMask;
				}
				*Pointer<UInt4>(buffer) = (As<UInt4>(packedCol) & mergedMask) | (As<UInt4>(value) & ~mergedMask);
			}
			break;
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_A8B8G8R8_UINT_PACK32:
		case VK_FORMAT_A8B8G8R8_SINT_PACK32:
			if((rgbaWriteMask & 0x0000000F) != 0x0)
			{
				UInt2 value, packedCol, mergedMask;

				buffer = cBuffer + 4 * x;

				bool isSigned = targetFormat == VK_FORMAT_R8G8B8A8_SINT || targetFormat == VK_FORMAT_A8B8G8R8_SINT_PACK32;

				if(isSigned)
				{
					packedCol = As<UInt2>(PackSigned(Short4(As<Int4>(oC.x)), Short4(As<Int4>(oC.y))));
				}
				else
				{
					packedCol = As<UInt2>(PackUnsigned(Short4(As<Int4>(oC.x)), Short4(As<Int4>(oC.y))));
				}
				value = *Pointer<UInt2>(buffer, 16);
				mergedMask = *Pointer<UInt2>(constants + OFFSET(Constants, maskD01Q) + xMask * 8);
				if(rgbaWriteMask != 0xF)
				{
					mergedMask &= *Pointer<UInt2>(constants + OFFSET(Constants, maskB4Q[rgbaWriteMask][0]));
				}
				*Pointer<UInt2>(buffer) = (packedCol & mergedMask) | (value & ~mergedMask);

				buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));

				if(isSigned)
				{
					packedCol = As<UInt2>(PackSigned(Short4(As<Int4>(oC.z)), Short4(As<Int4>(oC.w))));
				}
				else
				{
					packedCol = As<UInt2>(PackUnsigned(Short4(As<Int4>(oC.z)), Short4(As<Int4>(oC.w))));
				}
				value = *Pointer<UInt2>(buffer, 16);
				mergedMask = *Pointer<UInt2>(constants + OFFSET(Constants, maskD23Q) + xMask * 8);
				if(rgbaWriteMask != 0xF)
				{
					mergedMask &= *Pointer<UInt2>(constants + OFFSET(Constants, maskB4Q[rgbaWriteMask][0]));
				}
				*Pointer<UInt2>(buffer) = (packedCol & mergedMask) | (value & ~mergedMask);
			}
			break;
		case VK_FORMAT_A2B10G10R10_UINT_PACK32:
			if ((rgbaWriteMask & 0x0000000F) != 0x0)
			{
				Int2 mergedMask, packedCol, value;
				Int4 packed = ((As<Int4>(oC.w) & Int4(0x3)) << 30) |
						((As<Int4>(oC.z) & Int4(0x3ff)) << 20) |
						((As<Int4>(oC.y) & Int4(0x3ff)) << 10) |
						((As<Int4>(oC.x) & Int4(0x3ff)));

				buffer = cBuffer + 4 * x;
				value = *Pointer<Int2>(buffer, 16);
				mergedMask = *Pointer<Int2>(constants + OFFSET(Constants, maskD01Q) + xMask * 8);
				if (rgbaWriteMask != 0xF)
				{
					mergedMask &= *Pointer<Int2>(constants + OFFSET(Constants, mask10Q[rgbaWriteMask][0]));
				}
				*Pointer<Int2>(buffer) = (As<Int2>(packed) & mergedMask) | (value & ~mergedMask);

				buffer += *Pointer<Int>(data + OFFSET(DrawData, colorPitchB[index]));

				value = *Pointer<Int2>(buffer, 16);
				mergedMask = *Pointer<Int2>(constants + OFFSET(Constants, maskD23Q) + xMask * 8);
				if (rgbaWriteMask != 0xF)
				{
					mergedMask &= *Pointer<Int2>(constants + OFFSET(Constants, mask10Q[rgbaWriteMask][0]));
				}
				*Pointer<Int2>(buffer) = (As<Int2>(Int4(packed.zwww)) & mergedMask) | (value & ~mergedMask);
			}
			break;
		default:
			UNIMPLEMENTED("VkFormat: %d", int(targetFormat));
		}
	}

	UShort4 PixelRoutine::convertFixed16(Float4 &cf, bool saturate)
	{
		return UShort4(cf * Float4(0xFFFF), saturate);
	}

	void PixelRoutine::sRGBtoLinear16_12_16(Vector4s &c)
	{
		Pointer<Byte> LUT = constants + OFFSET(Constants,sRGBtoLinear12_16);

		c.x = As<UShort4>(c.x) >> 4;
		c.y = As<UShort4>(c.y) >> 4;
		c.z = As<UShort4>(c.z) >> 4;

		c.x = Insert(c.x, *Pointer<Short>(LUT + 2 * Int(Extract(c.x, 0))), 0);
		c.x = Insert(c.x, *Pointer<Short>(LUT + 2 * Int(Extract(c.x, 1))), 1);
		c.x = Insert(c.x, *Pointer<Short>(LUT + 2 * Int(Extract(c.x, 2))), 2);
		c.x = Insert(c.x, *Pointer<Short>(LUT + 2 * Int(Extract(c.x, 3))), 3);

		c.y = Insert(c.y, *Pointer<Short>(LUT + 2 * Int(Extract(c.y, 0))), 0);
		c.y = Insert(c.y, *Pointer<Short>(LUT + 2 * Int(Extract(c.y, 1))), 1);
		c.y = Insert(c.y, *Pointer<Short>(LUT + 2 * Int(Extract(c.y, 2))), 2);
		c.y = Insert(c.y, *Pointer<Short>(LUT + 2 * Int(Extract(c.y, 3))), 3);

		c.z = Insert(c.z, *Pointer<Short>(LUT + 2 * Int(Extract(c.z, 0))), 0);
		c.z = Insert(c.z, *Pointer<Short>(LUT + 2 * Int(Extract(c.z, 1))), 1);
		c.z = Insert(c.z, *Pointer<Short>(LUT + 2 * Int(Extract(c.z, 2))), 2);
		c.z = Insert(c.z, *Pointer<Short>(LUT + 2 * Int(Extract(c.z, 3))), 3);
	}

	void PixelRoutine::linearToSRGB16_12_16(Vector4s &c)
	{
		c.x = As<UShort4>(c.x) >> 4;
		c.y = As<UShort4>(c.y) >> 4;
		c.z = As<UShort4>(c.z) >> 4;

		linearToSRGB12_16(c);
	}

	void PixelRoutine::linearToSRGB12_16(Vector4s &c)
	{
		Pointer<Byte> LUT = constants + OFFSET(Constants,linearToSRGB12_16);

		c.x = Insert(c.x, *Pointer<Short>(LUT + 2 * Int(Extract(c.x, 0))), 0);
		c.x = Insert(c.x, *Pointer<Short>(LUT + 2 * Int(Extract(c.x, 1))), 1);
		c.x = Insert(c.x, *Pointer<Short>(LUT + 2 * Int(Extract(c.x, 2))), 2);
		c.x = Insert(c.x, *Pointer<Short>(LUT + 2 * Int(Extract(c.x, 3))), 3);

		c.y = Insert(c.y, *Pointer<Short>(LUT + 2 * Int(Extract(c.y, 0))), 0);
		c.y = Insert(c.y, *Pointer<Short>(LUT + 2 * Int(Extract(c.y, 1))), 1);
		c.y = Insert(c.y, *Pointer<Short>(LUT + 2 * Int(Extract(c.y, 2))), 2);
		c.y = Insert(c.y, *Pointer<Short>(LUT + 2 * Int(Extract(c.y, 3))), 3);

		c.z = Insert(c.z, *Pointer<Short>(LUT + 2 * Int(Extract(c.z, 0))), 0);
		c.z = Insert(c.z, *Pointer<Short>(LUT + 2 * Int(Extract(c.z, 1))), 1);
		c.z = Insert(c.z, *Pointer<Short>(LUT + 2 * Int(Extract(c.z, 2))), 2);
		c.z = Insert(c.z, *Pointer<Short>(LUT + 2 * Int(Extract(c.z, 3))), 3);
	}

	Float4 PixelRoutine::sRGBtoLinear(const Float4 &x)   // Approximates x^2.2
	{
		Float4 linear = x * x;
		linear = linear * Float4(0.73f) + linear * x * Float4(0.27f);

		return Min(Max(linear, Float4(0.0f)), Float4(1.0f));
	}
}
