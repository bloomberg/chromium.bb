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

void PrintExprNodeFlags(FILE* file, ExprNodeFlags flags) {
  if (flags == 0) {
    fprintf(file, "0");
  } else {
    ExprNodeFlag f;
    Bool is_first = TRUE;
    for (f = 0; f < ExprNodeFlagEnumSize; f++) {
      if (flags & ExprFlag(f)) {
        if (is_first) {
          is_first = FALSE;
        } else {
          fprintf(file, " | ");
        }
        fprintf(file, "%s", ExprNodeFlagName(f));
      }
    }
  }
}

typedef struct {
  /* The name of the expression operator. */
  ExprNodeKind name;
  /* The rank (i.e. number of children) the expression operator has. */
  const int rank;
} ExprNodeKindDescriptor;

/* The print names of valid ExprNodeKind values. */
static const ExprNodeKindDescriptor
g_ExprNodeKindDesc[ExprNodeKindEnumSize + 1]= {
  {UndefinedExp, 0},
  {ExprRegister, 0},
  {OperandReference, 1},
  {ExprConstant, 0},
  {ExprConstant64, 2},
  {ExprSegmentAddress, 2},
  {ExprMemOffset, 4},
};

int ExprNodeKindRank(ExprNodeKind kind) {
  assert(kind == g_ExprNodeKindDesc[kind].name);
  return g_ExprNodeKindDesc[kind].rank;
}

/* Returns the register defined by the given node. */
OperandKind GetNodeRegister(ExprNode* node) {
  assert(node->kind == ExprRegister);
  return (OperandKind) node->value;
}

/* Returns the name of the register defined by the indexed node in the
 * vector of nodes.
 */
OperandKind GetNodeVectorRegister(ExprNodeVector* vector,
                                                int node) {
  return GetNodeRegister(&vector->node[node]);
}

static int PrintDisassembledExp(FILE* file,
                                ExprNodeVector* vector,
                                uint32_t index);

/* Print the characters in the given string using lower case. */
static void PrintLower(FILE* file, char* str) {
  while (*str) {
    putc(tolower(*str), file);
    ++str;
  }
}

/* Return the sign (extended) integer in the given expr node. */
static int32_t GetSignExtendedValue(ExprNode* node) {
  if (node->flags & ExprFlag(ExprSize8)) {
    return (int8_t) node->value;
  } else if (node->flags & ExprFlag(ExprSize16)) {
    return (int16_t) node->value;
  } else {
    return (int32_t) node->value;
  }
}

/* Print out the given (constant) expression node to the given file. */
static void PrintDisassembledConst(FILE* file, ExprNode* node) {
  assert(node->kind == ExprConstant);
  if (node->flags & ExprFlag(ExprUnsignedHex)) {
    fprintf(file, "0x%"NACL_PRIx32, node->value);
  } else if (node->flags & ExprFlag(ExprSignedHex)) {
    int32_t value = GetSignExtendedValue(node);
    if (value < 0) {
      value = -value;
      fprintf(file, "-0x%"NACL_PRIx32, value);
    } else {
      fprintf(file, "0x%"NACL_PRIx32, value);
    }
  } else if (node->flags & ExprFlag(ExprUnsignedInt)) {
    fprintf(file, "%"NACL_PRIu32, node->value);
  } else {
    /* Assume ExprSignedInt. */
    fprintf(file, "%"NACL_PRId32, (int32_t) GetSignExtendedValue(node));
  }
}

/* Print out the given (64-bit constant) expression node to the given file. */
static void PrintDisassembledConst64(
    FILE* file, ExprNodeVector* vector, int index) {
  ExprNode* node;
  uint64_t value;
  node = &vector->node[index];
  assert(node->kind == ExprConstant64);
  value = GetExprConstant(vector, index);
  if (node->flags & ExprFlag(ExprUnsignedHex)) {
    fprintf(file, "0x%"NACL_PRIx64, value);
  } else if (node->flags & ExprFlag(ExprSignedHex)) {
    int64_t val = (int64_t) value;
    if (val < 0) {
      val = -val;
      fprintf(file, "-0x%"NACL_PRIx64, val);
    } else {
      fprintf(file, "0x%"NACL_PRIx64, val);
    }
  } else if (node->flags & ExprFlag(ExprUnsignedInt)) {
    fprintf(file, "%"NACL_PRIu64, value);
  } else {
    fprintf(file, "%"NACL_PRId64, (int64_t) value);
  }
}

/* Print out the disassembled representation of the given register
 * to the given file.
 */
