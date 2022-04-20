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

#include "ShaderCore.hpp"

#include "Device/Renderer.hpp"
#include "System/Debug.hpp"

#include <limits.h>

// TODO(chromium:1299047)
#ifndef SWIFTSHADER_LEGACY_PRECISION
#	define SWIFTSHADER_LEGACY_PRECISION false
#endif

namespace sw {

Vector4s::Vector4s()
{
}

Vector4s::Vector4s(unsigned short x, unsigned short y, unsigned short z, unsigned short w)
{
	this->x = Short4(x);
	this->y = Short4(y);
	this->z = Short4(z);
	this->w = Short4(w);
}

Vector4s::Vector4s(const Vector4s &rhs)
{
	x = rhs.x;
	y = rhs.y;
	z = rhs.z;
	w = rhs.w;
}

Vector4s &Vector4s::operator=(const Vector4s &rhs)
{
	x = rhs.x;
	y = rhs.y;
	z = rhs.z;
	w = rhs.w;

	return *this;
}

Short4 &Vector4s::operator[](int i)
{
	switch(i)
	{
	case 0: return x;
	case 1: return y;
	case 2: return z;
	case 3: return w;
	}

	return x;
}

Vector4f::Vector4f()
{
}

Vector4f::Vector4f(float x, float y, float z, float w)
{
	this->x = Float4(x);
	this->y = Float4(y);
	this->z = Float4(z);
	this->w = Float4(w);
}

Vector4f::Vector4f(const Vector4f &rhs)
{
	x = rhs.x;
	y = rhs.y;
	z = rhs.z;
	w = rhs.w;
}

Vector4f &Vector4f::operator=(const Vector4f &rhs)
{
	x = rhs.x;
	y = rhs.y;
	z = rhs.z;
	w = rhs.w;

	return *this;
}

Float4 &Vector4f::operator[](int i)
{
	switch(i)
	{
	case 0: return x;
	case 1: return y;
	case 2: return z;
	case 3: return w;
	}

	return x;
}

Vector4i::Vector4i()
{
}

Vector4i::Vector4i(int x, int y, int z, int w)
{
	this->x = Int4(x);
	this->y = Int4(y);
	this->z = Int4(z);
	this->w = Int4(w);
}

Vector4i::Vector4i(const Vector4i &rhs)
{
	x = rhs.x;
	y = rhs.y;
	z = rhs.z;
	w = rhs.w;
}

Vector4i &Vector4i::operator=(const Vector4i &rhs)
{
	x = rhs.x;
	y = rhs.y;
	z = rhs.z;
	w = rhs.w;

	return *this;
}

Int4 &Vector4i::operator[](int i)
{
	switch(i)
	{
	case 0: return x;
	case 1: return y;
	case 2: return z;
	case 3: return w;
	}

	return x;
}

// Approximation of atan in [0..1]
static RValue<Float4> Atan_01(Float4 x)
{
	// From 4.4.49, page 81 of the Handbook of Mathematical Functions, by Milton Abramowitz and Irene Stegun
	const Float4 a2(-0.3333314528f);
	const Float4 a4(0.1999355085f);
	const Float4 a6(-0.1420889944f);
	const Float4 a8(0.1065626393f);
	const Float4 a10(-0.0752896400f);
	const Float4 a12(0.0429096138f);
	const Float4 a14(-0.0161657367f);
	const Float4 a16(0.0028662257f);
	Float4 x2 = x * x;
	return (x + x * (x2 * (a2 + x2 * (a4 + x2 * (a6 + x2 * (a8 + x2 * (a10 + x2 * (a12 + x2 * (a14 + x2 * a16)))))))));
}

// Polynomial approximation of order 5 for sin(x * 2 * pi) in the range [-1/4, 1/4]
static RValue<Float4> Sin5(Float4 x)
{
	// A * x^5 + B * x^3 + C * x
	// Exact at x = 0, 1/12, 1/6, 1/4, and their negatives, which correspond to x * 2 * pi = 0, pi/6, pi/3, pi/2
	const Float4 A = (36288 - 20736 * sqrt(3)) / 5;
	const Float4 B = 288 * sqrt(3) - 540;
	const Float4 C = (47 - 9 * sqrt(3)) / 5;

	Float4 x2 = x * x;

	return MulAdd(MulAdd(A, x2, B), x2, C) * x;
}

RValue<Float4> Sin(RValue<Float4> x, bool relaxedPrecision)
{
	const Float4 q = 0.25f;
	const Float4 pi2 = 1 / (2 * 3.1415926535f);

	// Range reduction and mirroring
	Float4 x_2 = MulAdd(x, -pi2, q);
	Float4 z = q - Abs(x_2 - Round(x_2));

	return Sin5(z);
}

RValue<Float4> Cos(RValue<Float4> x, bool relaxedPrecision)
{
	const Float4 q = 0.25f;
	const Float4 pi2 = 1 / (2 * 3.1415926535f);

	// Phase shift, range reduction, and mirroring
	Float4 x_2 = x * pi2;
	Float4 z = q - Abs(x_2 - Round(x_2));

	return Sin5(z);
}

RValue<Float4> Tan(RValue<Float4> x, bool relaxedPrecision)
{
	return sw::Sin(x, relaxedPrecision) / sw::Cos(x, relaxedPrecision);
}

static RValue<Float4> Asin_4_terms(RValue<Float4> x)
{
	// From 4.4.45, page 81 of the Handbook of Mathematical Functions, by Milton Abramowitz and Irene Stegun
	// |e(x)| <= 5e-8
	const Float4 half_pi(1.57079632f);
	const Float4 a0(1.5707288f);
	const Float4 a1(-0.2121144f);
	const Float4 a2(0.0742610f);
	const Float4 a3(-0.0187293f);
	Float4 absx = Abs(x);
	return As<Float4>(As<Int4>(half_pi - Sqrt<Highp>(1.0f - absx) * (a0 + absx * (a1 + absx * (a2 + absx * a3)))) ^
	                  (As<Int4>(x) & Int4(0x80000000)));
}

static RValue<Float4> Asin_8_terms(RValue<Float4> x)
{
	// From 4.4.46, page 81 of the Handbook of Mathematical Functions, by Milton Abramowitz and Irene Stegun
	// |e(x)| <= 0e-8
	const Float4 half_pi(1.5707963268f);
	const Float4 a0(1.5707963050f);
	const Float4 a1(-0.2145988016f);
	const Float4 a2(0.0889789874f);
	const Float4 a3(-0.0501743046f);
	const Float4 a4(0.0308918810f);
	const Float4 a5(-0.0170881256f);
	const Float4 a6(0.006700901f);
	const Float4 a7(-0.0012624911f);
	Float4 absx = Abs(x);
	return As<Float4>(As<Int4>(half_pi - Sqrt<Highp>(1.0f - absx) * (a0 + absx * (a1 + absx * (a2 + absx * (a3 + absx * (a4 + absx * (a5 + absx * (a6 + absx * a7)))))))) ^
	                  (As<Int4>(x) & Int4(0x80000000)));
}

RValue<Float4> Asin(RValue<Float4> x, bool relaxedPrecision)
{
	// TODO(b/169755566): Surprisingly, deqp-vk's precision.acos.highp/mediump tests pass when using the 4-term polynomial
	// approximation version of acos, unlike for Asin, which requires higher precision algorithms.

	if(!relaxedPrecision)
	{
		return rr::Asin(x);
	}

	return Asin_8_terms(x);
}

RValue<Float4> Acos(RValue<Float4> x, bool relaxedPrecision)
{
	// pi/2 - arcsin(x)
	return 1.57079632e+0f - Asin_4_terms(x);
}

RValue<Float4> Atan(RValue<Float4> x, bool relaxedPrecision)
{
	Float4 absx = Abs(x);
	Int4 O = CmpNLT(absx, 1.0f);
	Float4 y = As<Float4>((O & As<Int4>(1.0f / absx)) | (~O & As<Int4>(absx)));  // FIXME: Vector select

	const Float4 half_pi(1.57079632f);
	Float4 theta = Atan_01(y);
	return As<Float4>(((O & As<Int4>(half_pi - theta)) | (~O & As<Int4>(theta))) ^  // FIXME: Vector select
	                  (As<Int4>(x) & Int4(0x80000000)));
}

RValue<Float4> Atan2(RValue<Float4> y, RValue<Float4> x, bool relaxedPrecision)
{
	const Float4 pi(3.14159265f);             // pi
	const Float4 minus_pi(-3.14159265f);      // -pi
	const Float4 half_pi(1.57079632f);        // pi/2
	const Float4 quarter_pi(7.85398163e-1f);  // pi/4

	// Rotate to upper semicircle when in lower semicircle
	Int4 S = CmpLT(y, 0.0f);
	Float4 theta = As<Float4>(S & As<Int4>(minus_pi));
	Float4 x0 = As<Float4>((As<Int4>(y) & Int4(0x80000000)) ^ As<Int4>(x));
	Float4 y0 = Abs(y);

	// Rotate to right quadrant when in left quadrant
	Int4 Q = CmpLT(x0, 0.0f);
	theta += As<Float4>(Q & As<Int4>(half_pi));
	Float4 x1 = As<Float4>((Q & As<Int4>(y0)) | (~Q & As<Int4>(x0)));   // FIXME: Vector select
	Float4 y1 = As<Float4>((Q & As<Int4>(-x0)) | (~Q & As<Int4>(y0)));  // FIXME: Vector select

	// Mirror to first octant when in second octant
	Int4 O = CmpNLT(y1, x1);
	Float4 x2 = As<Float4>((O & As<Int4>(y1)) | (~O & As<Int4>(x1)));  // FIXME: Vector select
	Float4 y2 = As<Float4>((O & As<Int4>(x1)) | (~O & As<Int4>(y1)));  // FIXME: Vector select

	// Approximation of atan in [0..1]
	Int4 zero_x = CmpEQ(x2, 0.0f);
	Int4 inf_y = IsInf(y2);  // Since x2 >= y2, this means x2 == y2 == inf, so we use 45 degrees or pi/4
	Float4 atan2_theta = Atan_01(y2 / x2);
	theta += As<Float4>((~zero_x & ~inf_y & ((O & As<Int4>(half_pi - atan2_theta)) | (~O & (As<Int4>(atan2_theta))))) |  // FIXME: Vector select
	                    (inf_y & As<Int4>(quarter_pi)));

	// Recover loss of precision for tiny theta angles
	// This combination results in (-pi + half_pi + half_pi - atan2_theta) which is equivalent to -atan2_theta
	Int4 precision_loss = S & Q & O & ~inf_y;

	return As<Float4>((precision_loss & As<Int4>(-atan2_theta)) | (~precision_loss & As<Int4>(theta)));  // FIXME: Vector select
}

// TODO(chromium:1299047)
static RValue<Float4> Exp2_legacy(RValue<Float4> x0)
{
	Int4 i = RoundInt(x0 - 0.5f);
	Float4 ii = As<Float4>((i + Int4(127)) << 23);

	Float4 f = x0 - Float4(i);
	Float4 ff = As<Float4>(Int4(0x3AF61905));
	ff = ff * f + As<Float4>(Int4(0x3C134806));
	ff = ff * f + As<Float4>(Int4(0x3D64AA23));
	ff = ff * f + As<Float4>(Int4(0x3E75EAD4));
	ff = ff * f + As<Float4>(Int4(0x3F31727B));
	ff = ff * f + 1.0f;

	return ii * ff;
}

RValue<Float4> Exp2(RValue<Float4> x, bool relaxedPrecision)
{
	// Clamp to prevent overflow past the representation of infinity.
	Float4 x0 = x;
	x0 = Min(x0, 128.0f);
	x0 = Max(x0, As<Float4>(Int4(0xC2FDFFFF)));  // -126.999992

	if(SWIFTSHADER_LEGACY_PRECISION)  // TODO(chromium:1299047)
	{
		return Exp2_legacy(x0);
	}

	Float4 xi = Floor(x0);
	Float4 f = x0 - xi;

	if(!relaxedPrecision)  // highp
	{
		// Polynomial which approximates (2^x-x-1)/x. Multiplying with x
		// gives us a correction term to be added to 1+x to obtain 2^x.
		const Float4 a = 1.8852974e-3f;
		const Float4 b = 8.9733787e-3f;
		const Float4 c = 5.5835927e-2f;
		const Float4 d = 2.4015281e-1f;
		const Float4 e = -3.0684753e-1f;

		Float4 r = MulAdd(MulAdd(MulAdd(MulAdd(a, f, b), f, c), f, d), f, e);

		// bit_cast<float>(int(x * 2^23)) is a piecewise linear approximation of 2^x.
		// See "Fast Exponential Computation on SIMD Architectures" by Malossi et al.
		Float4 y = MulAdd(r, f, x0);
		Int4 i = Int4(y * (1 << 23)) + (127 << 23);

		return As<Float4>(i);
	}
	else  // RelaxedPrecision / mediump
	{
		// Polynomial which approximates (2^x-x-1)/x. Multiplying with x
		// gives us a correction term to be added to 1+x to obtain 2^x.
		const Float4 a = 7.8145574e-2f;
		const Float4 b = 2.2617357e-1f;
		const Float4 c = -3.0444314e-1f;

		Float4 r = MulAdd(MulAdd(a, f, b), f, c);

		// bit_cast<float>(int(x * 2^23)) is a piecewise linear approximation of 2^x.
		// See "Fast Exponential Computation on SIMD Architectures" by Malossi et al.
		Float4 y = MulAdd(r, f, x0);
		Int4 i = Int4(MulAdd((1 << 23), y, (127 << 23)));

		return As<Float4>(i);
	}
}

RValue<Float4> Log2_legacy(RValue<Float4> x)
{
	Float4 x1 = As<Float4>(As<Int4>(x) & Int4(0x7F800000));
	x1 = As<Float4>(As<UInt4>(x1) >> 8);
	x1 = As<Float4>(As<Int4>(x1) | As<Int4>(Float4(1.0f)));
	x1 = (x1 - 1.4960938f) * 256.0f;
	Float4 x0 = As<Float4>((As<Int4>(x) & Int4(0x007FFFFF)) | As<Int4>(Float4(1.0f)));

	Float4 x2 = MulAdd(MulAdd(9.5428179e-2f, x0, 4.7779095e-1f), x0, 1.9782813e-1f);
	Float4 x3 = MulAdd(MulAdd(MulAdd(1.6618466e-2f, x0, 2.0350508e-1f), x0, 2.7382900e-1f), x0, 4.0496687e-2f);

	x1 += (x0 - 1.0f) * (x2 / x3);

	Int4 pos_inf_x = CmpEQ(As<Int4>(x), Int4(0x7F800000));
	return As<Float4>((pos_inf_x & As<Int4>(x)) | (~pos_inf_x & As<Int4>(x1)));
}

RValue<Float4> Log2(RValue<Float4> x, bool relaxedPrecision)
{
	if(SWIFTSHADER_LEGACY_PRECISION)  // TODO(chromium:1299047)
	{
		return Log2_legacy(x);
	}

	if(!relaxedPrecision)  // highp
	{
		// Reinterpretation as an integer provides a piecewise linear
		// approximation of log2(). Scale to the radix and subtract exponent bias.
		Int4 im = As<Int4>(x);
		Float4 y = Float4(im - (127 << 23)) * (1.0f / (1 << 23));

		// Handle log2(inf) = inf.
		y = As<Float4>(As<Int4>(y) | (CmpEQ(im, 0x7F800000) & As<Int4>(Float4::infinity())));

		Float4 m = Float4(im & 0x007FFFFF) * (1.0f / (1 << 23));  // Normalized mantissa of x.

		// Add a polynomial approximation of log2(m+1)-m to the result's mantissa.
		const Float4 a = -9.3091638e-3f;
		const Float4 b = 5.2059003e-2f;
		const Float4 c = -1.3752135e-1f;
		const Float4 d = 2.4186478e-1f;
		const Float4 e = -3.4730109e-1f;
		const Float4 f = 4.786837e-1f;
		const Float4 g = -7.2116581e-1f;
		const Float4 h = 4.4268988e-1f;

		Float4 z = MulAdd(MulAdd(MulAdd(MulAdd(MulAdd(MulAdd(MulAdd(a, m, b), m, c), m, d), m, e), m, f), m, g), m, h);

		return MulAdd(z, m, y);
	}
	else  // RelaxedPrecision / mediump
	{
		// Reinterpretation as an integer provides a piecewise linear
		// approximation of log2(). Scale to the radix and subtract exponent bias.
		Int4 im = As<Int4>(x);
		Float4 y = MulAdd(Float4(im), (1.0f / (1 << 23)), -127.0f);

		// Handle log2(inf) = inf.
		y = As<Float4>(As<Int4>(y) | (CmpEQ(im, 0x7F800000) & As<Int4>(Float4::infinity())));

		Float4 m = Float4(im & 0x007FFFFF);  // Unnormalized mantissa of x.

		// Add a polynomial approximation of log2(m+1)-m to the result's mantissa.
		const Float4 a = 2.8017103e-22f;
		const Float4 b = -8.373131e-15f;
		const Float4 c = 5.0615534e-8f;

		Float4 f = MulAdd(MulAdd(a, m, b), m, c);

		return MulAdd(f, m, y);
	}
}

RValue<Float4> Exp(RValue<Float4> x, bool relaxedPrecision)
{
	return sw::Exp2(1.44269504f * x, relaxedPrecision);  // 1/ln(2)
}

RValue<Float4> Log(RValue<Float4> x, bool relaxedPrecision)
{
	return 6.93147181e-1f * sw::Log2(x, relaxedPrecision);  // ln(2)
}

RValue<Float4> Pow(RValue<Float4> x, RValue<Float4> y, bool relaxedPrecision)
{
	Float4 log = sw::Log2(x, relaxedPrecision);
	log *= y;
	return sw::Exp2(log, relaxedPrecision);
}

RValue<Float4> Sinh(RValue<Float4> x, bool relaxedPrecision)
{
	return (sw::Exp(x, relaxedPrecision) - sw::Exp(-x, relaxedPrecision)) * 0.5f;
}

RValue<Float4> Cosh(RValue<Float4> x, bool relaxedPrecision)
{
	return (sw::Exp(x, relaxedPrecision) + sw::Exp(-x, relaxedPrecision)) * 0.5f;
}

RValue<Float4> Tanh(RValue<Float4> x, bool relaxedPrecision)
{
	Float4 e_x = sw::Exp(x, relaxedPrecision);
	Float4 e_minus_x = sw::Exp(-x, relaxedPrecision);
	return (e_x - e_minus_x) / (e_x + e_minus_x);
}

RValue<Float4> Asinh(RValue<Float4> x, bool relaxedPrecision)
{
	return sw::Log(x + Sqrt(x * x + 1.0f, relaxedPrecision), relaxedPrecision);
}

RValue<Float4> Acosh(RValue<Float4> x, bool relaxedPrecision)
{
	return sw::Log(x + Sqrt(x + 1.0f, relaxedPrecision) * Sqrt(x - 1.0f, relaxedPrecision), relaxedPrecision);
}

RValue<Float4> Atanh(RValue<Float4> x, bool relaxedPrecision)
{
	return sw::Log((1.0f + x) / (1.0f - x), relaxedPrecision) * 0.5f;
}

RValue<Float4> Sqrt(RValue<Float4> x, bool relaxedPrecision)
{
	return rr::Sqrt(x);  // TODO(b/222218659): Optimize for relaxed precision.
}

RValue<Float4> reciprocal(RValue<Float4> x, bool pp, bool exactAtPow2)
{
	return Rcp(x, pp, exactAtPow2);
}

RValue<Float4> reciprocalSquareRoot(RValue<Float4> x, bool absolute, bool pp)
{
	Float4 abs = x;

	if(absolute)
	{
		abs = Abs(abs);
	}

	return Rcp(abs, pp);
}

// TODO(chromium:1299047): Eliminate when Chromium tests accept both fused and unfused multiply-add.
RValue<Float4> mulAdd(RValue<Float4> x, RValue<Float4> y, RValue<Float4> z)
{
	if(SWIFTSHADER_LEGACY_PRECISION)
	{
		return x * y + z;
	}

	return rr::MulAdd(x, y, z);
}

void transpose4x4(Short4 &row0, Short4 &row1, Short4 &row2, Short4 &row3)
{
	Int2 tmp0 = UnpackHigh(row0, row1);
	Int2 tmp1 = UnpackHigh(row2, row3);
	Int2 tmp2 = UnpackLow(row0, row1);
	Int2 tmp3 = UnpackLow(row2, row3);

	row0 = UnpackLow(tmp2, tmp3);
	row1 = UnpackHigh(tmp2, tmp3);
	row2 = UnpackLow(tmp0, tmp1);
	row3 = UnpackHigh(tmp0, tmp1);
}

void transpose4x3(Short4 &row0, Short4 &row1, Short4 &row2, Short4 &row3)
{
	Int2 tmp0 = UnpackHigh(row0, row1);
	Int2 tmp1 = UnpackHigh(row2, row3);
	Int2 tmp2 = UnpackLow(row0, row1);
	Int2 tmp3 = UnpackLow(row2, row3);

	row0 = UnpackLow(tmp2, tmp3);
	row1 = UnpackHigh(tmp2, tmp3);
	row2 = UnpackLow(tmp0, tmp1);
}

void transpose4x4(Float4 &row0, Float4 &row1, Float4 &row2, Float4 &row3)
{
	Float4 tmp0 = UnpackLow(row0, row1);
	Float4 tmp1 = UnpackLow(row2, row3);
	Float4 tmp2 = UnpackHigh(row0, row1);
	Float4 tmp3 = UnpackHigh(row2, row3);

	row0 = Float4(tmp0.xy, tmp1.xy);
	row1 = Float4(tmp0.zw, tmp1.zw);
	row2 = Float4(tmp2.xy, tmp3.xy);
	row3 = Float4(tmp2.zw, tmp3.zw);
}

void transpose4x3(Float4 &row0, Float4 &row1, Float4 &row2, Float4 &row3)
{
	Float4 tmp0 = UnpackLow(row0, row1);
	Float4 tmp1 = UnpackLow(row2, row3);
	Float4 tmp2 = UnpackHigh(row0, row1);
	Float4 tmp3 = UnpackHigh(row2, row3);

	row0 = Float4(tmp0.xy, tmp1.xy);
	row1 = Float4(tmp0.zw, tmp1.zw);
	row2 = Float4(tmp2.xy, tmp3.xy);
}

void transpose4x2(Float4 &row0, Float4 &row1, Float4 &row2, Float4 &row3)
{
	Float4 tmp0 = UnpackLow(row0, row1);
	Float4 tmp1 = UnpackLow(row2, row3);

	row0 = Float4(tmp0.xy, tmp1.xy);
	row1 = Float4(tmp0.zw, tmp1.zw);
}

void transpose4x1(Float4 &row0, Float4 &row1, Float4 &row2, Float4 &row3)
{
	Float4 tmp0 = UnpackLow(row0, row1);
	Float4 tmp1 = UnpackLow(row2, row3);

	row0 = Float4(tmp0.xy, tmp1.xy);
}

void transpose2x4(Float4 &row0, Float4 &row1, Float4 &row2, Float4 &row3)
{
	Float4 tmp01 = UnpackLow(row0, row1);
	Float4 tmp23 = UnpackHigh(row0, row1);

	row0 = tmp01;
	row1 = Float4(tmp01.zw, row1.zw);
	row2 = tmp23;
	row3 = Float4(tmp23.zw, row3.zw);
}

void transpose4xN(Float4 &row0, Float4 &row1, Float4 &row2, Float4 &row3, int N)
{
	switch(N)
	{
	case 1: transpose4x1(row0, row1, row2, row3); break;
	case 2: transpose4x2(row0, row1, row2, row3); break;
	case 3: transpose4x3(row0, row1, row2, row3); break;
	case 4: transpose4x4(row0, row1, row2, row3); break;
	}
}

SIMD::UInt halfToFloatBits(SIMD::UInt halfBits)
{
	auto magic = SIMD::UInt(126 << 23);

	auto sign16 = halfBits & SIMD::UInt(0x8000);
	auto man16 = halfBits & SIMD::UInt(0x03FF);
	auto exp16 = halfBits & SIMD::UInt(0x7C00);

	auto isDnormOrZero = CmpEQ(exp16, SIMD::UInt(0));
	auto isInfOrNaN = CmpEQ(exp16, SIMD::UInt(0x7C00));

	auto sign32 = sign16 << 16;
	auto man32 = man16 << 13;
	auto exp32 = (exp16 + SIMD::UInt(0x1C000)) << 13;
	auto norm32 = (man32 | exp32) | (isInfOrNaN & SIMD::UInt(0x7F800000));

	auto denorm32 = As<SIMD::UInt>(As<SIMD::Float>(magic + man16) - As<SIMD::Float>(magic));

	return sign32 | (norm32 & ~isDnormOrZero) | (denorm32 & isDnormOrZero);
}

SIMD::UInt floatToHalfBits(SIMD::UInt floatBits, bool storeInUpperBits)
{
	SIMD::UInt sign = floatBits & SIMD::UInt(0x80000000);
	SIMD::UInt abs = floatBits & SIMD::UInt(0x7FFFFFFF);

	SIMD::UInt normal = CmpNLE(abs, SIMD::UInt(0x38800000));

	SIMD::UInt mantissa = (abs & SIMD::UInt(0x007FFFFF)) | SIMD::UInt(0x00800000);
	SIMD::UInt e = SIMD::UInt(113) - (abs >> 23);
	SIMD::UInt denormal = CmpLT(e, SIMD::UInt(24)) & (mantissa >> e);

	SIMD::UInt base = (normal & abs) | (~normal & denormal);  // TODO: IfThenElse()

	// float exponent bias is 127, half bias is 15, so adjust by -112
	SIMD::UInt bias = normal & SIMD::UInt(0xC8000000);

	SIMD::UInt rounded = base + bias + SIMD::UInt(0x00000FFF) + ((base >> 13) & SIMD::UInt(1));
	SIMD::UInt fp16u = rounded >> 13;

	// Infinity
	fp16u |= CmpNLE(abs, SIMD::UInt(0x47FFEFFF)) & SIMD::UInt(0x7FFF);

	return storeInUpperBits ? (sign | (fp16u << 16)) : ((sign >> 16) | fp16u);
}

Float4 r11g11b10Unpack(UInt r11g11b10bits)
{
	// 10 (or 11) bit float formats are unsigned formats with a 5 bit exponent and a 5 (or 6) bit mantissa.
	// Since the Half float format also has a 5 bit exponent, we can convert these formats to half by
	// copy/pasting the bits so the the exponent bits and top mantissa bits are aligned to the half format.
	// In this case, we have:
	// MSB | B B B B B B B B B B G G G G G G G G G G G R R R R R R R R R R R | LSB
	UInt4 halfBits;
	halfBits = Insert(halfBits, (r11g11b10bits & UInt(0x000007FFu)) << 4, 0);
	halfBits = Insert(halfBits, (r11g11b10bits & UInt(0x003FF800u)) >> 7, 1);
	halfBits = Insert(halfBits, (r11g11b10bits & UInt(0xFFC00000u)) >> 17, 2);
	halfBits = Insert(halfBits, UInt(0x00003C00u), 3);
	return As<Float4>(halfToFloatBits(halfBits));
}

UInt r11g11b10Pack(const Float4 &value)
{
	// 10 and 11 bit floats are unsigned, so their minimal value is 0
	auto halfBits = floatToHalfBits(As<UInt4>(Max(value, Float4(0.0f))), true);
	// Truncates instead of rounding. See b/147900455
	UInt4 truncBits = halfBits & UInt4(0x7FF00000, 0x7FF00000, 0x7FE00000, 0);
	return (UInt(truncBits.x) >> 20) | (UInt(truncBits.y) >> 9) | (UInt(truncBits.z) << 1);
}

Float4 linearToSRGB(const Float4 &c)
{
	Float4 lc = Min(c, 0.0031308f) * 12.92f;
	Float4 ec = MulAdd(1.055f, Pow<Mediump>(c, (1.0f / 2.4f)), -0.055f);  // TODO(b/149574741): Use a custom approximation.

	return Max(lc, ec);
}

Float4 sRGBtoLinear(const Float4 &c)
{
	Float4 lc = c * (1.0f / 12.92f);
	Float4 ec = Pow<Mediump>(MulAdd(c, 1.0f / 1.055f, 0.055f / 1.055f), 2.4f);  // TODO(b/149574741): Use a custom approximation.

	Int4 linear = CmpLT(c, 0.04045f);
	return As<Float4>((linear & As<Int4>(lc)) | (~linear & As<Int4>(ec)));  // TODO: IfThenElse()
}

RValue<Bool> AnyTrue(const RValue<SIMD::Int> &bools)
{
	return SignMask(bools) != 0;
}

RValue<Bool> AnyFalse(const RValue<SIMD::Int> &bools)
{
	return SignMask(~bools) != 0;  // TODO(b/214588983): Compare against mask of SIMD::Width 1's to avoid bitwise NOT.
}

RValue<Bool> AllTrue(const RValue<SIMD::Int> &bools)
{
	return SignMask(~bools) == 0;  // TODO(b/214588983): Compare against mask of SIMD::Width 1's to avoid bitwise NOT.
}

RValue<Bool> AllFalse(const RValue<SIMD::Int> &bools)
{
	return SignMask(bools) == 0;
}

RValue<Bool> Divergent(const RValue<SIMD::Int> &ints)
{
	auto broadcastFirst = SIMD::Int(Extract(ints, 0));
	return AnyTrue(CmpNEQ(broadcastFirst, ints));
}

RValue<Bool> Divergent(const RValue<SIMD::Float> &floats)
{
	auto broadcastFirst = SIMD::Float(Extract(floats, 0));
	return AnyTrue(CmpNEQ(broadcastFirst, floats));
}

RValue<Bool> Uniform(const RValue<SIMD::Int> &ints)
{
	auto broadcastFirst = SIMD::Int(Extract(ints, 0));
	return AllFalse(CmpNEQ(broadcastFirst, ints));
}

RValue<Bool> Uniform(const RValue<SIMD::Float> &floats)
{
	auto broadcastFirst = SIMD::Float(rr::Extract(floats, 0));
	return AllFalse(CmpNEQ(broadcastFirst, floats));
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

// Returns the number of 1s in bits, per lane.
sw::SIMD::UInt CountBits(rr::RValue<sw::SIMD::UInt> const &bits)
{
	// TODO: Add an intrinsic to reactor. Even if there isn't a
	// single vector instruction, there may be target-dependent
	// ways to make this faster.
	// https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
	sw::SIMD::UInt c = bits - ((bits >> 1) & sw::SIMD::UInt(0x55555555));
	c = ((c >> 2) & sw::SIMD::UInt(0x33333333)) + (c & sw::SIMD::UInt(0x33333333));
	c = ((c >> 4) + c) & sw::SIMD::UInt(0x0F0F0F0F);
	c = ((c >> 8) + c) & sw::SIMD::UInt(0x00FF00FF);
	c = ((c >> 16) + c) & sw::SIMD::UInt(0x0000FFFF);
	return c;
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
	auto xIsNan = IsNan(x);
	auto yIsNan = IsNan(y);
	return As<sw::SIMD::Float>(
	    // If neither are NaN, return min
	    ((~xIsNan & ~yIsNan) & As<sw::SIMD::Int>(Min(x, y))) |
	    // If one operand is a NaN, the other operand is the result
	    // If both operands are NaN, the result is a NaN.
	    ((~xIsNan & yIsNan) & As<sw::SIMD::Int>(x)) |
	    (xIsNan & As<sw::SIMD::Int>(y)));
}

// Returns y if y > x; otherwise result is x.
// If one operand is a NaN, the other operand is the result.
// If both operands are NaN, the result is a NaN.
rr::RValue<sw::SIMD::Float> NMax(rr::RValue<sw::SIMD::Float> const &x, rr::RValue<sw::SIMD::Float> const &y)
{
	auto xIsNan = IsNan(x);
	auto yIsNan = IsNan(y);
	return As<sw::SIMD::Float>(
	    // If neither are NaN, return max
	    ((~xIsNan & ~yIsNan) & As<sw::SIMD::Int>(Max(x, y))) |
	    // If one operand is a NaN, the other operand is the result
	    // If both operands are NaN, the result is a NaN.
	    ((~xIsNan & yIsNan) & As<sw::SIMD::Int>(x)) |
	    (xIsNan & As<sw::SIMD::Int>(y)));
}

// Returns the determinant of a 2x2 matrix.
rr::RValue<sw::SIMD::Float> Determinant(
    rr::RValue<sw::SIMD::Float> const &a, rr::RValue<sw::SIMD::Float> const &b,
    rr::RValue<sw::SIMD::Float> const &c, rr::RValue<sw::SIMD::Float> const &d)
{
	return a * d - b * c;
}

// Returns the determinant of a 3x3 matrix.
rr::RValue<sw::SIMD::Float> Determinant(
    rr::RValue<sw::SIMD::Float> const &a, rr::RValue<sw::SIMD::Float> const &b, rr::RValue<sw::SIMD::Float> const &c,
    rr::RValue<sw::SIMD::Float> const &d, rr::RValue<sw::SIMD::Float> const &e, rr::RValue<sw::SIMD::Float> const &f,
    rr::RValue<sw::SIMD::Float> const &g, rr::RValue<sw::SIMD::Float> const &h, rr::RValue<sw::SIMD::Float> const &i)
{
	return a * e * i + b * f * g + c * d * h - c * e * g - b * d * i - a * f * h;
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
	return { { s * d, -s * b, -s * c, s * a } };
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
	                                     g, h, i);  // TODO: duplicate arithmetic calculating the det and below.

	return { {
		s * (e * i - f * h),
		s * (c * h - b * i),
		s * (b * f - c * e),
		s * (f * g - d * i),
		s * (a * i - c * g),
		s * (c * d - a * f),
		s * (d * h - e * g),
		s * (b * g - a * h),
		s * (a * e - b * d),
	} };
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
	                                     m, n, o, p);  // TODO: duplicate arithmetic calculating the det and below.

