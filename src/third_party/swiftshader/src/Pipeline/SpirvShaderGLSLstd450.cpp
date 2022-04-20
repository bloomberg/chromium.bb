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

#include "ShaderCore.hpp"
#include "Device/Primitive.hpp"
#include "Pipeline/Constants.hpp"

#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.hpp>

namespace {
constexpr float PI = 3.141592653589793f;

sw::SIMD::Float Interpolate(const sw::SIMD::Float &x, const sw::SIMD::Float &y, const sw::SIMD::Float &rhw,
                            const sw::SIMD::Float &A, const sw::SIMD::Float &B, const sw::SIMD::Float &C,
                            bool flat, bool perspective)
{
	sw::SIMD::Float interpolant = C;

	if(!flat)
	{
		interpolant += x * A + y * B;

		if(perspective)
		{
			interpolant *= rhw;
		}
	}

	return interpolant;
}

}  // namespace

namespace sw {

SpirvShader::EmitResult SpirvShader::EmitExtGLSLstd450(InsnIterator insn, EmitState *state) const
{
	auto &type = getType(insn.resultTypeId());
	auto &dst = state->createIntermediate(insn.resultId(), type.componentCount);
	auto extInstIndex = static_cast<GLSLstd450>(insn.word(4));

	switch(extInstIndex)
	{
	case GLSLstd450FAbs:
		{
			auto src = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Abs(src.Float(i)));
			}
		}
		break;
	case GLSLstd450SAbs:
		{
			auto src = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Abs(src.Int(i)));
			}
		}
		break;
	case GLSLstd450Cross:
		{
			auto lhs = Operand(this, state, insn.word(5));
			auto rhs = Operand(this, state, insn.word(6));
			dst.move(0, lhs.Float(1) * rhs.Float(2) - rhs.Float(1) * lhs.Float(2));
			dst.move(1, lhs.Float(2) * rhs.Float(0) - rhs.Float(2) * lhs.Float(0));
			dst.move(2, lhs.Float(0) * rhs.Float(1) - rhs.Float(0) * lhs.Float(1));
		}
		break;
	case GLSLstd450Floor:
		{
			auto src = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Floor(src.Float(i)));
			}
		}
		break;
	case GLSLstd450Trunc:
		{
			auto src = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Trunc(src.Float(i)));
			}
		}
		break;
	case GLSLstd450Ceil:
		{
			auto src = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Ceil(src.Float(i)));
			}
		}
		break;
	case GLSLstd450Fract:
		{
			auto src = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Frac(src.Float(i)));
			}
		}
		break;
	case GLSLstd450Round:
		{
			auto src = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Round(src.Float(i)));
			}
		}
		break;
	case GLSLstd450RoundEven:
		{
			auto src = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				auto x = Round(src.Float(i));
				// dst = round(src) + ((round(src) < src) * 2 - 1) * (fract(src) == 0.5) * isOdd(round(src));
				dst.move(i, x + ((SIMD::Float(CmpLT(x, src.Float(i)) & SIMD::Int(1)) * SIMD::Float(2.0f)) - SIMD::Float(1.0f)) *
				                    SIMD::Float(CmpEQ(Frac(src.Float(i)), SIMD::Float(0.5f)) & SIMD::Int(1)) * SIMD::Float(Int4(x) & SIMD::Int(1)));
			}
		}
		break;
	case GLSLstd450FMin:
		{
			auto lhs = Operand(this, state, insn.word(5));
			auto rhs = Operand(this, state, insn.word(6));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Min(lhs.Float(i), rhs.Float(i)));
			}
		}
		break;
	case GLSLstd450FMax:
		{
			auto lhs = Operand(this, state, insn.word(5));
			auto rhs = Operand(this, state, insn.word(6));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Max(lhs.Float(i), rhs.Float(i)));
			}
		}
		break;
	case GLSLstd450SMin:
		{
			auto lhs = Operand(this, state, insn.word(5));
			auto rhs = Operand(this, state, insn.word(6));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Min(lhs.Int(i), rhs.Int(i)));
			}
		}
		break;
	case GLSLstd450SMax:
		{
			auto lhs = Operand(this, state, insn.word(5));
			auto rhs = Operand(this, state, insn.word(6));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Max(lhs.Int(i), rhs.Int(i)));
			}
		}
		break;
	case GLSLstd450UMin:
		{
			auto lhs = Operand(this, state, insn.word(5));
			auto rhs = Operand(this, state, insn.word(6));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Min(lhs.UInt(i), rhs.UInt(i)));
			}
		}
		break;
	case GLSLstd450UMax:
		{
			auto lhs = Operand(this, state, insn.word(5));
			auto rhs = Operand(this, state, insn.word(6));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Max(lhs.UInt(i), rhs.UInt(i)));
			}
		}
		break;
	case GLSLstd450Step:
		{
			auto edge = Operand(this, state, insn.word(5));
			auto x = Operand(this, state, insn.word(6));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, CmpNLT(x.Float(i), edge.Float(i)) & As<SIMD::Int>(SIMD::Float(1.0f)));
			}
		}
		break;
	case GLSLstd450SmoothStep:
		{
			auto edge0 = Operand(this, state, insn.word(5));
			auto edge1 = Operand(this, state, insn.word(6));
			auto x = Operand(this, state, insn.word(7));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				auto tx = Min(Max((x.Float(i) - edge0.Float(i)) /
				                      (edge1.Float(i) - edge0.Float(i)),
				                  SIMD::Float(0.0f)),
				              SIMD::Float(1.0f));
				dst.move(i, tx * tx * (Float4(3.0f) - Float4(2.0f) * tx));
			}
		}
		break;
	case GLSLstd450FMix:
		{
			auto x = Operand(this, state, insn.word(5));
			auto y = Operand(this, state, insn.word(6));
			auto a = Operand(this, state, insn.word(7));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, a.Float(i) * (y.Float(i) - x.Float(i)) + x.Float(i));
			}
		}
		break;
	case GLSLstd450FClamp:
		{
			auto x = Operand(this, state, insn.word(5));
			auto minVal = Operand(this, state, insn.word(6));
			auto maxVal = Operand(this, state, insn.word(7));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Min(Max(x.Float(i), minVal.Float(i)), maxVal.Float(i)));
			}
		}
		break;
	case GLSLstd450SClamp:
		{
			auto x = Operand(this, state, insn.word(5));
			auto minVal = Operand(this, state, insn.word(6));
			auto maxVal = Operand(this, state, insn.word(7));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Min(Max(x.Int(i), minVal.Int(i)), maxVal.Int(i)));
			}
		}
		break;
	case GLSLstd450UClamp:
		{
			auto x = Operand(this, state, insn.word(5));
			auto minVal = Operand(this, state, insn.word(6));
			auto maxVal = Operand(this, state, insn.word(7));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Min(Max(x.UInt(i), minVal.UInt(i)), maxVal.UInt(i)));
			}
		}
		break;
	case GLSLstd450FSign:
		{
			auto src = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				auto neg = As<SIMD::Int>(CmpLT(src.Float(i), SIMD::Float(-0.0f))) & As<SIMD::Int>(SIMD::Float(-1.0f));
				auto pos = As<SIMD::Int>(CmpNLE(src.Float(i), SIMD::Float(+0.0f))) & As<SIMD::Int>(SIMD::Float(1.0f));
				dst.move(i, neg | pos);
			}
		}
		break;
	case GLSLstd450SSign:
		{
			auto src = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				auto neg = CmpLT(src.Int(i), SIMD::Int(0)) & SIMD::Int(-1);
				auto pos = CmpNLE(src.Int(i), SIMD::Int(0)) & SIMD::Int(1);
				dst.move(i, neg | pos);
			}
		}
		break;
	case GLSLstd450Reflect:
		{
			auto I = Operand(this, state, insn.word(5));
			auto N = Operand(this, state, insn.word(6));

			SIMD::Float d = FDot(type.componentCount, I, N);

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, I.Float(i) - SIMD::Float(2.0f) * d * N.Float(i));
			}
		}
		break;
	case GLSLstd450Refract:
		{
			auto I = Operand(this, state, insn.word(5));
			auto N = Operand(this, state, insn.word(6));
			auto eta = Operand(this, state, insn.word(7));
			Decorations r = GetDecorationsForId(insn.resultId());

			SIMD::Float d = FDot(type.componentCount, I, N);
			SIMD::Float k = SIMD::Float(1.0f) - eta.Float(0) * eta.Float(0) * (SIMD::Float(1.0f) - d * d);
			SIMD::Int pos = CmpNLT(k, SIMD::Float(0.0f));
			SIMD::Float t = (eta.Float(0) * d + Sqrt(k, r.RelaxedPrecision));

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, pos & As<SIMD::Int>(eta.Float(0) * I.Float(i) - t * N.Float(i)));
			}
		}
		break;
	case GLSLstd450FaceForward:
		{
			auto N = Operand(this, state, insn.word(5));
			auto I = Operand(this, state, insn.word(6));
			auto Nref = Operand(this, state, insn.word(7));

			SIMD::Float d = FDot(type.componentCount, I, Nref);
			SIMD::Int neg = CmpLT(d, SIMD::Float(0.0f));

			for(auto i = 0u; i < type.componentCount; i++)
			{
				auto n = N.Float(i);
				dst.move(i, (neg & As<SIMD::Int>(n)) | (~neg & As<SIMD::Int>(-n)));
			}
		}
		break;
	case GLSLstd450Length:
		{
			auto x = Operand(this, state, insn.word(5));
			SIMD::Float d = FDot(getObjectType(insn.word(5)).componentCount, x, x);
			Decorations r = GetDecorationsForId(insn.resultId());

			dst.move(0, Sqrt(d, r.RelaxedPrecision));
		}
		break;
	case GLSLstd450Normalize:
		{
			auto x = Operand(this, state, insn.word(5));
			Decorations r = GetDecorationsForId(insn.resultId());

			SIMD::Float d = FDot(getObjectType(insn.word(5)).componentCount, x, x);
			SIMD::Float invLength = SIMD::Float(1.0f) / Sqrt(d, r.RelaxedPrecision);

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, invLength * x.Float(i));
			}
		}
		break;
	case GLSLstd450Distance:
		{
			auto p0 = Operand(this, state, insn.word(5));
			auto p1 = Operand(this, state, insn.word(6));
			Decorations r = GetDecorationsForId(insn.resultId());

			// sqrt(dot(p0-p1, p0-p1))
			SIMD::Float d = (p0.Float(0) - p1.Float(0)) * (p0.Float(0) - p1.Float(0));

			for(auto i = 1u; i < p0.componentCount; i++)
			{
				d += (p0.Float(i) - p1.Float(i)) * (p0.Float(i) - p1.Float(i));
			}

			dst.move(0, Sqrt(d, r.RelaxedPrecision));
		}
		break;
	case GLSLstd450Modf:
		{
			auto val = Operand(this, state, insn.word(5));
			auto ptrId = Object::ID(insn.word(6));

			Intermediate whole(type.componentCount);

			for(auto i = 0u; i < type.componentCount; i++)
			{
				auto wholeAndFrac = Modf(val.Float(i));
				dst.move(i, wholeAndFrac.second);
				whole.move(i, wholeAndFrac.first);
			}

			Store(ptrId, whole, false, std::memory_order_relaxed, state);
		}
		break;
	case GLSLstd450ModfStruct:
		{
			auto val = Operand(this, state, insn.word(5));

			for(auto i = 0u; i < val.componentCount; i++)
			{
				auto wholeAndFrac = Modf(val.Float(i));
				dst.move(i, wholeAndFrac.second);
				dst.move(val.componentCount + i, wholeAndFrac.first);
			}
		}
		break;
	case GLSLstd450PackSnorm4x8:
		{
			auto val = Operand(this, state, insn.word(5));
			dst.move(0, (SIMD::Int(Round(Min(Max(val.Float(0), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(127.0f))) &
			             SIMD::Int(0xFF)) |
			                ((SIMD::Int(Round(Min(Max(val.Float(1), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(127.0f))) &
			                  SIMD::Int(0xFF))
			                 << 8) |
			                ((SIMD::Int(Round(Min(Max(val.Float(2), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(127.0f))) &
			                  SIMD::Int(0xFF))
			                 << 16) |
			                ((SIMD::Int(Round(Min(Max(val.Float(3), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(127.0f))) &
			                  SIMD::Int(0xFF))
			                 << 24));
		}
		break;
	case GLSLstd450PackUnorm4x8:
		{
			auto val = Operand(this, state, insn.word(5));
			dst.move(0, (SIMD::UInt(Round(Min(Max(val.Float(0), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(255.0f)))) |
			                ((SIMD::UInt(Round(Min(Max(val.Float(1), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(255.0f)))) << 8) |
			                ((SIMD::UInt(Round(Min(Max(val.Float(2), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(255.0f)))) << 16) |
			                ((SIMD::UInt(Round(Min(Max(val.Float(3), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(255.0f)))) << 24));
		}
		break;
	case GLSLstd450PackSnorm2x16:
		{
			auto val = Operand(this, state, insn.word(5));
			dst.move(0, (SIMD::Int(Round(Min(Max(val.Float(0), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(32767.0f))) &
			             SIMD::Int(0xFFFF)) |
			                ((SIMD::Int(Round(Min(Max(val.Float(1), SIMD::Float(-1.0f)), SIMD::Float(1.0f)) * SIMD::Float(32767.0f))) &
			                  SIMD::Int(0xFFFF))
			                 << 16));
		}
		break;
	case GLSLstd450PackUnorm2x16:
		{
			auto val = Operand(this, state, insn.word(5));
			dst.move(0, (SIMD::UInt(Round(Min(Max(val.Float(0), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(65535.0f))) &
			             SIMD::UInt(0xFFFF)) |
			                ((SIMD::UInt(Round(Min(Max(val.Float(1), SIMD::Float(0.0f)), SIMD::Float(1.0f)) * SIMD::Float(65535.0f))) &
			                  SIMD::UInt(0xFFFF))
			                 << 16));
		}
		break;
	case GLSLstd450PackHalf2x16:
		{
			auto val = Operand(this, state, insn.word(5));
			dst.move(0, floatToHalfBits(val.UInt(0), false) | floatToHalfBits(val.UInt(1), true));
		}
		break;
	case GLSLstd450UnpackSnorm4x8:
		{
			auto val = Operand(this, state, insn.word(5));
			dst.move(0, Min(Max(SIMD::Float(((val.Int(0) << 24) & SIMD::Int(0xFF000000))) * SIMD::Float(1.0f / float(0x7f000000)), SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
			dst.move(1, Min(Max(SIMD::Float(((val.Int(0) << 16) & SIMD::Int(0xFF000000))) * SIMD::Float(1.0f / float(0x7f000000)), SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
			dst.move(2, Min(Max(SIMD::Float(((val.Int(0) << 8) & SIMD::Int(0xFF000000))) * SIMD::Float(1.0f / float(0x7f000000)), SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
			dst.move(3, Min(Max(SIMD::Float(((val.Int(0)) & SIMD::Int(0xFF000000))) * SIMD::Float(1.0f / float(0x7f000000)), SIMD::Float(-1.0f)), SIMD::Float(1.0f)));
		}
		break;
	case GLSLstd450UnpackUnorm4x8:
		{
			auto val = Operand(this, state, insn.word(5));
			dst.move(0, SIMD::Float((val.UInt(0) & SIMD::UInt(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(1, SIMD::Float(((val.UInt(0) >> 8) & SIMD::UInt(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(2, SIMD::Float(((val.UInt(0) >> 16) & SIMD::UInt(0xFF))) * SIMD::Float(1.0f / 255.f));
			dst.move(3, SIMD::Float(((val.UInt(0) >> 24) & SIMD::UInt(0xFF))) * SIMD::Float(1.0f / 255.f));
		}
		break;
	case GLSLstd450UnpackSnorm2x16:
		{
			auto val = Operand(this, state, insn.word(5));
			// clamp(f / 32767.0, -1.0, 1.0)
			dst.move(0, Min(Max(SIMD::Float(As<SIMD::Int>((val.UInt(0) & SIMD::UInt(0x0000FFFF)) << 16)) *
			                        SIMD::Float(1.0f / float(0x7FFF0000)),
			                    SIMD::Float(-1.0f)),
			                SIMD::Float(1.0f)));
			dst.move(1, Min(Max(SIMD::Float(As<SIMD::Int>(val.UInt(0) & SIMD::UInt(0xFFFF0000))) * SIMD::Float(1.0f / float(0x7FFF0000)),
			                    SIMD::Float(-1.0f)),
			                SIMD::Float(1.0f)));
		}
		break;
	case GLSLstd450UnpackUnorm2x16:
		{
			auto val = Operand(this, state, insn.word(5));
			// f / 65535.0
			dst.move(0, SIMD::Float((val.UInt(0) & SIMD::UInt(0x0000FFFF)) << 16) * SIMD::Float(1.0f / float(0xFFFF0000)));
			dst.move(1, SIMD::Float(val.UInt(0) & SIMD::UInt(0xFFFF0000)) * SIMD::Float(1.0f / float(0xFFFF0000)));
		}
		break;
	case GLSLstd450UnpackHalf2x16:
		{
			auto val = Operand(this, state, insn.word(5));
			dst.move(0, halfToFloatBits(val.UInt(0) & SIMD::UInt(0x0000FFFF)));
			dst.move(1, halfToFloatBits((val.UInt(0) & SIMD::UInt(0xFFFF0000)) >> 16));
		}
		break;
	case GLSLstd450Fma:
		{
			auto a = Operand(this, state, insn.word(5));
			auto b = Operand(this, state, insn.word(6));
			auto c = Operand(this, state, insn.word(7));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, MulAdd(a.Float(i), b.Float(i), c.Float(i)));
			}
		}
		break;
	case GLSLstd450Frexp:
		{
			auto val = Operand(this, state, insn.word(5));
			auto ptrId = Object::ID(insn.word(6));

			Intermediate exp(type.componentCount);

			for(auto i = 0u; i < type.componentCount; i++)
			{
				auto significandAndExponent = Frexp(val.Float(i));
				dst.move(i, significandAndExponent.first);
				exp.move(i, significandAndExponent.second);
			}

			Store(ptrId, exp, false, std::memory_order_relaxed, state);
		}
		break;
	case GLSLstd450FrexpStruct:
		{
			auto val = Operand(this, state, insn.word(5));

			for(auto i = 0u; i < val.componentCount; i++)
			{
				auto significandAndExponent = Frexp(val.Float(i));
				dst.move(i, significandAndExponent.first);
				dst.move(val.componentCount + i, significandAndExponent.second);
			}
		}
		break;
	case GLSLstd450Ldexp:
		{
			auto significand = Operand(this, state, insn.word(5));
			auto exponent = Operand(this, state, insn.word(6));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				// Assumes IEEE 754
				auto in = significand.Float(i);
				auto significandExponent = Exponent(in);
				auto combinedExponent = exponent.Int(i) + significandExponent;
				auto isSignificandZero = SIMD::UInt(CmpEQ(significand.Int(i), SIMD::Int(0)));
				auto isSignificandInf = SIMD::UInt(IsInf(in));
				auto isSignificandNaN = SIMD::UInt(IsNan(in));
				auto isExponentNotTooSmall = SIMD::UInt(CmpGE(combinedExponent, SIMD::Int(-126)));
				auto isExponentNotTooLarge = SIMD::UInt(CmpLE(combinedExponent, SIMD::Int(128)));
				auto isExponentInBounds = isExponentNotTooSmall & isExponentNotTooLarge;

				SIMD::UInt v;
				v = significand.UInt(i) & SIMD::UInt(0x7FFFFF);                          // Add significand.
				v |= (SIMD::UInt(combinedExponent + SIMD::Int(126)) << SIMD::UInt(23));  // Add exponent.
				v &= isExponentInBounds;                                                 // Clear v if the exponent is OOB.

				v |= significand.UInt(i) & SIMD::UInt(0x80000000);     // Add sign bit.
				v |= ~isExponentNotTooLarge & SIMD::UInt(0x7F800000);  // Mark as inf if the exponent is too great.

				// If the input significand is zero, inf or nan, just return the
				// input significand.
				auto passthrough = isSignificandZero | isSignificandInf | isSignificandNaN;
				v = (v & ~passthrough) | (significand.UInt(i) & passthrough);

				dst.move(i, As<SIMD::Float>(v));
			}
		}
		break;
	case GLSLstd450Radians:
		{
			auto degrees = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, degrees.Float(i) * SIMD::Float(PI / 180.0f));
			}
		}
		break;
	case GLSLstd450Degrees:
		{
			auto radians = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, radians.Float(i) * SIMD::Float(180.0f / PI));
			}
		}
		break;
	case GLSLstd450Sin:
		{
			auto radians = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Sin(radians.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Cos:
		{
			auto radians = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Cos(radians.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Tan:
		{
			auto radians = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Tan(radians.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Asin:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Asin(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Acos:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Acos(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Atan:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Atan(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Sinh:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Sinh(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Cosh:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Cosh(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Tanh:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Tanh(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Asinh:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Asinh(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Acosh:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Acosh(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Atanh:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Atanh(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Atan2:
		{
			auto x = Operand(this, state, insn.word(5));
			auto y = Operand(this, state, insn.word(6));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Atan2(x.Float(i), y.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Pow:
		{
			auto x = Operand(this, state, insn.word(5));
			auto y = Operand(this, state, insn.word(6));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Pow(x.Float(i), y.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Exp:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Exp(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Log:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Log(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Exp2:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Exp2(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Log2:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, sw::Log2(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Sqrt:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Sqrt(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450InverseSqrt:
		{
			auto val = Operand(this, state, insn.word(5));
			Decorations d = GetDecorationsForId(insn.resultId());

			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, RcpSqrt(val.Float(i), d.RelaxedPrecision));
			}
		}
		break;
	case GLSLstd450Determinant:
		{
			auto mat = Operand(this, state, insn.word(5));

			switch(mat.componentCount)
			{
			case 4:  // 2x2
				dst.move(0, Determinant(
				                mat.Float(0), mat.Float(1),
				                mat.Float(2), mat.Float(3)));
				break;
			case 9:  // 3x3
				dst.move(0, Determinant(
				                mat.Float(0), mat.Float(1), mat.Float(2),
				                mat.Float(3), mat.Float(4), mat.Float(5),
				                mat.Float(6), mat.Float(7), mat.Float(8)));
				break;
			case 16:  // 4x4
				dst.move(0, Determinant(
				                mat.Float(0), mat.Float(1), mat.Float(2), mat.Float(3),
				                mat.Float(4), mat.Float(5), mat.Float(6), mat.Float(7),
				                mat.Float(8), mat.Float(9), mat.Float(10), mat.Float(11),
				                mat.Float(12), mat.Float(13), mat.Float(14), mat.Float(15)));
				break;
			default:
				UNREACHABLE("GLSLstd450Determinant can only operate with square matrices. Got %d elements", int(mat.componentCount));
			}
		}
		break;
	case GLSLstd450MatrixInverse:
		{
			auto mat = Operand(this, state, insn.word(5));

			switch(mat.componentCount)
			{
			case 4:  // 2x2
				{
					auto inv = MatrixInverse(
					    mat.Float(0), mat.Float(1),
					    mat.Float(2), mat.Float(3));
					for(uint32_t i = 0; i < inv.size(); i++)
					{
						dst.move(i, inv[i]);
					}
				}
				break;
			case 9:  // 3x3
				{
					auto inv = MatrixInverse(
					    mat.Float(0), mat.Float(1), mat.Float(2),
					    mat.Float(3), mat.Float(4), mat.Float(5),
					    mat.Float(6), mat.Float(7), mat.Float(8));
					for(uint32_t i = 0; i < inv.size(); i++)
					{
						dst.move(i, inv[i]);
					}
				}
				break;
			case 16:  // 4x4
				{
					auto inv = MatrixInverse(
					    mat.Float(0), mat.Float(1), mat.Float(2), mat.Float(3),
					    mat.Float(4), mat.Float(5), mat.Float(6), mat.Float(7),
					    mat.Float(8), mat.Float(9), mat.Float(10), mat.Float(11),
					    mat.Float(12), mat.Float(13), mat.Float(14), mat.Float(15));
					for(uint32_t i = 0; i < inv.size(); i++)
					{
						dst.move(i, inv[i]);
					}
				}
				break;
			default:
				UNREACHABLE("GLSLstd450MatrixInverse can only operate with square matrices. Got %d elements", int(mat.componentCount));
			}
		}
		break;
	case GLSLstd450IMix:
		{
			UNREACHABLE("GLSLstd450IMix has been removed from the specification");
		}
		break;
	case GLSLstd450PackDouble2x32:
		{
			UNSUPPORTED("SPIR-V Float64 Capability (GLSLstd450PackDouble2x32)");
		}
		break;
	case GLSLstd450UnpackDouble2x32:
		{
			UNSUPPORTED("SPIR-V Float64 Capability (GLSLstd450UnpackDouble2x32)");
		}
		break;
	case GLSLstd450FindILsb:
		{
			auto val = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				auto v = val.UInt(i);
				dst.move(i, Cttz(v, true) | CmpEQ(v, SIMD::UInt(0)));
			}
		}
		break;
	case GLSLstd450FindSMsb:
		{
			auto val = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				auto v = val.UInt(i) ^ As<SIMD::UInt>(CmpLT(val.Int(i), SIMD::Int(0)));
				dst.move(i, SIMD::UInt(31) - Ctlz(v, false));
			}
		}
		break;
	case GLSLstd450FindUMsb:
		{
			auto val = Operand(this, state, insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, SIMD::UInt(31) - Ctlz(val.UInt(i), false));
			}
		}
		break;
	case GLSLstd450InterpolateAtCentroid:
		{
			Decorations d = GetDecorationsForId(insn.word(5));
			auto ptr = state->getPointer(insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Interpolate(ptr, d.Location, 0, i, state, SpirvShader::Centroid));
			}
		}
		break;
	case GLSLstd450InterpolateAtSample:
		{
			Decorations d = GetDecorationsForId(insn.word(5));
			auto ptr = state->getPointer(insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Interpolate(ptr, d.Location, insn.word(6), i, state, SpirvShader::AtSample));
			}
		}
		break;
	case GLSLstd450InterpolateAtOffset:
		{
			Decorations d = GetDecorationsForId(insn.word(5));
			auto ptr = state->getPointer(insn.word(5));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, Interpolate(ptr, d.Location, insn.word(6), i, state, SpirvShader::AtOffset));
			}
		}
		break;
	case GLSLstd450NMin:
		{
			auto x = Operand(this, state, insn.word(5));
			auto y = Operand(this, state, insn.word(6));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, NMin(x.Float(i), y.Float(i)));
			}
		}
		break;
	case GLSLstd450NMax:
		{
			auto x = Operand(this, state, insn.word(5));
			auto y = Operand(this, state, insn.word(6));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				dst.move(i, NMax(x.Float(i), y.Float(i)));
			}
		}
		break;
	case GLSLstd450NClamp:
		{
			auto x = Operand(this, state, insn.word(5));
			auto minVal = Operand(this, state, insn.word(6));
			auto maxVal = Operand(this, state, insn.word(7));
			for(auto i = 0u; i < type.componentCount; i++)
			{
				auto clamp = NMin(NMax(x.Float(i), minVal.Float(i)), maxVal.Float(i));
				dst.move(i, clamp);
			}
		}
		break;
	default:
		UNREACHABLE("ExtInst %d", int(extInstIndex));
		break;
	}

	return EmitResult::Continue;
}

SIMD::Float SpirvShader::Interpolate(SIMD::Pointer const &ptr, int32_t location, Object::ID paramId,
                                     uint32_t component, EmitState *state, InterpolationType type) const
{
	uint32_t interpolant = (location * 4);
	uint32_t components_per_row = GetNumInputComponents(location);
	if((location < 0) || (interpolant >= inputs.size()) || (components_per_row == 0))
	{
		return SIMD::Float(0.0f);
	}

	const auto &interpolationData = state->routine->interpolationData;

	SIMD::Float x;
	SIMD::Float y;
	SIMD::Float rhw;

	switch(type)
	{
	case Centroid:
		x = interpolationData.xCentroid;
		y = interpolationData.yCentroid;
		rhw = interpolationData.rhwCentroid;
		break;
	case AtSample:
		x = SIMD::Float(0.0f);
		y = SIMD::Float(0.0f);

		if(state->getMultiSampleCount() > 1)
		{
			static constexpr int NUM_SAMPLES = 4;
			ASSERT(state->getMultiSampleCount() == NUM_SAMPLES);

			Array<Float> sampleX(NUM_SAMPLES);
			Array<Float> sampleY(NUM_SAMPLES);
			for(int i = 0; i < NUM_SAMPLES; ++i)
			{
				sampleX[i] = Constants::SampleLocationsX[i];
				sampleY[i] = Constants::SampleLocationsY[i];
			}

			auto sampleOperand = Operand(this, state, paramId);
			ASSERT(sampleOperand.componentCount == 1);

			// If sample does not exist, the position used to interpolate the
			// input variable is undefined, so we just clamp to avoid OOB accesses.
			SIMD::Int samples = sampleOperand.Int(0) & SIMD::Int(NUM_SAMPLES - 1);

			for(int i = 0; i < SIMD::Width; ++i)
			{
				Int sample = Extract(samples, i);
				x = Insert(x, sampleX[sample], i);
				y = Insert(y, sampleY[sample], i);
			}
		}

		x += interpolationData.x;
		y += interpolationData.y;
		rhw = interpolationData.rhw;
		break;
	case AtOffset:
		{
			//  An offset of (0, 0) identifies the center of the pixel.
			auto offset = Operand(this, state, paramId);
			ASSERT(offset.componentCount == 2);

			x = interpolationData.x + offset.Float(0);
			y = interpolationData.y + offset.Float(1);
			rhw = interpolationData.rhw;
		}
		break;
	default:
		UNREACHABLE("Unknown interpolation type: %d", (int)type);
		return SIMD::Float(0.0f);
	}

	uint32_t packedInterpolant = GetPackedInterpolant(location);
	Pointer<Byte> planeEquation = interpolationData.primitive + OFFSET(Primitive, V[packedInterpolant]);
	if(ptr.hasDynamicOffsets)
	{
		// Combine plane equations into one
		SIMD::Float A;
		SIMD::Float B;
		SIMD::Float C;

		for(int i = 0; i < SIMD::Width; ++i)
		{
			Int offset = ((Extract(ptr.dynamicOffsets, i) + ptr.staticOffsets[i]) >> 2) + component;
			Pointer<Byte> planeEquationI = planeEquation + (offset * sizeof(PlaneEquation));
			A = Insert(A, Extract(*Pointer<SIMD::Float>(planeEquationI + OFFSET(PlaneEquation, A), 16), i), i);
			B = Insert(B, Extract(*Pointer<SIMD::Float>(planeEquationI + OFFSET(PlaneEquation, B), 16), i), i);
			C = Insert(C, Extract(*Pointer<SIMD::Float>(planeEquationI + OFFSET(PlaneEquation, C), 16), i), i);
		}
		return ::Interpolate(x, y, rhw, A, B, C, false, true);
	}
	else
	{
		ASSERT(ptr.hasStaticEqualOffsets());

		uint32_t offset = (ptr.staticOffsets[0] >> 2) + component;
		if((interpolant + offset) >= inputs.size())
		{
			return SIMD::Float(0.0f);
		}
		planeEquation += offset * sizeof(PlaneEquation);
	}

	return SpirvRoutine::interpolateAtXY(x, y, rhw, planeEquation, false, true);
}

SIMD::Float SpirvRoutine::interpolateAtXY(const SIMD::Float &x, const SIMD::Float &y, const SIMD::Float &rhw, Pointer<Byte> planeEquation, bool flat, bool perspective)
{
	SIMD::Float A;
	SIMD::Float B;
	SIMD::Float C = *Pointer<SIMD::Float>(planeEquation + OFFSET(PlaneEquation, C), 16);

	if(!flat)
	{
		A = *Pointer<SIMD::Float>(planeEquation + OFFSET(PlaneEquation, A), 16);
		B = *Pointer<SIMD::Float>(planeEquation + OFFSET(PlaneEquation, B), 16);
	}

	return ::Interpolate(x, y, rhw, A, B, C, flat, perspective);
}

}  // namespace sw