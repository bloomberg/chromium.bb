/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _NATIVE_CLIENT_SRC_UNTRUSTED_NACL_PNACLINTRIN_H_
#define _NATIVE_CLIENT_SRC_UNTRUSTED_NACL_PNACLINTRIN_H_ 1

#if !defined(__native_client__) || !defined(__pnacl__)
#error "Not currently compiling using PNaCl"
#else

/* Enumeration values returned by __builtin_nacl_target_arch. */
enum PnaclTargetArchitecture {
  PnaclTargetArchitectureInvalid = 0,
  PnaclTargetArchitectureX86_32,
  PnaclTargetArchitectureX86_64,
  PnaclTargetArchitectureARM_32,
  PnaclTargetArchitectureARM_32_Thumb,
  PnaclTargetArchitectureMips_32
};

int __builtin_nacl_target_arch(void) asm("llvm.nacl.target.arch");

#endif /* !defined(__native_client__) || !defined(__pnacl__) */

#endif /* _NATIVE_CLIENT_SRC_UNTRUSTED_NACL_PNACLINTRIN_H_ */
