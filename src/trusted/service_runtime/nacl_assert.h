/*
 * Copyright 2008, Google Inc.
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

/*
 * NaCl service runtime, assertion macros.
 *
 * THIS FILE SHOULD BE USED ONLY IN TEST CODE.
 */
#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_ASSERT_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_ASSERT_H_

#include "native_client/src/include/portability.h"
/* get PRIxPTR */

/*
 * Instead of using <assert.h>, we define a version that works with
 * our testing framework, printing out FAIL / SUCCESS to standard
 * output and then exiting with a non-zero exit status.
 */

#define ASSERT(bool_expr) do {                                          \
    if (!(bool_expr)) {                                                 \
      fprintf(stderr,                                                   \
              "Error at line %d, %s: " #bool_expr  " is FALSE\n",       \
              __LINE__, __FILE__);                                      \
      printf("FAIL\n");                                                 \
      exit(1);                                                          \
    }                                                                   \
  } while (0)

#define ASSERT_MSG(bool_expr, msg) do {                                 \
    if (!(bool_expr)) {                                                 \
      fprintf(stderr,                                                   \
              "Error at line %d, %s: " #bool_expr  " is FALSE\n",       \
              __LINE__, __FILE__);                                      \
      fprintf(stderr, "%s\n", msg);                                     \
      printf("FAIL\n");                                                 \
      exit(1);                                                          \
    }                                                                   \
  } while (0)

#define ASSERT_OP_THUNK(lhs, op, rhs, slhs, srhs, thunk) do {   \
    if (!(lhs op rhs)) {                                        \
      fprintf(stderr,                                           \
              "Error at line %d, %s:\n"                         \
              "Error: %s "#op" %s"                              \
              " is FALSE\n",                                    \
              __LINE__, __FILE__, slhs, srhs);                  \
      fprintf(stderr,                                           \
              "got 0x%08"PRIxPTR" (%"PRIdPTR"); "               \
              "comparison value 0x%08"PRIxPTR" (%"PRIdPTR")\n", \
              (uintptr_t) lhs, (uintptr_t) lhs,                 \
              (uintptr_t) rhs, (uintptr_t) rhs);                \
      thunk;                                                    \
      printf("FAIL\n");                                         \
      exit(1);                                                  \
    }                                                           \
  } while (0)

#define ASSERT_OP_MSG(lhs, op, rhs, slhs, srhs, msg)                    \
  ASSERT_OP_THUNK(lhs, op, rhs,                                         \
                  slhs, srhs, do { fprintf(stderr, "%s\n", msg); } while (0))

#define ASSERT_EQ_MSG(lhs, rhs, msg)            \
  ASSERT_OP_MSG(lhs, ==, rhs, #lhs, #rhs, msg)
#define ASSERT_NE_MSG(lhs, rhs, msg)            \
  ASSERT_OP_MSG(lhs, !=, rhs, #lhs, #rhs, msg)
#define ASSERT_LE_MSG(lhs, rhs, msg)            \
  ASSERT_OP_MSG(lhs, <=, rhs, #lhs, #rhs, msg)

#define ASSERT_OP(lhs, op, rhs, slhs, srhs)                     \
  ASSERT_OP_THUNK(lhs, op, rhs, slhs, srhs, do { ; } while (0))

#define ASSERT_EQ(lhs, rhs) ASSERT_OP(lhs, ==, rhs, #lhs, #rhs)
#define ASSERT_NE(lhs, rhs) ASSERT_OP(lhs, !=, rhs, #lhs, #rhs)
#define ASSERT_LE(lhs, rhs) ASSERT_OP(lhs, <=, rhs, #lhs, #rhs)

#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_NACL_ASSERT_H_ */
