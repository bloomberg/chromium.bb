/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Defines accessor functions for rex prefixes. */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_REX_PREFIXES_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_REX_PREFIXES_H__

/* Defines the corresponding mask for the bits of the the Rex prefix. */
#define NaClRexWMask 0x8
#define NaClRexRMask 0x4
#define NaClRexXMask 0x2
#define NaClRexBMask 0x1

/* Defines accessor functions for each of the rex bits. */
static INLINE uint8_t NaClRexW(uint8_t prefix) {
  return NaClHasBit(prefix, NaClRexWMask);
}

static INLINE uint8_t NaClRexR(uint8_t prefix) {
  return NaClHasBit(prefix, NaClRexRMask);
}

static INLINE uint8_t NaClRexX(uint8_t prefix) {
  return NaClHasBit(prefix, NaClRexXMask);
}

static INLINE uint8_t NaClRexB(uint8_t prefix) {
  return NaClHasBit(prefix, NaClRexBMask);
}

/* Defines the range of rex prefix values. */
#define NaClRexMin 0x40
#define NaClRexMax 0x4F

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_REX_PREFIXES_H__ */
