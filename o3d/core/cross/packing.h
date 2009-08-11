/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file defines Packing macros for the packing of structures.

#ifndef O3D_UTILS_CROSS_PACKING_H_
#define O3D_UTILS_CROSS_PACKING_H_

#if defined(COMPILER_GCC)

// NOTE: I tried building the string as in
//   #define O3D_SET_STRUCTURE_PACKING(p) _Pragma("pack(" #p ")")
//   but gcc doesn't like that. :-(
#define O3D_SET_STRUCTURE_PACKING_1 _Pragma("pack(1)")
#define O3D_SET_STRUCTURE_PACKING_2 _Pragma("pack(2)")
#define O3D_SET_STRUCTURE_PACKING_4 _Pragma("pack(4)")
#define O3D_PUSH_STRUCTURE_PACKING_1  _Pragma("pack(push, 1)")
#define O3D_PUSH_STRUCTURE_PACKING_2  _Pragma("pack(push, 2)")
#define O3D_PUSH_STRUCTURE_PACKING_4  _Pragma("pack(push, 4)")
#define O3D_POP_STRUCTURE_PACKING _Pragma("pack(pop)")

#elif defined(COMPILER_MSVC)

#define O3D_SET_STRUCTURE_PACKING_1 __pragma(pack(1))
#define O3D_SET_STRUCTURE_PACKING_2 __pragma(pack(2))
#define O3D_SET_STRUCTURE_PACKING_4 __pragma(pack(4))
#define O3D_PUSH_STRUCTURE_PACKING_1  __pragma(pack(push, 1))
#define O3D_PUSH_STRUCTURE_PACKING_2  __pragma(pack(push, 2))
#define O3D_PUSH_STRUCTURE_PACKING_4  __pragma(pack(push, 4))
#define O3D_POP_STRUCTURE_PACKING __pragma(pack(pop))

#else
#error Need Packing Macros
#endif

#endif  // O3D_UTILS_CROSS_PACKING_H_