	auto kplo = k * p - l * o, jpln = j * p - l * n, jokn = j * o - k * n;
	auto gpho = g * p - h * o, fphn = f * p - h * n, fogn = f * o - g * n;
	auto glhk = g * l - h * k, flhj = f * l - h * j, fkgj = f * k - g * j;
	auto iplm = i * p - l * m, iokm = i * o - k * m, ephm = e * p - h * m;
	auto eogm = e * o - g * m, elhi = e * l - h * i, ekgi = e * k - g * i;
	auto injm = i * n - j * m, enfm = e * n - f * m, ejfi = e * j - f * i;

	return { {
		s * (f * kplo - g * jpln + h * jokn),
		s * (-b * kplo + c * jpln - d * jokn),
		s * (b * gpho - c * fphn + d * fogn),
		s * (-b * glhk + c * flhj - d * fkgj),

		s * (-e * kplo + g * iplm - h * iokm),
		s * (a * kplo - c * iplm + d * iokm),
		s * (-a * gpho + c * ephm - d * eogm),
		s * (a * glhk - c * elhi + d * ekgi),

		s * (e * jpln - f * iplm + h * injm),
		s * (-a * jpln + b * iplm - d * injm),
		s * (a * fphn - b * ephm + d * enfm),
		s * (-a * flhj + b * elhi - d * ejfi),

		s * (-e * jokn + f * iokm - g * injm),
		s * (a * jokn - b * iokm + c * injm),
		s * (-a * fogn + b * eogm - c * enfm),
		s * (a * fkgj - b * ekgi + c * ejfi),
	} };
}

