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

#ifndef sw_SetupProcessor_hpp
#define sw_SetupProcessor_hpp

#include <Pipeline/SpirvShader.hpp>
#include "Context.hpp"
#include "Memset.hpp"
#include "RoutineCache.hpp"
#include "System/Types.hpp"

namespace sw
{
	struct Primitive;
	struct Triangle;
	struct Polygon;
	struct Vertex;
	struct DrawCall;
	struct DrawData;

	class SetupProcessor
	{
	public:
		struct States : Memset<States>
		{
			States() : Memset(this, 0) {}

			uint32_t computeHash();

			bool isDrawPoint               : 1;
			bool isDrawLine                : 1;
			bool isDrawTriangle            : 1;
			bool applySlopeDepthBias       : 1;
			bool interpolateZ              : 1;
			bool interpolateW              : 1;
			VkFrontFace frontFace          : BITS(VK_FRONT_FACE_MAX_ENUM);
			VkCullModeFlags cullMode       : BITS(VK_CULL_MODE_FLAG_BITS_MAX_ENUM);
			unsigned int multiSample       : 3;   // 1, 2 or 4
			bool rasterizerDiscard         : 1;

			SpirvShader::InterfaceComponent gradient[MAX_INTERFACE_COMPONENTS];
		};

		struct State : States
		{
			bool operator==(const State &states) const;

			uint32_t hash;
		};

		typedef bool (*RoutinePointer)(Primitive *primitive, const Triangle *triangle, const Polygon *polygon, const DrawData *draw);

		SetupProcessor();

		~SetupProcessor();

	protected:
		State update(const sw::Context* context) const;
		std::shared_ptr<Routine> routine(const State &state);

		void setRoutineCacheSize(int cacheSize);

	private:
		RoutineCache<State> *routineCache;
	};
}

#endif   // sw_SetupProcessor_hpp
