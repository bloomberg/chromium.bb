/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#include "native_client/src/trusted/validator_x86/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/utils/types.h"
#include "gen/native_client/src/trusted/validator_x86/ncop_expr_node_flag_impl.h"
#include "gen/native_client/src/trusted/validator_x86/ncop_expr_node_kind_impl.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

void NaClPrintExpFlags(FILE* file, NaClExpFlags flags) {
  if (flags == 0) {
    fprintf(file, "0");
  } else {
    NaClExpFlag f;
    Bool is_first = TRUE;
    for (f = 0; f < NaClExpFlagEnumSize; f++) {
      if (flags & NACL_EFLAG(f)) {
        if (is_first) {
          is_first = FALSE;
        } else {
          fprintf(file, " | ");
        }
        fprintf(file, "%s", NaClExpFlagName(f));
      }
    }
  }
}

typedef struct NaClExpKindDescriptor {
  /* The name of the expression operator. */
  NaClExpKind name;
  /* The rank (i.e. number of children) the expression operator has. */
  const int rank;
} NaClExpKindDescriptor;

/* The print names of valid NaClExpKind values. */
static const NaClExpKindDescriptor
g_NaClExpKindDesc[NaClExpKindEnumSize + 1]= {
  {UndefinedExp, 0},
  {ExprRegister, 0},
  {OperandReference, 1},
  {ExprConstant, 0},
  {ExprConstant64, 2},
  {ExprSegmentAddress, 2},
  {ExprMemOffset, 4},
};

int NaClExpKindRank(NaClExpKind kind) {
  assert(kind == g_NaClExpKindDesc[kind].name);
  return g_NaClExpKindDesc[kind].rank;
}

/* Returns the register defined by the given node. */
NaClOpKind NaClGetExpRegister(NaClExp* node) {
  assert(node->kind == ExprRegister);
  return (NaClOpKind) node->value;
}

/* Returns the name of the register defined by the indexed node in the
 * vector of nodes.
 */
NaClOpKind NaClGetExpVectorRegister(NaClExpVector* vector,
                                    int node) {
  return NaClGetExpRegister(&vector->node[node]);
}

static int NaClPrintDisassembledExp(FILE* file,
                                    NaClExpVector* vector,
                                    uint32_t index);

/* Print the characters in the given string using lower case. */
static void NaClPrintLower(FILE* file, char* str) {
  while (*str) {
    putc(tolower(*str), file);
    ++str;
  }
}

/* Return the sign (extended) integer in the given expr node. */
static int32_t NaClGetSignExtendedValue(NaClExp* node) {
  if (node->flags & NACL_EFLAG(ExprSize8)) {
    return (int8_t) node->value;
  } else if (node->flags & NACL_EFLAG(ExprSize16)) {
    return (int16_t) node->value;
  } else {
    return (int32_t) node->value;
  }
}

/* Print out the given (constant) expression node to the given file. */
static void NaClPrintDisassembledConst(FILE* file, NaClExp* node) {
  assert(node->kind == ExprConstant);
  if (node->flags & NACL_EFLAG(ExprUnsignedHex)) {
    fprintf(file, "0x%"NACL_PRIx32, node->value);
  } else if (node->flags & NACL_EFLAG(ExprSignedHex)) {
    int32_t value = NaClGetSignExtendedValue(node);
    if (value < 0) {
      value = -value;
      fprintf(file, "-0x%"NACL_PRIx32, value);
    } else {
      fprintf(file, "0x%"NACL_PRIx32, value);
    }
  } else if (node->flags & NACL_EFLAG(ExprUnsignedInt)) {
    fprintf(file, "%"NACL_PRIu32, node->value);
  } else {
    /* Assume ExprSignedInt. */
    fprintf(file, "%"NACL_PRId32, (int32_t) NaClGetSignExtendedValue(node));
  }
}

