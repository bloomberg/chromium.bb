/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Tests if the ABI for passing structures by value matches.
 *
 * We have 4 modules, two compiled by one compiler and two by the other.
 * CC1: MODULE0 and MODULE3, CC2: MODULE1 and MODULE2
 * Overall structure is this:
 * main():
 *   test_type()
 *   vv--call/check--vv
 *   mod0_type()  --call/check-->      mod1_type()
 *                                     vv--call/check--vv
 *   mod3_type()  <--call/check--      mod2_type()
 *
 * So, CC1 passes stuff to CC1 -> CC2 -> CC2 -> CC1
 *
 * In addition, each callee memsets the given struct, and upon return,
 * the caller checks that the original struct was untouched to ensure that the
 * by value passing is done via a copy.
 *
 * To see the pre-processor-generated source for MODULE${X}
 *
 * gcc file.c -E -o - -DMODULE${X} | indent | less
 */

#include "native_client/tests/callingconv_case_by_case/useful_structs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Trick inter-procedural constant propagation by adding some branching.
 * This shouldn't happen with LLVM since we have split every function into
 * separate native .o files, but... just in case the build changes.
 * Also tag each function as noinline... just in case.
 */
#if defined(MODULE0)
int should_be_true;
#else
/* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
extern int should_be_true;
#endif

/**********************************************************************/

/* We test the pnaclcall attribute several ways.
 * (1) Commandline flag (or nothing at all if there is 100% compatibility).
 * (2) Declaring the callee as having the "pnaclcall" attribute.
 * (3) Not declaring the callee to have the attribute, but by making a
 * call through a temporary function pointer that has the attribute.
 *
 * Module 2 is the only one that calls from nacl-gcc to pnacl, so
 * this is the only one that varies this way.
 *
 * Module 0 to Module 1 is where we transition from pnacl to nacl-gcc.
 * For this, we either use the commandline flag (1), or we use the
 * declaration (2), since pnacl itself cannot turn on/off the pnaclcall
 * convention.
 */
#if defined(TEST_ATTRIBUTE_VIA_DECL)

#define MOD3_DECL(TYPE)                                                 \
  /* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */                               \
  extern __attribute__((noinline, pnaclcall)) void mod3_##TYPE(TYPE z)
#define MOD2_TO_3_CALL(TYPE, arg)               \
  mod3_##TYPE(arg)
#define MOD1_DECL(TYPE)                                         \
  __attribute__((noinline, pnaclcall)) void mod1_##TYPE(TYPE z)

#elif defined(TEST_ATTRIBUTE_VIA_FP)

#define MOD3_DECL(TYPE)                                     \
  /* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */                   \
  extern __attribute__((noinline)) void mod3_##TYPE(TYPE z)
#define MOD2_TO_3_CALL(TYPE, arg)                                   \
  do {                                                              \
    void (__attribute__((pnaclcall)) *temp_fp)(TYPE z) =            \
        (void (__attribute__((pnaclcall)) *)(TYPE)) &mod3_##TYPE;   \
    (*temp_fp)(arg);                                                \
  } while (0)
#define MOD1_DECL(TYPE)                                         \
  __attribute__((noinline, pnaclcall)) void mod1_##TYPE(TYPE z)

#else /* Testing with just the commandline flag / or nothing at all. */

#define MOD3_DECL(TYPE)                                     \
  /* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */                   \
  extern __attribute__((noinline)) void mod3_##TYPE(TYPE z)
#define MOD2_TO_3_CALL(TYPE, arg)               \
  mod3_##TYPE(arg)
#define MOD1_DECL(TYPE)                                 \
  __attribute__((noinline)) void mod1_##TYPE(TYPE z)

#endif  /* TEST_ATTRIBUTE_VIA_... */

/**********************************************************************/
/* The actual test code structure (which is divided into 4 modules). */

