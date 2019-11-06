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

#include "Debug.hpp"

#include <stdio.h>
#include <stdarg.h>

namespace sw
{
void trace(const char *format, ...)
{
	if(false)
	{
		FILE *file = fopen("debug.txt", "a");

		if(file)
		{
			va_list vararg;
			va_start(vararg, format);
			vfprintf(file, format, vararg);
			va_end(vararg);

			fclose(file);
		}
	}
}
}
