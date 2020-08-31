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

#ifndef sw_Primitive_hpp
#define sw_Primitive_hpp

#include "Memset.hpp"
#include "Vertex.hpp"
#include "Device/Config.hpp"
#include "System/Build.hpp"

namespace sw {

struct Triangle MEMORY_SANITIZER_ONLY(
    : Memset<Triangle>)
{
#if MEMORY_SANITIZER_ENABLED
	// Memory sanitizer cannot 'see' writes from JIT'd code, and can raise
	// false-positives when read. By clearing the struct in the constructor,
	// we can avoid triggering these false-positives.
	inline Triangle()
	    : Memset<Triangle>(this, 0)
	{}
#endif  // MEMORY_SANITIZER_ENABLED

	Vertex v0;
	Vertex v1;
	Vertex v2;
};

struct PlaneEquation  // z = A * x + B * y + C
{
	float4 A;
	float4 B;
	float4 C;
};

struct Primitive MEMORY_SANITIZER_ONLY(
    : Memset<Primitive>)
{
#if MEMORY_SANITIZER_ENABLED
	// Memory sanitizer cannot 'see' writes from JIT'd code, and can raise
	// false-positives when read. By clearing the struct in the constructor,
	// we can avoid triggering these false-positives.
	inline Primitive()
	    : Memset<Primitive>(this, 0)
	{}
#endif  // MEMORY_SANITIZER_ENABLED

	int yMin;
	int yMax;

	float4 xQuad;
	float4 yQuad;

	float pointCoordX;
	float pointCoordY;
	float pointSizeInv;

	PlaneEquation z;
	PlaneEquation w;
	PlaneEquation V[MAX_INTERFACE_COMPONENTS];

	PlaneEquation clipDistance[MAX_CLIP_DISTANCES];
	PlaneEquation cullDistance[MAX_CULL_DISTANCES];

	// Masks for two-sided stencil
	int64_t clockwiseMask;
	int64_t invClockwiseMask;

	struct Span
	{
		unsigned short left;
		unsigned short right;
	};

	// The rasterizer adds a zero length span to the top and bottom of the polygon to allow
	// for 2x2 pixel processing. We need an even number of spans to keep accesses aligned.
	Span outlineUnderflow[2];
	Span outline[OUTLINE_RESOLUTION];
	Span outlineOverflow[2];
};

}  // namespace sw

#endif  // sw_Primitive_hpp
