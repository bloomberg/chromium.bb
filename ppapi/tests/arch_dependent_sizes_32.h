/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file has compile assertions for the sizes of types that are dependent
 * on the architecture for which they are compiled (i.e., 32-bit vs 64-bit).
 */

#ifndef PPAPI_TESTS_ARCH_DEPENDENT_SIZES_32_H_
#define PPAPI_TESTS_ARCH_DEPENDENT_SIZES_32_H_

#include "ppapi/tests/test_struct_sizes.c"

PP_COMPILE_ASSERT_SIZE_IN_BYTES(GLintptr, 4);
PP_COMPILE_ASSERT_SIZE_IN_BYTES(GLsizeiptr, 4);
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_CompletionCallback_Func, 4);
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_URLLoaderTrusted_StatusCallback, 4);
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_CompletionCallback, 8);
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_FileChooserOptions_Dev, 8);
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_Picture_Dev, 24);
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_VideoBitstreamBuffer_Dev, 12);
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PPB_VideoDecoder_Dev, 36);
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PPP_VideoDecoder_Dev, 20);

#endif  /* PPAPI_TESTS_ARCH_DEPENDENT_SIZES_32_H_ */