#define GENERATE_FOR_MODULE3(TYPE)                                  \
  __attribute__((noinline)) void mod3_##TYPE(TYPE z);               \
  void mod3_##TYPE(TYPE z) {                                        \
    CHECK_##TYPE(z);                                                \
    if (should_be_true) {                                           \
      printf("Made it to mod3_" #TYPE "\n");                        \
      memset((void*)&z, 0, sizeof z);                               \
    } else {                                                        \
      printf("should_be_true is not true for mod3_" #TYPE "\n");    \
      memset((void*)&z, 1, sizeof z);                               \
    }                                                               \
  }

#define GENERATE_FOR_MODULE2(TYPE)                                  \
  MOD3_DECL(TYPE);                                                  \
  __attribute__((noinline)) void mod2_##TYPE(TYPE z);               \
  void mod2_##TYPE(TYPE z) {                                        \
    CHECK_##TYPE(z);                                                \
    if (should_be_true) {                                           \
      MOD2_TO_3_CALL(TYPE, z);                                      \
      CHECK_##TYPE(z);                                              \
      printf("Made it to mod2_" #TYPE "\n");                        \
      memset((void*)&z, 0, sizeof z);                               \
    } else {                                                        \
      memset((void*)&z, 0, sizeof z);                               \
      printf("should_be_true is not true for mod2_" #TYPE "\n");    \
      mod3_##TYPE(z);                                               \
    }                                                               \
  }

#define GENERATE_FOR_MODULE1(TYPE)                                  \
  /* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */                           \
  extern __attribute__((noinline)) void mod2_##TYPE(TYPE z);        \
  MOD1_DECL(TYPE);                                                  \
  void mod1_##TYPE(TYPE z) {                                        \
    CHECK_##TYPE(z);                                                \
    if (should_be_true) {                                           \
      mod2_##TYPE(z);                                               \
      CHECK_##TYPE(z);                                              \
      printf("Made it to mod1_" #TYPE "\n");                        \
      memset((void*)&z, 0, sizeof z);                               \
    } else {                                                        \
      memset((void*)&z, 0, sizeof z);                               \
      printf("should_be_true is not true for mod1_" #TYPE "\n");    \
      mod2_##TYPE(z);                                               \
    }                                                               \
  }

#define GENERATE_FOR_MODULE0(TYPE)                                  \
  /* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */                           \
  extern __attribute__((noinline)) void mod1_##TYPE(TYPE z) ;       \
  void __attribute__((noinline)) mod0_##TYPE(TYPE z) ;              \
  void mod0_##TYPE(TYPE z) {                                        \
    CHECK_##TYPE(z);                                                \
    if (should_be_true) {                                           \
      mod1_##TYPE(z);                                               \
      CHECK_##TYPE(z);                                              \
      printf("Made it to mod0_" #TYPE "\n");                        \
      memset((void*)&z, 0, sizeof z);                               \
    } else {                                                        \
      memset((void*)&z, 0, sizeof z);                               \
      printf("should_be_true is not true for mod0_" #TYPE "\n");    \
      mod1_##TYPE(z);                                               \
    }                                                               \
  }                                                                 \
  void  __attribute__((noinline)) test_##TYPE(void);                \
  void test_##TYPE(void) {                                          \
    TYPE z = k##TYPE;                                               \
    CHECK_##TYPE(z);                                                \
    if (should_be_true) {                                           \
      mod0_##TYPE(z);                                               \
      CHECK_##TYPE(z);                                              \
      printf("Made it through test_" #TYPE "\n");                   \
    } else {                                                        \
      memset((void*)&z, 0, sizeof z);                               \
      printf("should_be_true is not true for test_" #TYPE "\n");    \
      mod0_##TYPE(z);                                               \
    }                                                               \
  }


#undef DO_FOR_TYPE
#if defined(MODULE0)
#define DO_FOR_TYPE GENERATE_FOR_MODULE0
#elif defined(MODULE1)
#define DO_FOR_TYPE GENERATE_FOR_MODULE1
#elif defined(MODULE2)
#define DO_FOR_TYPE GENERATE_FOR_MODULE2
#elif defined(MODULE3)
#define DO_FOR_TYPE GENERATE_FOR_MODULE3
#else
#error "Must define MODULE0. or MODULE1, 2 or 3 in preprocessor!"
#endif  /* defined(MODULE0) */

#include "native_client/tests/callingconv_case_by_case/for_each_type.h"
#undef DO_FOR_TYPE

/* Place Main in Module 0 */
#if defined(MODULE0)
int main(int argc, char* argv[]) {
  should_be_true = 0;

  /* This should always be true when running this test. */
  if (argc != 55) {
    should_be_true = 1;
  }

  /* Set NOT_DECLARING_DEFINING to tell for_each_type.h that this is not
   * for declarations and definition specific stuff like extern "C".
   */
#define NOT_DECLARING_DEFINING 1
#define CALL_TEST(TYPE)                         \
  test_##TYPE();

#define DO_FOR_TYPE CALL_TEST
#include "native_client/tests/callingconv_case_by_case/for_each_type.h"
#undef DO_FOR_TYPE
#undef NOT_DECLARING_DEFINING

  return 0;
}
#endif /* defined(MODULE0) */
