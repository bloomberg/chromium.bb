/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Generates file nc_subregs.h
 *
 * TODO(karl) Fix naming conventions for array names so that they
 * are consistent.
 */

#include "native_client/src/trusted/validator/x86/decoder/generator/nacl_regsgen.h"

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/x86/nacl_regs.h"
#include "native_client/src/trusted/validator/x86/nacl_regs32.h"
#include "native_client/src/trusted/validator/x86/nacl_regs64.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"

/* Contains mapping from nacl operand kind, to the
 * the corresponding general purpose 64 bit register index
 * that it is a subpart of.
 * That is, the index into NaClRegTable64, or NACL_REGISTER_UNDEFINED
 * if not a general purpose register.
 */
static int nacl_gp_subreg_index[NaClOpKindEnumSize];

/*
 * Contains the mapping from nacl operand kind, to the
 * corresponding general purpose 64 bit register index.
 * That is, the index into NaClRegTable64, or NACL_REGISTERED_UNDEFIND
 * if not a general purpose 64 bit register.
 */
static int nacl_gp_reg_64_index[NaClOpKindEnumSize];

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
    const NaClOpKind* subreg_table,
    const char* subreg_tablename,
    const int* to64index,
    const int subreg_table_size,
    const int register_undefined) {
  int i;
  for (i = 0; i < subreg_table_size; ++i) {
    NaClOpKind subreg = subreg_table[i];
    if (subreg != RegUnknown) {
      int old_index = register_index[subreg];
      if (old_index == register_undefined) {
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
 * Define a mapping from NaClRegTable8Rex_32 indices, to the corresponding
 * register index in NaClRegTable64_32, for which each register in
 * NaClRegTable8Rex_32 is a subregister in NaClRegTable64_32, assuming the
 * REX prefix isn't defined for the instruction.
 * Note: this index will only be used if the corresponding index in
 * NaClRegTable8NoRex_32 is not RegUnknown.
 */
static const int NaClRegTable8NoRexTo64_32[NACL_REG_TABLE_SIZE_32] = {
  0,
  1,
  2,
  3,
  0,
  1,
  2,
  3,
};

/*
 * Define a mapping from NaClRegTable8Rex_64 indices, to the corresponding
 * register index in NaClRegTable64_64, for which each register in
 * NaClRegTable8Rex_64 is a subregister in NaClRegTable64_64, assuming the
 * REX prefix isn't defined for the instruction.
 * Note: this index will only be used if the corresponding index in
 * NaClRegTable8NoRex_64 is not RegUnknown.
 */
static const int NaClRegTable8NoRexTo64_64[NACL_REG_TABLE_SIZE_64] = {
  0,
  1,
  2,
  3,
  0,
  1,
  2,
  3,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0
};

/* Define a mapping from NaClRegTable8Rex_32 indices, to the corresponding
 * register index in NaClRegTable64_32, for which each register in
 * NaClRegTable8Rex_32 is a subregister in NaClRegTable64_32, assuming the
 * REX prefix is defined for the instruction.
 * Note: this index will only be used if the corresponding index in
 * NaClRegTable8Rex_32 is not RegUnknown.
 */
static const int NaClRegTable8RexTo64_32[NACL_REG_TABLE_SIZE_32] = {
  0,
  1,
  2,
  3,
  0,
  1,
  2,
  3
};

/* Define a mapping from NaClRegTable8Rex_64 indices, to the corresponding
 * register index in NaClRegTable64_64, for which each register in
 * NaClRegTable8Rex is a subregister in NaClRegTable64_64, assuming the
 * REX prefix is defined for the instruction.
 * Note: this index will only be used if the corresponding index in
 * NaClRegTable8Rex_64 is not RegUnknown.
 */
static const int NaClRegTable8RexTo64_64[NACL_REG_TABLE_SIZE_64] = {
  0,
  1,
  2,
  3,
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
};

/* Define the default mapping for register tables for register indexes
 * in NaClRegTable64_32. This table does no reordering.
 */
static const int NaClRegTableDefaultTo64_32[NACL_REG_TABLE_SIZE_32] = {
  0,
  1,
  2,
  3,
  4,
  5,
  6,
  7,
};

/* Define the default mapping for register tables for register indexes
 * in NaClRegTable64_64. This table does no reordering.
 */
static const int NaClRegTableDefaultTo64_64[NACL_REG_TABLE_SIZE_64] = {
  0,
  1,
  2,
  3,
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
};

/*
 * Build NaClSubreg64RegRex and NaClSubreg64RegNoRex using encoded register
 * tables.
 */
static void NaClBuildGpRegisterIndexes(
    const NaClOpKind* reg_table_8_no_rex,
    const int* reg_table_8_no_rex_to_64,
    const NaClOpKind* reg_table_8_rex,
    const int* reg_table_8_rex_to_64,
    const NaClOpKind* reg_table_16,
    const NaClOpKind* reg_table_32,
    const NaClOpKind* reg_table_64,
    const int* reg_table_default_to_64,
    int subreg_table_size,
    int register_undefined) {
  int i;

  /* Initialize indices to general purpose registers to defaultx. */
  for (i = 0; i < NaClOpKindEnumSize; ++i) {
    nacl_gp_subreg_index[i] = register_undefined;
    nacl_gp_reg_64_index[i] = register_undefined;
  }

  /* Fold in subregister to full register map values. */
  NaClFoldSubregisters(nacl_gp_subreg_index, "nacl_gp_subreg_index",
                       reg_table_8_no_rex, "reg_table_8_no_rex",
                       reg_table_8_no_rex_to_64,
                       subreg_table_size,
                       register_undefined);
  NaClFoldSubregisters(nacl_gp_subreg_index, "nacl_gp_subreg_index",
                       reg_table_8_rex, "rex_table_8_rex",
                       reg_table_8_rex_to_64,
                       subreg_table_size,
                       register_undefined);
  NaClFoldSubregisters(nacl_gp_subreg_index, "nacl_gp_subreg_index",
                       reg_table_16, "reg_table_16",
                       reg_table_default_to_64,
                       subreg_table_size,
                       register_undefined);
  NaClFoldSubregisters(nacl_gp_subreg_index, "nacl_gp_subreg_index",
                       reg_table_32, "reg_table_32",
                       reg_table_default_to_64,
                       subreg_table_size,
                       register_undefined);
  NaClFoldSubregisters(nacl_gp_reg_64_index, "nacl_gp_reg_64_index",
                       reg_table_64, "reg_table_64",
                       reg_table_default_to_64,
                       subreg_table_size,
                       register_undefined);
}

/* Print out table entries using index values and symbolic constants. */
static char* NaClPrintIndexName(int i, int register_undefined) {
  static char buf[BUFFER_SIZE];
  if (i == register_undefined) {
    return "NACL_REGISTER_UNDEFINED";
  } else {
    SNPRINTF(buf, BUFFER_SIZE, "%3d", i);
    return buf;
  }
}

/* Print out a operand kind lookup table. */
static void NaClPrintGpRegIndex(struct Gio* f,
                                const char* register_index_name,
                                const int register_index[NaClOpKindEnumSize],
                                int register_undefined) {
  NaClOpKind i;
  gprintf(f, "static int %s[NaClOpKindEnumSize] = {\n", register_index_name);
  for (i = 0; i < NaClOpKindEnumSize; ++i) {
    gprintf(f, "  /* %20s */ %s,\n", NaClOpKindName(i),
            NaClPrintIndexName(register_index[i], register_undefined));
  }
  gprintf(f, "};\n\n");
}

void NaClPrintGpRegisterIndexes_32(struct Gio* f) {
  NaClBuildGpRegisterIndexes(NaClRegTable8NoRex_32,
                             NaClRegTable8NoRexTo64_32,
                             NaClRegTable8Rex_32,
                             NaClRegTable8RexTo64_32,
                             NaClRegTable16_32,
                             NaClRegTable32_32,
                             NaClRegTable64_32,
                             NaClRegTableDefaultTo64_32,
                             NACL_REG_TABLE_SIZE_32,
                             NACL_REGISTER_UNDEFINED_32);
  NaClPrintGpRegIndex(f, "NaClGpSubregIndex", nacl_gp_subreg_index,
                      NACL_REGISTER_UNDEFINED_32);
  NaClPrintGpRegIndex(f, "NaClGpReg64Index", nacl_gp_reg_64_index,
                      NACL_REGISTER_UNDEFINED_32);
}

void NaClPrintGpRegisterIndexes_64(struct Gio* f) {
  NaClBuildGpRegisterIndexes(NaClRegTable8NoRex_64,
                             NaClRegTable8NoRexTo64_64,
                             NaClRegTable8Rex_64,
                             NaClRegTable8RexTo64_64,
                             NaClRegTable16_64,
                             NaClRegTable32_64,
                             NaClRegTable64_64,
                             NaClRegTableDefaultTo64_64,
                             NACL_REG_TABLE_SIZE_64,
                             NACL_REGISTER_UNDEFINED_64);
  NaClPrintGpRegIndex(f, "NaClGpSubregIndex", nacl_gp_subreg_index,
                      NACL_REGISTER_UNDEFINED_64);
  NaClPrintGpRegIndex(f, "NaClGpReg64Index", nacl_gp_reg_64_index,
                      NACL_REGISTER_UNDEFINED_64);
}