/* Print out the given (64-bit constant) expression node to the given file. */
static void NaClPrintDisassembledConst64(
    FILE* file, NaClExpVector* vector, int index) {
  NaClExp* node;
  uint64_t value;
  node = &vector->node[index];
  assert(node->kind == ExprConstant64);
  value = NaClGetExpConstant(vector, index);
  if (node->flags & NACL_EFLAG(ExprUnsignedHex)) {
    fprintf(file, "0x%"NACL_PRIx64, value);
  } else if (node->flags & NACL_EFLAG(ExprSignedHex)) {
    int64_t val = (int64_t) value;
    if (val < 0) {
      val = -val;
      fprintf(file, "-0x%"NACL_PRIx64, val);
    } else {
      fprintf(file, "0x%"NACL_PRIx64, val);
    }
  } else if (node->flags & NACL_EFLAG(ExprUnsignedInt)) {
    fprintf(file, "%"NACL_PRIu64, value);
  } else {
    fprintf(file, "%"NACL_PRId64, (int64_t) value);
  }
}

/* Print out the disassembled representation of the given register
 * to the given file.
 */
static void NaClPrintDisassembledRegKind(FILE* file, NaClOpKind reg) {
  const char* name = NaClOpKindName(reg);
  char* str = strstr(name, "Reg");
  putc('%', file);
  NaClPrintLower(file, str == NULL ? (char*) name : str + strlen("Reg"));
}

static INLINE void NaClPrintDisassembledReg(FILE* file, NaClExp* node) {
  NaClPrintDisassembledRegKind(file, NaClGetExpRegister(node));
}

void NaClExpVectorPrint(FILE* file, NaClExpVector* vector) {
  uint32_t i;
  fprintf(file, "NaClExpVector[%d] = {\n", vector->number_expr_nodes);
  for (i = 0; i < vector->number_expr_nodes; i++) {
    NaClExp* node = &vector->node[i];
    fprintf(file, "  { %s[%d] , ",
            NaClExpKindName(node->kind),
            NaClExpKindRank(node->kind));
    switch (node->kind) {
      case ExprRegister:
        NaClPrintDisassembledReg(file, node);
        break;
      case ExprConstant:
        NaClPrintDisassembledConst(file, node);
        break;
      case ExprConstant64:
        NaClPrintDisassembledConst64(file, vector, i);
        break;
      default:
        fprintf(file, "%"NACL_PRIu32, node->value);
        break;
    }
    fprintf(file, ", ");
    NaClPrintExpFlags(file, node->flags);
    fprintf(file, " },\n");
  }
  fprintf(file, "};\n");
}

/* Print out the given (memory offset) expression node to the given file.
 * Returns the index of the node following the given (indexed) memory offset.
 */
static int NaClPrintDisassembledMemOffset(FILE* file,
                                      NaClExpVector* vector,
                                      int index) {
  int r1_index = index + 1;
  int r2_index = r1_index + NaClExpWidth(vector, r1_index);
  int scale_index = r2_index + NaClExpWidth(vector, r2_index);
  int disp_index = scale_index + NaClExpWidth(vector, scale_index);
  NaClOpKind r1 = NaClGetExpVectorRegister(vector, r1_index);
  NaClOpKind r2 = NaClGetExpVectorRegister(vector, r2_index);
  int scale = (int) NaClGetExpConstant(vector, scale_index);
  uint64_t disp = NaClGetExpConstant(vector, disp_index);
  assert(ExprMemOffset == vector->node[index].kind);
  fprintf(file,"[");
  if (r1 != RegUnknown) {
    NaClPrintDisassembledRegKind(file, r1);
  }
  if (r2 != RegUnknown) {
    if (r1 != RegUnknown) {
      fprintf(file, "+");
    }
    NaClPrintDisassembledRegKind(file, r2);
    fprintf(file, "*%d", scale);
  }
  if (disp != 0) {
    if ((r1 != RegUnknown || r2 != RegUnknown) &&
        !NaClIsExpNegativeConstant(vector, disp_index)) {
      fprintf(file, "+");
    }
    /* Recurse to handle print using format flags. */
    NaClPrintDisassembledExp(file, vector, disp_index);
  } else if (r1 == RegUnknown && r2 == RegUnknown) {
    /* be sure to generate case: [0x0]. */
    NaClPrintDisassembledExp(file, vector, disp_index);
  }
  fprintf(file, "]");
  return disp_index + NaClExpWidth(vector, disp_index);
}

