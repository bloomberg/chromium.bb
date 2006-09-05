/* Copyright (C) 2006 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

/* airbag_types.h: Precise-width types
 *
 * (This is C99 source, please don't corrupt it with C++.)
 *
 * This file ensures that types u_intN_t are defined for N = 8, 16, 32, and
 * 64.  Types of precise widths are crucial to the task of writing data
 * structures on one platform and reading them on another.
 *
 * Author: Mark Mentovai */

#ifndef GOOGLE_AIRBAG_TYPES_H__
#define GOOGLE_AIRBAG_TYPES_H__


#ifndef _WIN32

#include <sys/types.h>

#else /* !_WIN32 */

#include <WTypes.h>

typedef unsigned __int8  u_int8_t;
typedef unsigned __int16 u_int16_t;
typedef unsigned __int32 u_int32_t;
typedef unsigned __int64 u_int64_t;

#endif /* !_WIN32 */

typedef u_int64_t airbag_time_t;

#endif /* GOOGLE_AIRBAG_TYPES_H__ */
