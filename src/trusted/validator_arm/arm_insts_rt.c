/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Defines runtime suppport for the modeled ARM instrucitons.
 */

#include "native_client/src/trusted/validator_arm/arm_insts_rt.h"
#include "native_client/src/trusted/validator_arm/arm_insts.h"
#include "native_client/src/shared/utils/formatting.h"

/*
 * Model an instruction that isn't known.
 */
const OpInfo kUndefinedArmOp =
  { "???",
    ARM_UNKNOWN_INST,
    &kArmUndefinedAccessMode,
    ARM_UNDEFINED,
    0,
    "%?",
    ARM_WORD_LENGTH,
    NULL,
    ARM_DONT_CARE_MASK };

/*
 * Return the condition code name for the given condition value.
 */
static const char* GetCondName(int32_t cond) {
  if (cond > 15) {
    printf("GetCondName(%u) is not defined\n", cond);
    cond = 15;
  }
  if (FLAGS_name_cond) {
    /* Define the 16 possible condition codes, when the name comes first. */
    static const char* ArmNameCond[16] = {
      "eq", /* 0000 */
      "ne", /* 0001 */
      "cs", /* 0010 */
      "cc", /* 0011 */
      "mi", /* 0100 */
      "pl", /* 0101 */
      "vs", /* 0110 */
      "vc", /* 0111 */
      "hi", /* 1000 */
      "ls", /* 1001 */
      "ge", /* 1010 */
      "lt", /* 1011 */
      "gt", /* 1100 */
      "le", /* 1101 */
      "",   /* 1110 - This implies always */
      ""    /* 1111 - This implies that there isn't a condition code. */
    };
    return ArmNameCond[cond];
  } else {
    /* Define the 16 possible condition codes, when the condition comes first.
     * Be sure to align choices so that indentation looks nice.
     */
    static const char* ArmCondName[16] = {
      "(eq)\t", /* 0000 */
      "(ne)\t", /* 0001 */
      "(cs)\t", /* 0010 */
      "(cc)\t", /* 0011 */
      "(mi)\t", /* 0100 */
      "(pl)\t", /* 0101 */
      "(vs)\t", /* 0110 */
      "(vc)\t", /* 0111 */
      "(hi)\t", /* 1000 */
      "(ls)\t", /* 1001 */
      "(ge)\t", /* 1010 */
      "(lt)\t", /* 1011 */
      "(gt)\t", /* 1100 */
      "(le)\t", /* 1101 */
      "\t",     /* 1110 - This implies always */
      "\t"      /* 1111 - This implies that there isn't a condition code. */
    };
    return ArmCondName[cond];
  }
}

/* Return the corresponding field masks for the given mask. */
static const char* GetMsrFieldMask(uint32_t mask) {
  /* Possible field masks for msr instruction. */
  static const char* kMsrFieldMasks[16] = {
    "",         /* 0000 */
    "c",        /* 0001 */
    "x",        /* 0010 */
    "xc",       /* 0011 */
    "s",        /* 0100 */
    "sc",       /* 0101 */
    "sx",       /* 0110 */
    "sxc",      /* 0111 */
    "f",        /* 1000 */
    "fc",       /* 1001 */
    "fx",       /* 1010 */
    "fxc",      /* 1011 */
    "fs",       /* 1100 */
    "fsc",      /* 1110 */
    "fsxc",     /* 1111 */
  };
  return kMsrFieldMasks[mask];
}

/*
 * Extract the masked bits from the value, and then right
 * shift to normalize the value (i.e. shift to the
 * smallest, non-zero bit in the mask.
 *
 * arguments:
 *   value - the value to mask
 *   mask - the mask to apply.
 */
static uint32_t GetMaskedValue(uint32_t value, int32_t mask) {
  /* Note: this is innefficient. However, once working we can */
  /* find a faster solution. */
  return (value & mask) >> PosOfLowestBitSet(mask);
}

uint32_t RealAddress(uint32_t pc, uint32_t displacement) {
  /* Sign extend. */
  if (displacement & 0x00800000) {
    displacement |= 0xFF000000;
  }

  /* left shift two bits */
  displacement = displacement << 2;

  /* add the contents of the PC (plus 8). */
  displacement += pc + 8;
  return displacement;
}