static void PrintDisassembledRegKind(FILE* file, OperandKind reg) {
  const char* name = OperandKindName(reg);
  char* str = strstr(name, "Reg");
  putc('%', file);
  PrintLower(file, str == NULL ? (char*) name : str + strlen("Reg"));
}

static INLINE void PrintDisassembledReg(FILE* file, ExprNode* node) {
  PrintDisassembledRegKind(file, GetNodeRegister(node));
}

void PrintExprNodeVector(FILE* file, ExprNodeVector* vector) {
  uint32_t i;
  fprintf(file, "ExprNodeVector[%d] = {\n", vector->number_expr_nodes);
  for (i = 0; i < vector->number_expr_nodes; i++) {
    ExprNode* node = &vector->node[i];
    fprintf(file, "  { %s[%d] , ",
            ExprNodeKindName(node->kind),
            ExprNodeKindRank(node->kind));
    switch (node->kind) {
      case ExprRegister:
        PrintDisassembledReg(file, node);
        break;
      case ExprConstant:
        PrintDisassembledConst(file, node);
        break;
      case ExprConstant64:
        PrintDisassembledConst64(file, vector, i);
        break;
      default:
        fprintf(file, "%"NACL_PRIu32, node->value);
        break;
    }
    fprintf(file, ", ");
    PrintExprNodeFlags(file, node->flags);
    fprintf(file, " },\n");
  }
  fprintf(file, "};\n");
}

/* Print out the given (memory offset) expression node to the given file.
 * Returns the index of the node following the given (indexed) memory offset.
 */
static int PrintDisassembledMemOffset(FILE* file,
                                      ExprNodeVector* vector,
                                      int index) {
  int r1_index = index + 1;
  int r2_index = r1_index + ExprNodeWidth(vector, r1_index);
  int scale_index = r2_index + ExprNodeWidth(vector, r2_index);
  int disp_index = scale_index + ExprNodeWidth(vector, scale_index);
  OperandKind r1 = GetNodeVectorRegister(vector, r1_index);
  OperandKind r2 = GetNodeVectorRegister(vector, r2_index);
  int scale = (int) GetExprConstant(vector, scale_index);
  uint64_t disp = GetExprConstant(vector, disp_index);
  assert(ExprMemOffset == vector->node[index].kind);
  fprintf(file,"[");
  if (r1 != RegUnknown) {
    PrintDisassembledRegKind(file, r1);
  }
  if (r2 != RegUnknown) {
    if (r1 != RegUnknown) {
      fprintf(file, "+");
    }
    PrintDisassembledRegKind(file, r2);
    fprintf(file, "*%d", scale);
  }
  if (disp != 0) {
    if ((r1 != RegUnknown || r2 != RegUnknown) &&
        !IsExprNegativeConstant(vector, disp_index)) {
      fprintf(file, "+");
    }
    /* Recurse to handle print using format flags. */
    PrintDisassembledExp(file, vector, disp_index);
  } else if (r1 == RegUnknown && r2 == RegUnknown) {
    /* be sure to generate case: [0x0]. */
    PrintDisassembledExp(file, vector, disp_index);
  }
  fprintf(file, "]");
  return disp_index + ExprNodeWidth(vector, disp_index);
}

/* Print out the given (segment address) expression node to the
 * given file. Returns the index of the node following the
 * given (indexed) segment address.
 */
static int PrintDisassembledSegmentAddr(FILE* file,
                                        ExprNodeVector* vector,
                                        int index) {
  assert(ExprSegmentAddress == vector->node[index].kind);
  index = PrintDisassembledExp(file, vector, index + 1);
  if (vector->node[index].kind != ExprMemOffset) {
    fprintf(file, ":");
  }
  return PrintDisassembledExp(file, vector, index);
}

/* Print out the given expression node to the given file.
 * Returns the index of the node following the given indexed
 * expression.
 */
static int PrintDisassembledExp(FILE* file,
                                ExprNodeVector* vector,
                                uint32_t index) {
  ExprNode* node;
  assert(index < vector->number_expr_nodes);
  node = &vector->node[index];
  switch (node->kind) {
    default:
      fprintf(file, "undefined");
      return index + 1;
    case ExprRegister:
      PrintDisassembledReg(file, node);
      return index + 1;
    case OperandReference:
      return PrintDisassembledExp(file, vector, index + 1);
    case ExprConstant:
      PrintDisassembledConst(file, node);
      return index + 1;
    case ExprConstant64:
      PrintDisassembledConst64(file, vector, index);
      return index + 3;
    case ExprSegmentAddress:
      return PrintDisassembledSegmentAddr(file, vector, index);
    case ExprMemOffset:
      return PrintDisassembledMemOffset(file, vector, index);
  }
}

