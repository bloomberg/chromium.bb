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

#ifndef sw_RoutineCache_hpp
#define sw_RoutineCache_hpp

#include "LRUCache.hpp"

#include "Reactor/Reactor.hpp"

namespace sw
{
	template<class State>
	class RoutineCache : public LRUCache<State, Routine>
	{
	public:
		RoutineCache(int n, const char *precache = 0);
		~RoutineCache();

	private:
		const char *precache;
		#if defined(_WIN32)
		HMODULE precacheDLL;
		#endif
	};

	template<class State>
	RoutineCache<State>::RoutineCache(int n, const char *precache) : LRUCache<State, Routine>(n), precache(precache)
	{
	}

	template<class State>
	RoutineCache<State>::~RoutineCache()
	{
	}
}

#endif   // sw_RoutineCache_hpp