// Adds bit[24]*2 to real address
static uint32_t RealAddressPlusH(uint32_t pc,
                                 uint32_t displacement,
                                 Bool h_bit) {
  return RealAddress(pc, displacement) + (h_bit << 0x1);
}

static int32_t BuildOffsetFromPair(int32_t Hi4Bits, int32_t Lo4Bits) {
  return (Hi4Bits << 4) | Lo4Bits;
}

static int32_t BuildOffsetFromArgs(const InstValues* values) {
  int32_t value = BuildOffsetFromPair(values->arg1, values->arg2);
  value = BuildOffsetFromPair(value, values->arg3);
  return BuildOffsetFromPair(value, values->arg4);
}

uint32_t ImmediateRotateRight(uint32_t immediate, int32_t rotate) {
  rotate = rotate & 0x1F;  /* remove unnecessary shifts. */
  if (0 == rotate) {
    return immediate;
  } else {
    return (immediate << (32 - rotate)) | (immediate >> rotate);
  }
}

/*
 * Append the real address (defined by the given program counter and
 * the corresponding immediate address), into the buffer at the
 * cursor position. When the buffer fills, no additional text is added
 * to the buffer,even though the cursor is incremented accordingly.
 */
static void RealAddressAppend(char* buffer,
                              size_t buffer_size,
                              uint32_t pc,
                              int32_t address,
                              size_t* cursor) {
  char addr_string[24];
  snprintf(addr_string, sizeof(addr_string), "%x", RealAddress(pc, address));
  FormatAppend(buffer, buffer_size, addr_string, cursor);
}

/*
 * Append the real adddres plus the H bit (defined by the given program
 * counter, corresponding immediate address, and the value of bit H), into
 * the buffer at the cursor position. When the buffer fills, no additional text
 * is added to the buffer, even though the cursor is incremented accordingly.
 */
static void RealAddressPlusHAppend(char* buffer,
                                   size_t buffer_size,
                                   uint32_t pc,
                                   int32_t address,
                                   Bool h_bit,
                                   size_t* cursor) {
  char addr_string[24];
  snprintf(addr_string, sizeof(addr_string),
           "%x", RealAddressPlusH(pc, address, h_bit));
  FormatAppend(buffer, buffer_size, addr_string, cursor);
}

/*
 * Append the (right rotated) immediate value into the buffer at the
 * cursor position. When the buffer fills, no additional text is added
 * to the buffer,even though the cursor is incremented accordingly.
 */
static void ImmediateRotateRightAppend(char* buffer,
                                       size_t buffer_size,
                                       uint32_t immediate,
                                       int32_t rotate,
                                       size_t* cursor) {
  char value_string[48];
  snprintf(value_string, sizeof(value_string),
           "%d", ImmediateRotateRight(immediate, rotate));
  FormatAppend(buffer, buffer_size, value_string, cursor);
}

/* The kinds of MultiplyAccumulateSuffixes */
static const char* kMultiplyAccumulateSuffixes[2] = {
  "B",
  "T"
};

/* Append multiple accumulate suffixes into the buffer at the cursor position.
 * When the buffer fills, no additional text is added to the buffer,
 * even though the cursor is incremented accordingly.
 *
 * arguments:
 *   buffer - The buffer to append the text.
 *   buffer_size - The size of the buffer.
 *   bits  - bits defining a multiply accumulate suffixes.
 *   mask   - bits to check for (and add) multiply accumulate suffixes.
 *   cursor - The index into the buffer, where the next character should
 *          be added, if there is sufficient room in the buffer.
 */
static void MultiplyAccumulateSuffixAppend(char* buffer,
                                           size_t buffer_size,
                                           uint32_t bits,
                                           uint32_t mask,
                                           size_t* cursor) {
  int i;
  for (i = 0; i < 32; ++i) {
    if (mask & 0x1) {
      FormatAppend(buffer, buffer_size,
                   kMultiplyAccumulateSuffixes[bits & 0x1],
                   cursor);
    }
    mask <<= 1;
    bits <<= 1;
  }
}

