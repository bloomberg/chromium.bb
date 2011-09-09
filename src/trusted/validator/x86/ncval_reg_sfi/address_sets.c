/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.

 */

/*
 * address_sets.c - Implements a bit set of addresses that is used by branch
 * validation to check if branches are safe.
 */

#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/address_sets.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter_internal.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Model the set of possible 3-bit tails of possible PcAddresses. */
static const uint8_t nacl_pc_address_masks[8] = {
  0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

/* Model the offset created by removing the bottom three bits of a PcAddress. */
typedef NaClPcAddress NaClPcOffset;

/* Convert an address into the corresponding offset in an address table.
 * That is, strip off the last three bits, since these remaining bits
 * will be encoded using the union of address masks in the address table.
 */
static INLINE NaClPcOffset NaClPcAddressToOffset(NaClPcAddress address) {
  return address >> 3;
}

/* Convert the 3 lower bits of an address into the corresponding address mask
 * to use.
 */
static INLINE uint8_t NaClPcAddressToMask(NaClPcAddress address) {
  return nacl_pc_address_masks[(int) (address & (NaClPcAddress)0x7)];
}

/* Returns true if the given address is within the code segment. Generates
 * error messages if it isn't.
 */
static Bool NaClCheckAddressRange(NaClPcAddress address,
                                  NaClValidatorState* state) {
  if (address < state->vbase) {
    NaClValidatorPcAddressMessage(LOG_ERROR, state, address,
                                  "Jump to address before code block.\n");
    return FALSE;
  }
  if (address >= state->vlimit) {
    NaClValidatorPcAddressMessage(LOG_ERROR, state, address,
                                  "Jump to address beyond code block limit.\n");
    return FALSE;
  }
  return TRUE;
}

uint8_t NaClAddressSetContains(NaClAddressSet set,
                               NaClPcAddress address,
                               NaClValidatorState* state) {
  if (NaClCheckAddressRange(address, state)) {
    NaClPcAddress offset = address - state->vbase;
    return set[NaClPcAddressToOffset(offset)] & NaClPcAddressToMask(offset);
  } else {
    return FALSE;
  }
}

void NaClAddressSetAdd(NaClAddressSet set, NaClPcAddress address,
                       NaClValidatorState* state) {
  if (NaClCheckAddressRange(address, state)) {
    NaClPcAddress offset = address - state->vbase;
    DEBUG(NaClLog(LOG_INFO,
                  "Address set add: %"NACL_PRIxNaClPcAddress"\n", address));
    set[NaClPcAddressToOffset(offset)] |= NaClPcAddressToMask(offset);
  }
}

size_t NaClAddressSetArraySize(NaClMemorySize size) {
  /* Be sure to add an element for partial overlaps. */
  /* TODO(karl) The cast to size_t for the number of elements may
   * cause loss of data. We need to fix this. This is a security
   * issue when doing cross-platform (32-64 bit) generation.
   */
  return (size_t) NaClPcAddressToOffset(size) + 1;
}

NaClAddressSet NaClAddressSetCreate(NaClMemorySize size) {
  return (NaClAddressSet) calloc(NaClAddressSetArraySize(size),
                                 sizeof(uint8_t));
}

void NaClAddressSetDestroy(NaClAddressSet set) {
  free(set);
}