namespace SIMD {

Pointer::Pointer(rr::Pointer<Byte> base, rr::Int limit)
    : base(base)
    , dynamicLimit(limit)
    , staticLimit(0)
    , dynamicOffsets(0)
    , staticOffsets{}
    , hasDynamicLimit(true)
    , hasDynamicOffsets(false)
{}

Pointer::Pointer(rr::Pointer<Byte> base, unsigned int limit)
    : base(base)
    , dynamicLimit(0)
    , staticLimit(limit)
    , dynamicOffsets(0)
    , staticOffsets{}
    , hasDynamicLimit(false)
    , hasDynamicOffsets(false)
{}

Pointer::Pointer(rr::Pointer<Byte> base, rr::Int limit, SIMD::Int offset)
    : base(base)
    , dynamicLimit(limit)
    , staticLimit(0)
    , dynamicOffsets(offset)
    , staticOffsets{}
    , hasDynamicLimit(true)
    , hasDynamicOffsets(true)
{}

Pointer::Pointer(rr::Pointer<Byte> base, unsigned int limit, SIMD::Int offset)
    : base(base)
    , dynamicLimit(0)
    , staticLimit(limit)
    , dynamicOffsets(offset)
    , staticOffsets{}
    , hasDynamicLimit(false)
    , hasDynamicOffsets(true)
{}

Pointer &Pointer::operator+=(Int i)
{
	dynamicOffsets += i;
	hasDynamicOffsets = true;
	return *this;
}

Pointer &Pointer::operator*=(Int i)
{
	dynamicOffsets = offsets() * i;
	staticOffsets = {};
	hasDynamicOffsets = true;
	return *this;
}

Pointer Pointer::operator+(SIMD::Int i)
{
	Pointer p = *this;
	p += i;
	return p;
}
Pointer Pointer::operator*(SIMD::Int i)
{
	Pointer p = *this;
	p *= i;
	return p;
}

Pointer &Pointer::operator+=(int i)
{
	for(int el = 0; el < SIMD::Width; el++) { staticOffsets[el] += i; }
	return *this;
}

Pointer &Pointer::operator*=(int i)
{
	for(int el = 0; el < SIMD::Width; el++) { staticOffsets[el] *= i; }
	if(hasDynamicOffsets)
	{
		dynamicOffsets *= SIMD::Int(i);
	}
	return *this;
}

Pointer Pointer::operator+(int i)
{
	Pointer p = *this;
	p += i;
	return p;
}
Pointer Pointer::operator*(int i)
{
	Pointer p = *this;
	p *= i;
	return p;
}

SIMD::Int Pointer::offsets() const
{
	static_assert(SIMD::Width == 4, "Expects SIMD::Width to be 4");
	return dynamicOffsets + SIMD::Int(staticOffsets[0], staticOffsets[1], staticOffsets[2], staticOffsets[3]);
}

SIMD::Int Pointer::isInBounds(unsigned int accessSize, OutOfBoundsBehavior robustness) const
{
	ASSERT(accessSize > 0);

	if(isStaticallyInBounds(accessSize, robustness))
	{
		return SIMD::Int(0xffffffff);
	}

	if(!hasDynamicOffsets && !hasDynamicLimit)
	{
		// Common fast paths.
		static_assert(SIMD::Width == 4, "Expects SIMD::Width to be 4");
		return SIMD::Int(
		    (staticOffsets[0] + accessSize - 1 < staticLimit) ? 0xffffffff : 0,
		    (staticOffsets[1] + accessSize - 1 < staticLimit) ? 0xffffffff : 0,
		    (staticOffsets[2] + accessSize - 1 < staticLimit) ? 0xffffffff : 0,
		    (staticOffsets[3] + accessSize - 1 < staticLimit) ? 0xffffffff : 0);
	}

	return CmpGE(offsets(), SIMD::Int(0)) & CmpLT(offsets() + SIMD::Int(accessSize - 1), SIMD::Int(limit()));
}

bool Pointer::isStaticallyInBounds(unsigned int accessSize, OutOfBoundsBehavior robustness) const
{
	if(hasDynamicOffsets)
	{
		return false;
	}

	if(hasDynamicLimit)
	{
		if(hasStaticEqualOffsets() || hasStaticSequentialOffsets(accessSize))
		{
			switch(robustness)
			{
			case OutOfBoundsBehavior::UndefinedBehavior:
				// With this robustness setting the application/compiler guarantees in-bounds accesses on active lanes,
				// but since it can't know in advance which branches are taken this must be true even for inactives lanes.
				return true;
			case OutOfBoundsBehavior::Nullify:
			case OutOfBoundsBehavior::RobustBufferAccess:
			case OutOfBoundsBehavior::UndefinedValue:
				return false;
			}
		}
	}

	for(int i = 0; i < SIMD::Width; i++)
	{
		if(staticOffsets[i] + accessSize - 1 >= staticLimit)
		{
			return false;
		}
	}

	return true;
}

rr::Int Pointer::limit() const
{
	return dynamicLimit + staticLimit;
}

// Returns true if all offsets are sequential
// (N+0*step, N+1*step, N+2*step, N+3*step)
rr::Bool Pointer::hasSequentialOffsets(unsigned int step) const
{
	if(hasDynamicOffsets)
	{
		auto o = offsets();
		static_assert(SIMD::Width == 4, "Expects SIMD::Width to be 4");
		return rr::SignMask(~CmpEQ(o.yzww, o + SIMD::Int(1 * step, 2 * step, 3 * step, 0))) == 0;
	}
	return hasStaticSequentialOffsets(step);
}

// Returns true if all offsets are are compile-time static and
// sequential (N+0*step, N+1*step, N+2*step, N+3*step)
bool Pointer::hasStaticSequentialOffsets(unsigned int step) const
{
	if(hasDynamicOffsets)
	{
		return false;
	}
	for(int i = 1; i < SIMD::Width; i++)
	{
		if(staticOffsets[i - 1] + int32_t(step) != staticOffsets[i]) { return false; }
	}
	return true;
}

// Returns true if all offsets are equal (N, N, N, N)
rr::Bool Pointer::hasEqualOffsets() const
{
	if(hasDynamicOffsets)
	{
		auto o = offsets();
		static_assert(SIMD::Width == 4, "Expects SIMD::Width to be 4");
		return rr::SignMask(~CmpEQ(o, o.yzwx)) == 0;
	}
	return hasStaticEqualOffsets();
}

// Returns true if all offsets are compile-time static and are equal
// (N, N, N, N)
bool Pointer::hasStaticEqualOffsets() const
{
	if(hasDynamicOffsets)
	{
		return false;
	}
	for(int i = 1; i < SIMD::Width; i++)
	{
		if(staticOffsets[i - 1] != staticOffsets[i]) { return false; }
	}
	return true;
}

}  // namespace SIMD

}  // namespace sw