/* Print out the given (segment address) expression node to the
 * given file. Returns the index of the node following the
 * given (indexed) segment address.
 */
static int NaClPrintDisassembledSegmentAddr(FILE* file,
                                            NaClExpVector* vector,
                                            int index) {
  assert(ExprSegmentAddress == vector->node[index].kind);
  index = NaClPrintDisassembledExp(file, vector, index + 1);
  if (vector->node[index].kind != ExprMemOffset) {
    fprintf(file, ":");
  }
  return NaClPrintDisassembledExp(file, vector, index);
}

/* Print out the given expression node to the given file.
 * Returns the index of the node following the given indexed
 * expression.
 */
static int NaClPrintDisassembledExp(FILE* file,
                                    NaClExpVector* vector,
                                    uint32_t index) {
  NaClExp* node;
  assert(index < vector->number_expr_nodes);
  node = &vector->node[index];
  switch (node->kind) {
    default:
      fprintf(file, "undefined");
      return index + 1;
    case ExprRegister:
      NaClPrintDisassembledReg(file, node);
      return index + 1;
    case OperandReference:
      return NaClPrintDisassembledExp(file, vector, index + 1);
    case ExprConstant:
      NaClPrintDisassembledConst(file, node);
      return index + 1;
    case ExprConstant64:
      NaClPrintDisassembledConst64(file, vector, index);
      return index + 3;
    case ExprSegmentAddress:
      return NaClPrintDisassembledSegmentAddr(file, vector, index);
    case ExprMemOffset:
      return NaClPrintDisassembledMemOffset(file, vector, index);
  }
}

/* Print the given instruction opcode of the give state, to the
 * given file.
 */
static void NaClPrintDisassembled(FILE* file,
                                  NaClInstState* state,
                                  NaClInst* inst) {
  uint32_t tree_index = 0;
  Bool is_first = TRUE;
  NaClExpVector* vector = NaClInstStateExpVector(state);
  NaClPrintLower(file, (char*) NaClMnemonicName(inst->name));
  while (tree_index < vector->number_expr_nodes) {
    if (vector->node[tree_index].kind != OperandReference ||
        (0 == (vector->node[tree_index].flags & NACL_EFLAG(ExprImplicit)))) {
      if (is_first) {
        putc(' ', file);
        is_first = FALSE;
      } else {
        fprintf(file, ", ");
      }
      tree_index = NaClPrintDisassembledExp(file, vector, tree_index);
    } else {
      tree_index += NaClExpWidth(vector, tree_index);
    }
  }
}

void NaClInstStateInstPrint(FILE* file, NaClInstState* state) {
  int i;
  NaClInst* inst;

  /* Print out the address and the inst bytes. */
  int length = NaClInstStateLength(state);
  DEBUG(NaClInstPrint(stdout, NaClInstStateInst(state)));
  DEBUG(NaClExpVectorPrint(stdout, NaClInstStateExpVector(state)));
  fprintf(file, "%"NACL_PRIxNaClPcAddressAll": ", NaClInstStateVpc(state));
  for (i = 0; i < length; ++i) {
    fprintf(file, "%02"NACL_PRIx8" ", NaClInstStateByte(state, i));
  }
  for (i = length; i < NACL_MAX_BYTES_PER_X86_INSTRUCTION; ++i) {
    fprintf(file, "   ");
  }

  /* Print out the assembly instruction it disassembles to. */
  inst = NaClInstStateInst(state);
  NaClPrintDisassembled(file, state, inst);

  /* Print out if not allowed in native client (as a comment). */
  if (! NaClInstStateIsNaClLegal(state)) {
    fprintf(file, "; *NACL Disallows!*");
  }
  putc('\n', file);
}