/* Print the given instruction opcode of the give state, to the
 * given file.
 */
static void PrintDisassembled(FILE* file,
                              NcInstState* state,
                              Opcode* opcode) {
  uint32_t tree_index = 0;
  Bool is_first = TRUE;
  ExprNodeVector* vector = NcInstStateNodeVector(state);
  PrintLower(file, (char*) InstMnemonicName(opcode->name));
  while (tree_index < vector->number_expr_nodes) {
    if (vector->node[tree_index].kind != OperandReference ||
        (0 == (vector->node[tree_index].flags & ExprFlag(ExprImplicit)))) {
      if (is_first) {
        putc(' ', file);
        is_first = FALSE;
      } else {
        fprintf(file, ", ");
      }
      tree_index = PrintDisassembledExp(file, vector, tree_index);
    } else {
      tree_index += ExprNodeWidth(vector, tree_index);
    }
  }
}

void PrintNcInstStateInstruction(FILE* file, NcInstState* state) {
  int i;
  Opcode* opcode;

  /* Print out the address and the opcode bytes. */
  int length = NcInstStateLength(state);
  DEBUG(PrintOpcode(stdout, NcInstStateOpcode(state)));
  DEBUG(PrintExprNodeVector(stdout, NcInstStateNodeVector(state)));
  fprintf(file, "%"NACL_PRIxPcAddressAll": ", NcInstStateVpc(state));
  for (i = 0; i < length; ++i) {
      fprintf(file, "%02"NACL_PRIx8" ", NcInstStateByte(state, i));
  }
  for (i = length; i < MAX_BYTES_PER_X86_INSTRUCTION; ++i) {
    fprintf(file, "   ");
  }

  /* Print out the assembly instruction it disassembles to. */
  opcode = NcInstStateOpcode(state);
  PrintDisassembled(file, state, opcode);

  /* Print out if not allowed in native client (as a comment). */
  if (! NcInstStateIsNaclLegal(state)) {
    fprintf(file, "; *NACL Disallows!*");
  }
  putc('\n', file);
}

char* PrintNcInstStateInstructionToString(struct NcInstState* state) {
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

    PrintNcInstStateInstruction(file, state);
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

int ExprNodeWidth(ExprNodeVector* vector, int node) {
  int i;
  int count = 1;
  int num_kids = ExprNodeKindRank(vector->node[node].kind);
  for (i = 0; i < num_kids; i++) {
    count += ExprNodeWidth(vector, node + count);
  }
  return count;
}

int GetExprNodeKidIndex(ExprNodeVector* vector, int node, int kid) {
  node++;
  while (kid-- > 0) {
    node += ExprNodeWidth(vector, node);
  }
  return node;
}

int GetExprNodeParentIndex(ExprNodeVector* vector, int index) {
  int node_rank;
  int num_kids = 1;
  while (index > 0) {
    --index;
    node_rank = ExprNodeKindRank(vector->node[index].kind);
    if (node_rank >= num_kids) {
      return index;
    } else {
      num_kids -= (node_rank - 1);
    }
  }
  return 0;
}

int GetNthNodeKind(ExprNodeVector* vector,
                   ExprNodeKind kind,
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

uint64_t GetExprConstant(ExprNodeVector* vector, int index) {
  ExprNode* node = &vector->node[index];
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

void SplitExprConstant(uint64_t val, uint32_t* val1, uint32_t* val2) {
  *val1 = (uint32_t) (val & 0xFFFFFFFF);
  *val2 = (uint32_t) (val >> 32);
}

Bool IsExprNegativeConstant(ExprNodeVector* vector, int index) {
  ExprNode* node = &vector->node[index];
  switch (node->kind) {
    case ExprConstant:
      if (node->flags & ExprFlag(ExprUnsignedHex) ||
          node->flags & ExprFlag(ExprUnsignedInt)) {
        return FALSE;
      } else {
        /* Assume signed value. */
        return GetSignExtendedValue(node) < 0;
      }
      break;
    case ExprConstant64:
      if (node->flags & ExprFlag(ExprUnsignedHex) ||
          node->flags & ExprFlag(ExprUnsignedInt)) {
        return FALSE;
      } else {
        /* Assume signed value. */
        int64_t value = (int64_t) GetExprConstant(vector, index);
        return value < 0;
      }
      break;
    default:
      break;
  }
  return FALSE;
}