/* Append the name and condition of the given instruction into the buffer
 * at the cursor position. When the buffer fills, no additional text is added
 * to the buffer,even though the cursor is incremented accordingly.
 */
static void ConditionAndNameAppend(char* buffer,
                                   size_t buffer_size,
                                   const char* name,
                                   int32_t cond,
                                   size_t* cursor) {
  if (FLAGS_name_cond) {
    FormatAppend(buffer, buffer_size, name, cursor);
    FormatAppend(buffer, buffer_size, GetCondName(cond), cursor);
  } else {
    FormatAppend(buffer, buffer_size, GetCondName(cond), cursor);
    FormatAppend(buffer, buffer_size, name, cursor);
  }
}

/* Append the name and condition of the given instruction into the buffer
 * at the currsor position. When the buffer fills, no additional text is added
 * to the buffer,even though the cursor is incremented accordingly.
 */
static void ConditionalNameAppend(char* buffer,
                                  size_t buffer_size,
                                  NcDecodedInstruction* inst,
                                  size_t* cursor) {
  ConditionAndNameAppend(buffer, buffer_size,
                         inst->matched_inst->name,
                         inst->values.cond,
                         cursor);
}

/* Appends the name, multiply accumulate suffixes, and condition of the
 * given instruction into the buffer at the cursor position.
 * When the buffer fills, no additional text is added to the buffer,
 * even though the cursor is incremented accordingly.
 */
static void ConditionalNameMultiplyAccumulateSuffixAppend(
    char* buffer,
    size_t buffer_size,
    NcDecodedInstruction* inst,
    uint32_t mask,
    size_t* cursor) {
  if (FLAGS_name_cond) {
    FormatAppend(buffer, buffer_size, inst->matched_inst->name, cursor);
    MultiplyAccumulateSuffixAppend(buffer, buffer_size,
                                   inst->values.
                                   shift,
                                   mask,
                                   cursor);
    FormatAppend(buffer, buffer_size, GetCondName(inst->values.cond), cursor);
  } else {
    FormatAppend(buffer, buffer_size, GetCondName(inst->values.cond), cursor);
    FormatAppend(buffer, buffer_size, inst->matched_inst->name, cursor);
    MultiplyAccumulateSuffixAppend(buffer, buffer_size,
                                   inst->values.shift,
                                   mask,
                                   cursor);
  }
}

/*
 * Append the register name (with the given index) into the buffer
 * at the cursor position. When the buffer fills, no additional text is added
 * to the buffer,even though the cursor is incremented accordingly.
 */
static void RegisterAppend(char* buffer,
                           size_t buffer_size,
                           uint32_t index,
                           size_t* cursor) {
  /* The print names of known registers. */
  static const char* kRegisterName[16] = {
    "r0",
    "r1",
    "r2",
    "r3",
    "r4",
    "r5",
    "r6",
    "r7",
    "r8",
    "r9",
    "sl",
    "fp",
    "ip",
    "sp",
    "lr",
    "pc"
  };
  if (index < 16) {
    FormatAppend(buffer,
                 buffer_size,
                 kRegisterName[index],
                 cursor);
  } else {
    /* This shouldn't happen, but be safe. */
    char reg_string[24];
    snprintf(reg_string, sizeof(reg_string), "r%u", index);
    FormatAppend(buffer, buffer_size, reg_string, cursor);
  }
}

/*
 * Append the coprocessor register name (with the given index) into
 * the buffer at the cursor position. When the buffer fills, no
 * additional text is added to the buffer,even though the cursor
 * is incremented accordingly.
 */
static void CoprocessorRegisterAppend(char* buffer,
                                      size_t buffer_size,
                                      uint32_t index,
                                      size_t* cursor) {
  char reg_string[24];
  snprintf(reg_string, sizeof(reg_string), "cr%u", index);
  FormatAppend(buffer, buffer_size, reg_string, cursor);
}

/*
 * Return the CPS effect for the corresponding imod value.
 */
static const char* CpsEffect(int32_t value) {
  switch (value) {
    case 0x2:
      return "ie";
    case 0x3:
      return "id";
    default:
      /* This should not happen. */
      return "??";
  }
}

