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

#include "CPUID.hpp"

#if defined(_WIN32)
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	include <windows.h>
#	include <intrin.h>
#	include <float.h>
#else
#	include <unistd.h>
#	include <sched.h>
#	include <sys/types.h>
#endif

namespace rr {

static void cpuid(int registers[4], int info)
{
#if defined(__i386__) || defined(__x86_64__)
#	if defined(_WIN32)
	__cpuid(registers, info);
#	else
	__asm volatile("cpuid"
	               : "=a"(registers[0]), "=b"(registers[1]), "=c"(registers[2]), "=d"(registers[3])
	               : "a"(info));
#	endif
#else
	registers[0] = 0;
	registers[1] = 0;
	registers[2] = 0;
	registers[3] = 0;
#endif
}

bool CPUID::supportsMMX()
{
	int registers[4];
	cpuid(registers, 1);
	return (registers[3] & 0x00800000) != 0;
}

bool CPUID::supportsCMOV()
{
	int registers[4];
	cpuid(registers, 1);
	return (registers[3] & 0x00008000) != 0;
}

bool CPUID::supportsSSE()
{
	int registers[4];
	cpuid(registers, 1);
	return (registers[3] & 0x02000000) != 0;
}

bool CPUID::supportsSSE2()
{
	int registers[4];
	cpuid(registers, 1);
	return (registers[3] & 0x04000000) != 0;
}

bool CPUID::supportsSSE3()
{
	int registers[4];
	cpuid(registers, 1);
	return (registers[2] & 0x00000001) != 0;
}

bool CPUID::supportsSSSE3()
{
	int registers[4];
	cpuid(registers, 1);
	return (registers[2] & 0x00000200) != 0;
}

bool CPUID::supportsSSE4_1()
{
	int registers[4];
	cpuid(registers, 1);
	return (registers[2] & 0x00080000) != 0;
}

}  // namespace rr