char* NaClInstStateInstructionToString(struct NaClInstState* state) {
  FILE* file;
  char* out_string;
  struct stat st;
  size_t file_size;
  size_t fread_items;

  do {
    file = fopen("out_file", "w");
    if (file == NULL) break;

#if NACL_LINUX || NACL_OSX
    chmod("out_file", S_IRUSR | S_IWUSR);
#endif

    NaClInstStateInstPrint(file, state);
    fclose(file);

    if (stat("out_file", &st)) break;

    file_size = (size_t) st.st_size;
    if (file_size == 0) break;

    out_string = (char*) malloc(file_size + 1);
    if (out_string == NULL) break;

    file = fopen("out_file", "r");
    if (file == NULL) break;

    fread_items = fread(out_string, file_size, 1, file);
    if (fread_items != 1) break;

    fclose(file);
    unlink("out_file");
    out_string[file_size] = 0;
    return out_string;
  } while (0);

  /* failure */
  if (file != NULL) fclose(file);
  unlink("out_file");
  return NULL;
}

int NaClExpWidth(NaClExpVector* vector, int node) {
  int i;
  int count = 1;
  int num_kids = NaClExpKindRank(vector->node[node].kind);
  for (i = 0; i < num_kids; i++) {
    count += NaClExpWidth(vector, node + count);
  }
  return count;
}

int NaClGetExpKidIndex(NaClExpVector* vector, int node, int kid) {
  node++;
  while (kid-- > 0) {
    node += NaClExpWidth(vector, node);
  }
  return node;
}

int NaClGetExpParentIndex(NaClExpVector* vector, int index) {
  int node_rank;
  int num_kids = 1;
  while (index > 0) {
    --index;
    node_rank = NaClExpKindRank(vector->node[index].kind);
    if (node_rank >= num_kids) {
      return index;
    } else {
      num_kids -= (node_rank - 1);
    }
  }
  return 0;
}

int NaClGetNthExpKind(NaClExpVector* vector,
                      NaClExpKind kind,
                      int n) {
  if (n > 0) {
    uint32_t i;
    for (i = 0; i < vector->number_expr_nodes; ++i) {
      if (kind == vector->node[i].kind) {
        --n;
        if (n == 0) return i;
      }
    }
  }
  return -1;
}

uint64_t NaClGetExpConstant(NaClExpVector* vector, int index) {
  NaClExp* node = &vector->node[index];
  switch (node->kind) {
    case ExprConstant:
      return node->value;
    case ExprConstant64:
      return (uint64_t) (uint32_t) vector->node[index+1].value |
          (((uint64_t) vector->node[index+2].value) << 32);
    default:
      assert(0);
  }
  /* NOT REACHED */
  return 0;
}

void NaClSplitExpConstant(uint64_t val, uint32_t* val1, uint32_t* val2) {
  *val1 = (uint32_t) (val & 0xFFFFFFFF);
  *val2 = (uint32_t) (val >> 32);
}

Bool NaClIsExpNegativeConstant(NaClExpVector* vector, int index) {
  NaClExp* node = &vector->node[index];
  switch (node->kind) {
    case ExprConstant:
      if (node->flags & NACL_EFLAG(ExprUnsignedHex) ||
          node->flags & NACL_EFLAG(ExprUnsignedInt)) {
        return FALSE;
      } else {
        /* Assume signed value. */
        return NaClGetSignExtendedValue(node) < 0;
      }
      break;
    case ExprConstant64:
      if (node->flags & NACL_EFLAG(ExprUnsignedHex) ||
          node->flags & NACL_EFLAG(ExprUnsignedInt)) {
        return FALSE;
      } else {
        /* Assume signed value. */
        int64_t value = (int64_t) NaClGetExpConstant(vector, index);
        return value < 0;
      }
      break;
    default:
      break;
  }
  return FALSE;
}