/*
 * Append the CPS effect into the buffer at the cursor position.
 * When the buffer fills, no additional text is added to the buffer,
 * even though the cursor is incremented accordingly.
 */
static void CpsEffectAppend(char* buffer,
                            size_t buffer_size,
                            int32_t value,
                            size_t* cursor) {
  FormatAppend(buffer, buffer_size, CpsEffect(value), cursor);
}

/*
 * Translates the format directives for instructions, given the
 * corresponding decoded instruction data.
 *
 * format directives:
 * %a => immedate(address)
 * %A => immediate(address) + bit(24)*2.
 * %b => generate value by appending bits of arg1-arg4
 * %c => cond
 * %C => insert name and condition code (command line specific).
 * %d => add "s" suffix if S bit is set.
 * %e => CPS effect ((arg1 & 0xC) >> 2)
 * %f => MSR fields from arg1
 * %H - Combine arg3 and arg4 (which don't occur contiguously) into a single
 *      offset.
 * %i = immediate.
 * %I => right rotate(shift*2, immediate)
 * %L => add L if bit 0x4 of the opcode is set.
 * %m => multiply accumuate suffix append shift (last 2 bits)
 * %M => insert name, multiply accumulate suffix (%m) and condition code.
 * %n => matched instruction name
 * %o => opcode
 * %r => register(arg1)
 * %R => coprocessor register (arg1)
 * %S - Insert set of registers to load/store for multiple load/store.
 * %s => shift.
 * %t => multiply accumuate suffix append shift (second to last bit).
 * %T => insert name, multiply accumulate suffix (%t) and condition code.
 * %x => register(arg2)
 * %X => coprocessor register (arg2)
 * %y => register(arg3)
 * %y => coprocessor register (arg3)
 * %z => register(arg4)
 * %z => coprocessor register (arg4).
 * %1 => arg1
 * %2 => arg2
 * %3 => arg3
 * %# => if arg3 == 0 then 32 else arg3
 * %4 => arg4
 * %+ => arg1 + 1;
 * %- - If the load/store operation modifies by a negative offset, replace
 *      with a - (minus sign), and nothing otherwise.
 + %? => insert ??? for unknown instruction name.
 */
