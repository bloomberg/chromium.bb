/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Generates file nc_subregs.h
 */

#include "native_client/src/trusted/validator_x86/nacl_regsgen.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nacl_regs.h"
#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

/* Contains mapping from nacl operand kind, to the
 * the corresponding general purpose 64 bit register index
 * that it is a subpart of.
 * That is, the index into NaClRegTable64, or NACL_REGISTER_UNDEFINED
 * if not a general purpose register.
 */
static int NaClGpSubregIndex[NaClOpKindEnumSize];

/*
 * Contains the mapping from nacl operand kind, to the
 * corresponding general purpose 64 bit register index.
 * That is, the index into NaClRegTable64, or NACL_REGISTERED_UNDEFIND
 * if not a general purpose 64 bit register.
 */
static int NaClGpReg64Index[NaClOpKindEnumSize];

/*
 * Define the default buffer size to use.
 */
#define BUFFER_SIZE 256

/*
 * Given a subregister table, and a table corresponding to
 * how those subregisters layout to the corresponding registers
 * in the 64 bit table (i.e. NaClRegTable64), update the
 * regsiter index to map the corresponding operand kinds to
 * the corresponding 64 bit table entries.
 *
 * Parameters:
 *   register_index - Table from operand kind to position in 64-bit table.
 *   register_index_name - Name of register_index (for debugging messages).
 *   subreg_table - Table of subregisters to fold into register_index.
 *   subreg_table_name - Name of subreg_table (for debugging messages).
 *   to64Index - Mapping from subregister to corresponding position in
 *        NaClRegTable64.
 */
static void NaClFoldSubregisters(
    int register_index[NaClOpKindEnumSize],
    const char* register_index_name,
    const NaClOpKind subreg_table[NACL_REG_TABLE_SIZE],
    const char* subreg_tablename,
    const int to64index[NACL_REG_TABLE_SIZE]) {
  int i;
  for (i = 0; i < NACL_REG_TABLE_SIZE; ++i) {
    NaClOpKind subreg = subreg_table[i];
    if (subreg != RegUnknown) {
      int old_index = register_index[subreg];
      if (old_index == NACL_REGISTER_UNDEFINED) {
        register_index[subreg] = to64index[i];
      } else if (old_index != to64index[i]) {
        NaClLog(LOG_INFO, "%s[%d] = %s, cant replace %s[%s] = %d with %d\n",
                subreg_tablename, i, NaClOpKindName(subreg),
                register_index_name,
                NaClOpKindName(subreg),
                old_index,
                to64index[i]);
        NaClFatal("Fix before continuing\n");
      }
    }
  }
}

/*
 * Define a mapping from NaClRegTable8Rex indicies, to the corresponding
 * register index in NaClRegTable64, for which each register in
 * NaClRegTable8Rex is a subregister in NaClRegTable64, assuming the
 * REX prefix isn't defined for the instruction.
 * Note: this index will only be used if the corresponding index in
 * NaClRegTable8NoRex is not RegUnknown.
 */
static const int NaClRegTable8NoRexTo64[NACL_REG_TABLE_SIZE] = {
  0,
  1,
  2,
  3,
  0,
  1,
  2,
  3,
#if NACL_TARGET_SUBARCH == 64
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0
#endif
};

/* Define a mapping from NaClRegTable8Rex indicies, to the corresponding
 * register index in NaClRegTable64, for which each register in
 * NaClRegTable8Rex is a subregister in NaClRegTable64, assuming the
 * REX prefix is defined for the instruction.
 * Note: this index will only be used if the corresponding index in
 * NaClRegTable8Rex is not RegUnknown.
 */
static const int NaClRegTable8RexTo64[NACL_REG_TABLE_SIZE] = {
  0,
  1,
  2,
  3,
#if NACL_TARGET_SUBARCH == 64
  4,
  5,
  6,
  7,
  8,
  9,
  10,
  11,
  12,
  13,
  14,
  15
#else
  0,
  1,
  2,
  3
#endif
};

/* Define the default mapping for register tables for register indexes
 * in NaClRegTable64. This table does no reordering.
 */
static const int NaClRegTableDefaultTo64[NACL_REG_TABLE_SIZE] = {
  0,
  1,
  2,
  3,
  4,
  5,
  6,
  7,
#if NACL_TARGET_SUBARCH == 64
  8,
  9,
  10,
  11,
  12,
  13,
  14,
  15
#endif
};

/*
 * Build NaClSubreg64RegRex and NaClSubreg64RegNoRex using encoded register
 * tables.
 */
static void NaClBuildGpRegisterIndexes() {
  int i;

  /* Initialize indices to general purpose registers to defaultx. */
  for (i = 0; i < NaClOpKindEnumSize; ++i) {
    NaClGpSubregIndex[i] = NACL_REGISTER_UNDEFINED;
    NaClGpReg64Index[i] = NACL_REGISTER_UNDEFINED;
  }

  /* Fold in subregister to full register map values. */
  NaClFoldSubregisters(NaClGpSubregIndex, "NaClGpSubregIndex",
                       NaClRegTable8NoRex, "NaClRegTable8NoRex",
                       NaClRegTable8NoRexTo64);
  NaClFoldSubregisters(NaClGpSubregIndex, "NaClGpSubregIndex",
                       NaClRegTable8Rex, "NaClRexTable8Rex",
                       NaClRegTable8RexTo64);
  NaClFoldSubregisters(NaClGpSubregIndex, "NaClGpSubregIndex",
                       NaClRegTable16, "NaClRegTable16",
                       NaClRegTableDefaultTo64);
  NaClFoldSubregisters(NaClGpSubregIndex, "NaClGpSubregIndex",
                       NaClRegTable32, "NacRegTable32",
                       NaClRegTableDefaultTo64);
  NaClFoldSubregisters(NaClGpReg64Index, "NaClGpReg64Index",
                       NaClRegTable64, "NaClRegTable64",
                       NaClRegTableDefaultTo64);
}

/* Print out table entries using index values and symbolic constants. */
static char* NaClPrintIndexName(int i) {
  static char buf[BUFFER_SIZE];
  if (i == NACL_REGISTER_UNDEFINED) {
    return "NACL_REGISTER_UNDEFINED";
  } else {
    SNPRINTF(buf, BUFFER_SIZE, "%3d", i);
    return buf;
  }
}

/* Print out a operand kind lookup table. */
static void NaClPrintGpRegIndex(struct Gio* f,
                                const char* register_index_name,
                                const int register_index[NaClOpKindEnumSize]) {
  NaClOpKind i;
  gprintf(f, "static int %s[NaClOpKindEnumSize] = {\n", register_index_name);
  for (i = 0; i < NaClOpKindEnumSize; ++i) {
    gprintf(f, "  /* %20s */ %s,\n", NaClOpKindName(i),
            NaClPrintIndexName(register_index[i]));
  }
  gprintf(f, "};\n\n");
}

void NaClPrintGpRegisterIndexes(struct Gio* f) {
  NaClBuildGpRegisterIndexes();
  NaClPrintGpRegIndex(f, "NaClGpSubregIndex", NaClGpSubregIndex);
  NaClPrintGpRegIndex(f, "NaClGpReg64Index", NaClGpReg64Index);
}
