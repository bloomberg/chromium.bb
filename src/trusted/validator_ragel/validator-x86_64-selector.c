/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file exist for one purpose one: to pick fast, goto driven FSM in
 * validator-x86_64.c or slower table driven FSM in validator-x86_64_table.c.
 * They are functionally identical and produced from the same ragel source
 * with different generation options. Ideally we'd like to always use goto
 * driven FSM, but it includes tens of thousands lines of code which breaks
 * some old/exotic compilers.  Details are below.
 */

/*
 * XCode 4.2 takes too long to compile with -Os.
 * TODO(khim): Remove this when LLVM is upgraded in XCode (clang 3.1 works fine,
 * for example - and it's LLVM-based, too).
 */
#if defined(__APPLE__) && defined(__llvm__)
# define USE_TABLE_BASED_VALIDATOR
#endif

/*
 * MSVC 2010 works fine, MSVC 2008 is too slow with /O1.
 * TODO(khim): Remove this when MSVC 2008 is deprecated.
 */
#if defined(_MSC_VER)
# if _MSC_VER < 1600
#  define USE_TABLE_BASED_VALIDATOR
# endif
#endif

/*
 * ASAN takes too long with optimizations enabled and is killed on buildbot.
 * We don't really need to build with optimizations under ASAN (although it's
 * nice thing to do).
 * TODO(khim): Remove this when/if the following bug is fixed.
 *   http://code.google.com/p/address-sanitizer/issues/detail?id=61
 */
#if !defined(__has_feature)
# define __has_feature(x) 0
#endif
#if __has_feature(address_sanitizer)
# define USE_TABLE_BASED_VALIDATOR
#endif

#ifdef USE_TABLE_BASED_VALIDATOR
# include "native_client/src/trusted/validator_ragel/gen/validator-x86_64_table.c"
#else
# include "native_client/src/trusted/validator_ragel/gen/validator-x86_64.c"
#endif