static Bool DescribeInstDirective(char directive,
                                  char* buffer,
                                  size_t buffer_size,
                                  void* data,
                                  size_t* cursor) {
  NcDecodedInstruction* mstate = (NcDecodedInstruction*) data;
  switch (directive) {
    case 'a':
      RealAddressAppend(buffer,
                        buffer_size,
                        mstate->vpc,
                        mstate->values.immediate,
                        cursor);
      return TRUE;
    case 'A':
      RealAddressPlusHAppend(buffer,
                             buffer_size,
                             mstate->vpc,
                             mstate->values.immediate,
                             mstate->values.opcode,
                             cursor);
      return TRUE;
    case 'b':
      ValueAppend(buffer,
                  buffer_size,
                  BuildOffsetFromArgs(&(mstate->values)),
                  cursor);
      return TRUE;
    case 'c':
      FormatAppend(buffer,
                   buffer_size,
                   GetCondName(mstate->values.cond),
                   cursor);
      return TRUE;
    case 'd':
      {
        if (GetBit(mstate->inst, ARM_S_BIT)) {
          FormatAppend(buffer, buffer_size, "s", cursor);
        }
      }
      return TRUE;
    case 'C':
      ConditionalNameAppend(buffer, buffer_size, mstate, cursor);
      return TRUE;
    case 'e':
      CpsEffectAppend(buffer, buffer_size, mstate->values.arg1, cursor);
      return TRUE;
    case 'f':
      FormatAppend(buffer,
                   buffer_size,
                   GetMsrFieldMask(mstate->values.arg1),
                   cursor);
      return TRUE;
    case 'H':
      ValueAppend(buffer,
                  buffer_size,
                  BuildOffsetFromPair(mstate->values.arg3,
                                      mstate->values.arg4),
                  cursor);
      return TRUE;
    case 'i':
      ValueAppend(buffer, buffer_size, mstate->values.immediate, cursor);
      return TRUE;
    case 'I':
      ImmediateRotateRightAppend(buffer,
                                 buffer_size,
                                 mstate->values.immediate,
                                 mstate->values.shift * 2,
                                 cursor);
      return TRUE;
    case 'L':
      {
        if (mstate->values.opcode & 0x4) {
          FormatAppend(buffer, buffer_size, "l", cursor);
        }
      }
      return TRUE;
    case 'm':
      MultiplyAccumulateSuffixAppend(buffer,
                                     buffer_size,
                                     mstate->values.shift,
                                     0x3,
                                     cursor);
      return TRUE;
    case 'M':
      ConditionalNameMultiplyAccumulateSuffixAppend(buffer,
                                                    buffer_size,
                                                    mstate,
                                                    0x3,
                                                    cursor);
      return TRUE;
    case 'n':
      ConditionAndNameAppend(buffer, buffer_size,
                             mstate->matched_inst->name, 0xE, cursor);
      return TRUE;
    case 'o':
      ValueAppend(buffer, buffer_size, mstate->values.opcode, cursor);
      return TRUE;
    case 'r':
      RegisterAppend(buffer, buffer_size, mstate->values.arg1, cursor);
      return TRUE;
    case 'R':
      CoprocessorRegisterAppend(buffer, buffer_size,
                                mstate->values.arg1, cursor);
      return TRUE;
    case 's':
      ValueAppend(buffer, buffer_size, mstate->values.shift, cursor);
      return TRUE;
    case 'S':
      {
        int i;
        Bool is_first = TRUE;
        FormatAppend(buffer, buffer_size, "{", cursor);
        for (i = 0; i < 16; ++i) {
          if (GetBit(mstate->values.immediate, i)) {
            if (is_first) {
              is_first = FALSE;
            } else {
              FormatAppend(buffer, buffer_size, ", ", cursor);
            }
            RegisterAppend(buffer, buffer_size, i, cursor);
          }
        }
        FormatAppend(buffer, buffer_size, "}", cursor);
      }
      return TRUE;
    case 't':
      MultiplyAccumulateSuffixAppend(buffer,
                                     buffer_size,
                                     mstate->values.shift,
                                     0x2,
                                     cursor);
      return TRUE;
    case 'T':
      ConditionalNameMultiplyAccumulateSuffixAppend(buffer,
                                                    buffer_size,
                                                    mstate,
                                                    0x2,
                                                    cursor);
      return TRUE;
    case 'x':
      RegisterAppend(buffer, buffer_size, mstate->values.arg2, cursor);
      return TRUE;
    case 'X':
      CoprocessorRegisterAppend(buffer,
                                buffer_size,
                                mstate->values.arg2,
                                cursor);
      return TRUE;
    case 'y':
      RegisterAppend(buffer, buffer_size, mstate->values.arg3, cursor);
      return TRUE;
    case 'Y':
      CoprocessorRegisterAppend(buffer,
                                buffer_size,
                                mstate->values.arg3,
                                cursor);
      return TRUE;
    case 'z':
      RegisterAppend(buffer, buffer_size, mstate->values.arg4, cursor);
      return TRUE;
    case 'Z':
      CoprocessorRegisterAppend(buffer,
                                buffer_size,
                                mstate->values.arg4,
                                cursor);
      return TRUE;
    case '1':
      ValueAppend(buffer, buffer_size, mstate->values.arg1, cursor);
      return TRUE;
    case '2':
      ValueAppend(buffer, buffer_size, mstate->values.arg2, cursor);
      return TRUE;
    case '3':
      ValueAppend(buffer, buffer_size, mstate->values.arg3, cursor);
      return TRUE;
    case '#':
      ValueAppend(buffer, buffer_size,
                  (mstate->values.arg3 == 0 ? 32 : mstate->values.arg3),
                  cursor);
      return TRUE;
    case '4':
      ValueAppend(buffer, buffer_size, mstate->values.arg4, cursor);
      return TRUE;
    case '+':
      ValueAppend(buffer, buffer_size, mstate->values.arg1 + 1, cursor);
      return TRUE;
    case '-':
      if (0 == GetBit(mstate->inst, 23)) {
        FormatAppend(buffer, buffer_size, "-", cursor);
      }
      return TRUE;
    case '?':
      ConditionAndNameAppend(buffer, buffer_size, "???", 0xE, cursor);
      return TRUE;
    default:
      return FALSE;
  }
}

/*
 * Expands optional print fields for the disassembler.
 *
 * format driectives:
 * %h => If The offset pair defined by args 3 and 4 are non-zero,
 *       expand to ", #%-%H".
 */
static Bool ExpandOptionalPrintDirective(char directive,
                                         char* buffer,
                                         size_t buffer_size,
                                         void* data,
                                         size_t* cursor) {
  NcDecodedInstruction* inst = (NcDecodedInstruction*) data;
  switch (directive) {
    case 'h':
      {
        int32_t offset = BuildOffsetFromPair(inst->values.arg3,
                                             inst->values.arg4);
        if (offset != 0) {
          FormatAppend(buffer, buffer_size, ", #%-%H", cursor);
        }
        return TRUE;
      }
    default:
      return FALSE;
  }
}

static void InstAppend(char* buffer,
                       size_t buffer_size,
                       const NcDecodedInstruction* inst,
                size_t* cursor) {
  /* Note: We format twice, to allow the handling of optionally
   * printed values. The first pass checks conditions for expanding
   * optional fields, while the second fills in the field values.
   */
  char* tmp_buffer = (char*) calloc(buffer_size, sizeof(char));
  FormatData(tmp_buffer,
             buffer_size,
             inst->matched_inst->describe_format,
             (void*) inst,
             ExpandOptionalPrintDirective);
  FormatDataAppend(buffer,
                   buffer_size,
                   tmp_buffer,
                   (void*) inst,
                   DescribeInstDirective,
                   cursor);
  cfree(tmp_buffer);
}

Bool DescribeInst(char* buffer,
                  size_t buffer_size,
                  const NcDecodedInstruction* inst) {
  size_t cursor = 0;
  InstAppend(buffer, buffer_size, inst, &cursor);
  return cursor < buffer_size;
}


/*
 * Print out the specified field of the given ArmAccessMode
 * into the buffer at the cursor position.
 *
 * %m - field address_mode.
 * %s - field address_submode
 * %i - field index_mode.
 */
static Bool ArmAccessModeDirective(char directive,
                                   char* buffer,
                                   size_t buffer_size,
                                   void* data,
                                   size_t* cursor) {
  ArmAccessMode* access = (ArmAccessMode*)data;
  switch (directive) {
    case 'm':
      FormatAppend(buffer,
                   buffer_size,
                   GetArmAddressingModeName(access->address_mode),
                   cursor);
      return TRUE;
    case 's':
      FormatAppend(buffer,
                   buffer_size,
                   GetArmSubaddressingModeName(access->address_submode),
                   cursor);
      return TRUE;
    case 'i':
      FormatAppend(buffer,
                   buffer_size,
                   GetArmIndexingModeName(access->index_mode),
                   cursor);
      return TRUE;
    default:
      return FALSE;
  }
}

/*
 * Print a description of the given access mode into the buffer
 * at the cursor position. When the buffer fills, no additional
 * text is added to the buffer, even though the cursor is incremented
 * accordingly.
 */
static void AccessModeAppend(char* buffer,
                             size_t buffer_size,
                             const ArmAccessMode* access,
                             size_t* cursor) {
  FormatDataAppend(buffer,
                   buffer_size,
                   "{ %m , %s , %i }",
                   (void*) access,
                   ArmAccessModeDirective,
                   cursor);
}

/*
 * Prints out the specified field of an OpInfo structure into
 * the buffer at the cursor position.
 *
 * %n - name field.
 * %k - inst_kind field.
 * %a - inst_access field.
 * %t - inst_type field.
 * %s - arm_safe field.
 * %f - describe_format field.
 * %b - num_bytes field.
 * %e - expected_values field.
 */
static Bool OpInfoDirective(char directive,
                            char* buffer,
                            size_t buffer_size,
                            void* data,
                            size_t* cursor) {
  OpInfo* op = (OpInfo*) data;
  switch (directive) {
    case 'n':
      FormatAppend(buffer, buffer_size, op->name, cursor);
      return TRUE;
    case 'k':
      FormatAppend(buffer,
                   buffer_size,
                   GetArmInstKindName(op->inst_kind),
                   cursor);
      return TRUE;
    case 'a':
      AccessModeAppend(buffer, buffer_size, op->inst_access, cursor);
      return TRUE;
    case 't':
      FormatAppend(buffer,
                   buffer_size,
                   GetArmInstTypeName(op->inst_type),
                   cursor);
      return TRUE;
    case 's':
      FormatAppend(buffer, buffer_size, GetBoolName(op->arm_safe), cursor);
      return TRUE;
    case 'f':
      FormatAppend(buffer, buffer_size, op->describe_format, cursor);
      return TRUE;
    case 'b':
      ValueAppend(buffer, buffer_size, op->num_bytes, cursor);
      return TRUE;
    case 'e':
      InstValuesAppend(buffer,
                       buffer_size,
                       &(op->expected_values),
                       op->inst_type,
                       cursor);
      return TRUE;
    default:
      return FALSE;
  }
}

/* Append text describing the given OpInfo data into the buffer. */
void OpInfoAppend(char* buffer,
                  size_t buffer_size,
                  const OpInfo* op,
                  size_t* cursor) {
  FormatDataAppend(buffer,
                   buffer_size,
                   "%s\t%k\t%t \t%n:\t%f\n\t%a\n\t%e",
                   (void*) op,
                   OpInfoDirective,
                   cursor);
}

Bool DescribeOpInfo(char* buffer, size_t buffer_size, const OpInfo* op) {
  size_t cursor = 0;
  OpInfoAppend(buffer, buffer_size, op, &cursor);
  return cursor < buffer_size;
}

void DecodeValues(uint32_t inst, ArmInstType inst_type,
                  InstValues* values) {
  const InstValues* masks = GetArmInstMasks(inst_type);
  values->cond = GetMaskedValue(inst, masks->cond);
  values->prefix = GetMaskedValue(inst, masks->prefix);
  values->opcode = GetMaskedValue(inst, masks->opcode);
  values->arg1 = GetMaskedValue(inst, masks->arg1);
  values->arg2 = GetMaskedValue(inst, masks->arg2);
  values->arg3 = GetMaskedValue(inst, masks->arg3);
  values->arg4 = GetMaskedValue(inst, masks->arg4);
  values->shift = GetMaskedValue(inst, masks->shift);
  values->immediate = GetMaskedValue(inst, masks->immediate);
}

Bool ValuesDefaultMatch(int32_t actual, int32_t expected, uint32_t mask) {
  switch (expected) {
    case NOT_USED:
      return 0 == actual;
    case ANY:
      return TRUE;
    case NON_ZERO:
      return 0 != actual;
    case CONTAINS_ZERO:
      return 0 != (~actual & mask);
    default:
      return actual == expected;
  }
}

/* Define the sequence of bits that denote register R15. */
#define R15 0xF

Bool Args_1_4_NotEqual(NcDecodedInstruction* inst) {
  return inst->values.arg1 != inst->values.arg4;
}

Bool Arg1IsntR15(NcDecodedInstruction* inst) {
  return R15 != inst->values.arg1;
}

Bool Args_3_4_NotBothZero(NcDecodedInstruction* inst) {
  return inst->values.arg3 != 0x0 ||
      inst->values.arg4 != 0x0;
}

/*
 * Check if the register defined by value is in the set
 * of listed load/store multiple registers.
 */
static Bool ValueInLoadStoreRegisters(NcDecodedInstruction* inst,
                                      int32_t value) {
  return GetBit(inst->values.immediate, value);
}

Bool Arg1NotInLoadStoreRegisters(NcDecodedInstruction* inst) {
  return !ValueInLoadStoreRegisters(inst, inst->values.arg1);
}

Bool R15NotInLoadStoreRegisters(NcDecodedInstruction* inst) {
  return !ValueInLoadStoreRegisters(inst, R15);
}

Bool R15InLoadStoreRegisters(NcDecodedInstruction* inst) {
  return ValueInLoadStoreRegisters(inst, R15);
}
