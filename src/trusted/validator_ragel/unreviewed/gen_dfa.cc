/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

namespace {

const char* short_program_name;
const char* current_dir_name;

const struct option kProgramOptions[] = {
  {"mode",              required_argument,      NULL,   'm'},
  {"disable",           required_argument,      NULL,   'd'},
  {"output",            required_argument,      NULL,   'o'},
  {"const_file",        required_argument,      NULL,   'c'},
  {"help",              no_argument,            NULL,   'h'},
  {"version",           no_argument,            NULL,   'v'},
  {NULL,                0,                      NULL,   0}
};

const char kVersion[] = "2012.09.15";

const char* const kProgramHelp = "Usage: %s [OPTION]... [FILE]...\n"
"\n"
"Creates ragel machine which recognizes instructions listed in given files.\n"
"\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"\n"
"Options list:\n"
"  -m, --mode=mode            CPU mode: ia32 for IA32, amd64 for x86-64\n"
"  -d, --disable=action_list  disable actions from the comma-separated list\n"
"  -o, --output=FILE          write DFA to FILE instead of standard output\n"
"  -c, --const_file=FILE      write consts to FILE instead of standard output\n"
"  -h, --help                 display this help and exit\n"
"  -v, --version              output version information and exit\n"
"\n"
"Here is the full list of possible action types with short description of\n"
"places where they are inserted:\n"
"  vex_prefix          triggered when VEX-encoded instruction is detected\n"
"    @vex_prefix2        inserted after second byte of 3-byte VEX/XOP prefix\n"
"    @vex_prefix3        inserted after third byte of 3-byte VEX/XOP prefix\n"
"    @vex_prefix_short   inserted after second byte of two-byte VEX prefix\n"
"                      Note: there are no 'vex_prefix1' because first byte of\n"
"                        VEX/XOP encoding is multiplexed to 'lds', 'les' or\n"
"                        'pop Eq' instruction\n"
"  instruction_name    triggered when we have processed enough bytes to know \n"
"                        the name of instruction\n"
"    @instruction_NAME   inserted when NAME is known.\n"
"                      Note: if this action is enabled then a set of actions\n"
"                        will be generated, not just a single call of single\n"
"                        action\n"
"  opcode              this will generate opcode actions\n"
"    >opcode_begin       inserted where actual opcode is started (without any\n"
"                        possible prefixes)\n"
"    @opcode_end         inserted where actual opcode is parsed\n"
"                      Note: some instructions use dedicated prefixes (0x66,\n"
"                        0xf2, 0xf3) or immediate byte ('cmpeqpd', 'pf2id',\n"
"                        etc) to distinguish operands, these are not captured\n"
"                        between '>opcode_begin' and '@opcode_end'\n"
"  parse_operands      this will grab instruction operands\n"
"  check_access        this will check memory access\n"
"\n"
"  nacl-forbidden      generate instructions forbidden for nacl\n"
"\n"
"  parse_operands_states     this will grab states of operands (read/write)\n"
"  parse_nonwrite_registers  parse register operands which are only read\n"
"                              or not touched at all\n"
"  parse_immediate_operands  parse immediate operands\n"
"  parse_relative_operands   parse immediate operands (jumps, calls, etc)\n"
"  parse_x87_operands        parse x87 operands\n"
"  parse_mmx_operands        parse MMX operands\n"
"  parse_xmm_operands        parse XMM operands\n"
"  parse_ymm_operands        parse YMM operands\n"
"  parse_operand_positions   produce correct numbers of operands (required\n"
"                              for decoding, not important for validation)\n";

const char* const kVersionHelp = "%s %s\n"
"Copyright (c) 2012 The Native Client Authors. All rights reserved.\n"
"Use of this source code is governed by a BSD-style license that can be\n"
"found in the LICENSE file.\n";

enum Action {
  kVexPrefix,
  kInstructionName,
  kOpcode,
  kParseOperands,
  kCheckAccess,
  kNaClForbidden,
  kParseOperandsStates,
  kParseNonWriteRegisters,
  kParseImmediateOperands,
  kParseRelativeOperands,
  kParseX87Operands,
  kParseMMXOperands,
  kParseXMMOperands,
  kParseYMMOperands,
  kParseOperandPositions
};
const char* kDisablableActionsList[] = {
  "vex_prefix",
  "instruction_name",
  "opcode",
  "parse_operands",
  "check_access",
  "nacl-forbidden",
  "parse_operands_states",
  "parse_nonwrite_registers",
  "parse_immediate_operands",
  "parse_relative_operands",
  "parse_x87_operands",
  "parse_mmx_operands",
  "parse_xmm_operands",
  "parse_ymm_operands",
  "parse_operand_positions"
};
bool disabled_actions[NACL_ARRAY_SIZE(kDisablableActionsList)];
bool enabled(Action action) {
  return !disabled_actions[static_cast<int>(action)];
}

std::map<std::string, size_t> instruction_names;

class Instruction {
 public:
  struct Operand {
    char type;
    std::string size;
    bool enabled;
    bool read;
    bool write;
    bool implicit;
  };

  const std::string& name(void) const {
    return name_;
  }
  Instruction with_name(const std::string& name) const {
    Instruction result = *this;
    result.name_ = name;
    return result;
  }
  const std::vector<Operand>& operands(void) const {
    return operands_;
  }
  Instruction with_operands(const std::vector<Operand>& operands) const {
    Instruction result = *this;
    result.operands_ = operands;
    return result;
  }
  const std::vector<std::string>& opcodes(void) const {
    return opcodes_;
  }
  Instruction with_opcodes(const std::vector<std::string>& opcodes) const {
    Instruction result = *this;
    result.opcodes_ = opcodes;
    return result;
  }
  const std::set<std::string>& flags(void) const {
    return flags_;
  }
  Instruction with_flags(const std::set<std::string>& flags) const {
    Instruction result = *this;
    result.flags_ = flags;
    return result;
  }
  bool has_flag(const std::string& flag) const {
    return flags_.find(flag) != flags_.end();
  }

 protected:
  std::string name_;
  std::vector<Operand> operands_;
  std::vector<std::string> opcodes_;
  std::set<std::string> flags_;

 private:
  static void check_flag_valid(const std::string& flag) {
    static const char* all_instruction_flags[] = {
      /* Parsing flags. */
      "branch_hint",
      "condrep",
      "lock",
      "no_memory_access",
      "norex",
      "norexw",
      "rep",

      /* CPUID flags. */
      "CPUFeature_3DNOW",
      "CPUFeature_3DPRFTCH",
      "CPUFeature_AES",
      "CPUFeature_AESAVX",
      "CPUFeature_ALTMOVCR8",
      "CPUFeature_AVX",
      "CPUFeature_BMI1",
      "CPUFeature_CLFLUSH",
      "CPUFeature_CLMUL",
      "CPUFeature_CLMULAVX",
      "CPUFeature_CMOV",
      "CPUFeature_CMOVx87",
      "CPUFeature_CX16",
      "CPUFeature_CX8",
      "CPUFeature_E3DNOW",
      "CPUFeature_EMMX",
      "CPUFeature_EMMXSSE",
      "CPUFeature_F16C",
      "CPUFeature_FMA",
      "CPUFeature_FMA4",
      "CPUFeature_FXSR",
      "CPUFeature_LAHF",
      "CPUFeature_LWP",
      "CPUFeature_LZCNT",
      "CPUFeature_MMX",
      "CPUFeature_MON",
      "CPUFeature_MOVBE",
      "CPUFeature_MSR",
      "CPUFeature_POPCNT",
      "CPUFeature_SEP",
      "CPUFeature_SFENCE",
      "CPUFeature_SKINIT",
      "CPUFeature_SSE",
      "CPUFeature_SSE2",
      "CPUFeature_SSE3",
      "CPUFeature_SSE41",
      "CPUFeature_SSE42",
      "CPUFeature_SSE4A",
      "CPUFeature_SSSE3",
      "CPUFeature_SVM",
      "CPUFeature_SYSCALL",
      "CPUFeature_TBM",
      "CPUFeature_TSC",
      "CPUFeature_TSCP",
      "CPUFeature_TZCNT",
      "CPUFeature_x87",
      "CPUFeature_XOP",

       /* Flags for enabling/disabling based on architecture and validity.  */
       "ia32",
       "amd64",
       "nacl-ia32-forbidden",
       "nacl-amd64-forbidden",
       "nacl-forbidden",
       "nacl-amd64-zero-extends",
       "nacl-amd64-modifiable"
    };
    if (find_if(
          all_instruction_flags,
          all_instruction_flags + NACL_ARRAY_SIZE(all_instruction_flags),
          bind1st(std::equal_to<std::string>(), flag)) ==
          all_instruction_flags + NACL_ARRAY_SIZE(all_instruction_flags)) {
      fprintf(stderr, "%s: unknown flag: '%s'\n", short_program_name,
              flag.c_str());
      exit(1);
    }
  }

  void add_flag(const std::string& flag) {
    check_flag_valid(flag);
    flags_.insert(flag);
  }

  friend void parse_instructions(const char*);
};
std::vector<Instruction> instructions;

FILE* out_file = stdout;
FILE* const_file = stdout;
const char* out_file_name = NULL;
const char* const_file_name = NULL;

bool ia32_mode = true;

/* read_file reads the filename and returns it's contents.  We don't try to
   save memory by using line-by-line processing.  */
std::string read_file(const char* filename) {
  std::string file_content;
  int file = open(filename, O_RDONLY);
  char buf[1024];
  ssize_t count;
  bool mac_eol_found = false;

  if (file == -1) {
    fprintf(stderr, "%s: can not open '%s' file (%s)\n",
            short_program_name, filename, strerror(errno));
    exit(1);
  }
  while ((count = read(file, buf, sizeof buf)) > 0)
    for (char* it = buf; it < buf + count ; ++it)
      if (*it == '\r') {
        /* This may be MacOS EOL, or Windows one.  Assume MacOS for now.  */
        if (mac_eol_found)
          file_content.push_back('\n');
        else
          mac_eol_found = true;
      } else if (*it == '\n') {
        /* '\r\n' is Windows EOL, not Mac's one.  */
        mac_eol_found = false;
        file_content.push_back('\n');
      } else {
        if (mac_eol_found) {
          file_content.push_back('\n');
          mac_eol_found = false;
        }
        file_content.push_back(*it);
      }
  if (mac_eol_found)
    file_content.push_back('\n');
  if (count == -1) {
    fprintf(stderr, "%s: can not read '%s' file (%s)\n",
            short_program_name, filename, strerror(errno));
    exit(1);
  }
  if (close(file)) {
    fprintf(stderr, "%s: can not close '%s' file (%s)\n",
            short_program_name, filename, strerror(errno));
    exit(1);
  }
  return file_content;
}

/* Advances the iterator till the next comma or line end, splits the traversed
 * text by whitespace and returns it.  Respects quoted text.
 */
std::vector<std::string> split_till_comma(
    std::string::const_iterator* it_out,
    const std::string::const_iterator& line_end) {
  std::vector<std::string> result;
  std::string::const_iterator& it = *it_out;
  std::string part;

  for (; it != line_end; ++it) {
    if (*it == ' ' || *it == '\t') {
      if (!part.empty()) {
        result.push_back(part);
        part.clear();
      }
      continue;
    }
    if (*it == ',') {
      break;
    }
    if (*it != '"') {
      part.push_back(*it);
      continue;
    }

    for (++it; *it != '"'; ++it) {
      if (it == line_end) {
        fprintf(stderr, "%s: quoted text reaches end of line: %s\n",
                short_program_name, part.c_str());
        exit(1);
      }
      part.push_back(*it);
    }
  }
  if (!part.empty())
    result.push_back(part);
  return result;
}

/* find_end_of_line looks for the end of line or the end of file (whichever
   comes first).  */
std::string::const_iterator find_end_of_line(std::string::const_iterator it,
                                             std::string::const_iterator eof) {
  while (it != eof && *it != '\n')
    ++it;
  return it;
}

/* is_supported_instruction returns true if instruction is supported in a
   current mode (some instructions are only supported in ia32 mode or x86-64
   mode, some are excluded from validator's reduced DFA).  */
bool is_supported_instruction(const Instruction& instruction) {
  return ((!instruction.has_flag("ia32") || ia32_mode) &&
          (!instruction.has_flag("amd64") || !ia32_mode) &&
          (!instruction.has_flag("nacl-ia32-forbidden") || !ia32_mode ||
           enabled(kNaClForbidden)) &&
          (!instruction.has_flag("nacl-amd64-forbidden") || ia32_mode ||
           enabled(kNaClForbidden)) &&
          (!instruction.has_flag("nacl-forbidden") || enabled(kNaClForbidden)));
}

/* parse_instructions parses the given *.def file and adds instruction
   definitions to the instructions vector and their names to instruction_names
   map.

   Note: it only accepts flags listed in check_flag_valid.  */
void parse_instructions(const char* filename) {
  const std::string file_content = read_file(filename);
  std::string::const_iterator it = file_content.begin();
  std::string::const_iterator eof = file_content.end();

  while (it != eof) {
    std::string::const_iterator line_end = find_end_of_line(it, eof);
    if (it == line_end) {
      ++it;
      continue;
    }
    /* If line starts with '#' then it's a comment. */
    if (*it == '#') {
      it = line_end;
      continue;
    }
    Instruction instruction;
    std::vector<std::string> operation = split_till_comma(&it, line_end);
    /* Line with just whitespaces is ignored.  */
    if (operation.size() == 0)
      continue;
    /* First word is operation name, other words are operands.  */
    for (std::vector<std::string>::reverse_iterator string = operation.rbegin();
         string != operation.rend() - 1; ++string) {
      Instruction::Operand operand;
      operand.enabled  = true;
      operand.implicit = false;
      /* Most times we can determine whether operand is used for reading or
       * writing using its position and number of operands (default case in this
       * switch).  In a few exceptions we add read/write annotations that
       * override default behavior. All such annotations are created by hand. */
      switch (string->at(0)) {
        case '\'':
          operand.type  = string->at(1);
          operand.size  = string->substr(2);
          operand.read  = false;
          operand.write = false;
          break;
        case '=':
          operand.type  = string->at(1);
          operand.size  = string->substr(2);
          operand.read  = true;
          operand.write = false;
          break;
        case '!':
          operand.type  = string->at(1);
          operand.size  = string->substr(2);
          operand.read  = false;
          operand.write = true;
          break;
        case '&':
          operand.type  = string->at(1);
          operand.size  = string->substr(2);
          operand.read  = true;
          operand.write = true;
          break;
        default:
          operand.type  = string->at(0);
          operand.size  = string->substr(1);
          if (string == operation.rbegin()) {
            if (operation.size() <= 3) {
              operand.read  = true;
              operand.write = true;
            } else {
              operand.read  = false;
              operand.write = true;
            }
          } else {
            operand.read  = true;
            operand.write = false;
          }
          break;
      }
      if (*(operand.size.rbegin()) == '*') {
        operand.size.resize(operand.size.length() - 1);
        operand.implicit = true;
      }
      instruction.operands_.push_back(operand);
    }
    bool enabled_instruction = true;
    if (*it == ',') {
      ++it;
      if (it == line_end) line_end = find_end_of_line(++it, eof);
      instruction.opcodes_ = split_till_comma(&it, line_end);
      if (*it == ',') {
        ++it;
        if (it == line_end) line_end = find_end_of_line(++it, eof);
        const std::vector<std::string>& flags = split_till_comma(&it, line_end);
        for (std::vector<std::string>::const_iterator flag = flags.begin();
             flag != flags.end(); ++flag)
          instruction.add_flag(*flag);
        if (!is_supported_instruction(instruction))
          enabled_instruction = false;
      }
    }
    if (enabled_instruction) {
      instruction.name_ = operation[0];
      instruction_names[instruction.name_] = 0;
      instructions.push_back(instruction);
    }
    if (find_end_of_line(it, eof) != it) {
      fprintf(stderr, "%s: definition files must have three columns",
              short_program_name);
      exit(1);
    }
  }
}

/* print_consts does three things:
    • prints instruction_names array in -consts.c (if needed).
    • adjusts offsets in instruction_names map.
    • prints index_registers array in -consts.c (if needed).  */
void print_consts(void) {
  if (enabled(kInstructionName)) {
    for (std::map<std::string, size_t>::iterator instruction_name =
           instruction_names.begin();
         instruction_name != instruction_names.end(); ++instruction_name)
      if (instruction_name->second == 0)
        for (size_t p = 1; p < instruction_name->first.length(); ++p) {
          const std::map<std::string, size_t>::iterator it =
            instruction_names.find(std::string(instruction_name->first, p));
          if (it != instruction_names.end())
            it->second = 1;
        }
    size_t offset = 0;
    const char* delimiter = "static const char instruction_names[] = {\n  ";
    for (std::map<std::string, size_t>::iterator instruction_name =
           instruction_names.begin();
         instruction_name != instruction_names.end(); ++instruction_name)
      if (instruction_name->second != 1) {
        fprintf(const_file, "%s", delimiter);
        for (std::string::const_iterator c = instruction_name->first.begin();
             c != instruction_name->first.end(); ++c)
          fprintf(const_file, "0x%02x, ", static_cast<int>(*c));
        fprintf(const_file, "\'\\0\',  /* ");
        fprintf(const_file, "%s", instruction_name->first.c_str());
        delimiter = " */\n  ";
        instruction_name->second = offset;
        offset += instruction_name->first.length() + 1;
      }
    for (std::map<std::string, size_t>::iterator instruction_name =
           instruction_names.begin();
         instruction_name != instruction_names.end(); ++instruction_name) {
      size_t offset = instruction_name->second;
      if (offset != 1)
        for (size_t p = 1; p < instruction_name->first.length(); ++p) {
          const std::map<std::string, size_t>::iterator it =
            instruction_names.find(std::string(instruction_name->first, p));
          if ((it != instruction_names.end()) && (it->second == 1))
            it->second = offset + p;
        }
    }
    fprintf(const_file, " */\n};\n");
  }
  if (enabled(kParseOperands)) {
    fprintf(const_file, "static const uint8_t index_registers[] = {\n"
"  REG_RAX, REG_RCX, REG_RDX, REG_RBX,\n"
"  REG_RIZ, REG_RBP, REG_RSI, REG_RDI,\n"
"  REG_R8,  REG_R9,  REG_R10, REG_R11,\n"
"  REG_R12, REG_R13, REG_R14, REG_R15\n"
"};\n"
"");
  }
}

/* c_identifier generates action name by replacing all characters except
   letters and numbers with underscores.  This may lead to the collisions,
   but these will be detected by ragel thus they can not lead to runtime
   errors.  */
std::string c_identifier(std::string text) {
  std::string name;
  for (std::string::const_iterator c = text.begin(); c != text.end(); ++c)
    if (('A' <= *c && *c <= 'Z') || ('a' <= *c && *c <= 'z') ||
        ('0' <= *c && *c <= '9'))
      name.push_back(*c);
    else
      name.push_back('_');
  return name;
}

/* print_name_actions prints set of actions which save name of the instruction
   in instruction_name variable.  It uses instruction_names array generated by
   print_consts function.  */
void print_name_actions(void) {
  for (std::map<std::string, size_t>::const_iterator pair =
         instruction_names.begin(); pair != instruction_names.end(); ++pair)
    fprintf(out_file, "  action instruction_%s"
            " { SET_INSTRUCTION_NAME(instruction_names + %" NACL_PRIuS "); }\n",
            c_identifier(pair->first).c_str(), pair->second);
}

class MarkedInstruction : public Instruction {
 public:
  /* Describes REX.B, REX.X, REX.R, and REX.W bits in REX/VEX instruction prefix
     (see AMD/Intel documentation for details for when these bits can/should be
     allowed and when they are correct/incorrect).  */
  struct RexType {
    bool b_ : 1;
    bool x_ : 1;
    bool r_ : 1;
    bool w_ : 1;
    RexType(bool b, bool x, bool r, bool w) : b_(b), x_(x), r_(r), w_(w) { }
  };

  explicit MarkedInstruction(const Instruction& instruction_) :
      Instruction(instruction_),
      required_prefixes_(),
      optional_prefixes_(),
      rex_(false, false, false, false),
      opcode_in_modrm_(false),
      opcode_in_imm_(false),
      fwait_(false) {
    if (has_flag("branch_hint")) optional_prefixes_.insert("branch_hint");
    if (has_flag("condrep")) optional_prefixes_.insert("condrep");
    if (has_flag("rep")) optional_prefixes_.insert("rep");
    for (std::vector<std::string>::const_iterator opcode = opcodes_.begin();
         opcode != opcodes_.end(); ++opcode)
      if (*opcode == "/") {
        opcode_in_imm_ = true;
        break;
      } else if (opcode->find('/') != opcode->npos) {
        opcode_in_modrm_ = true;
        break;
      }
    /* If register is stored in opcode we need to expand opcode now.  */
    for (std::vector<Operand>::const_iterator operand = operands_.begin();
         operand != operands_.end(); ++operand)
      if (operand->type == 'r') {
        std::vector<std::string>::reverse_iterator opcode = opcodes_.rbegin();
        for (; opcode != opcodes_.rend(); ++opcode) {
          if (opcode->find('/') == opcode->npos) {
          std::string saved_opcode = *opcode;
            switch (*(opcode->rbegin())) {
              case '0':
                (*opcode) = "(";
                opcode->append(saved_opcode);
                for (char c = '1'; c <= '7'; ++c) {
                  opcode->push_back('|');
                  (*(saved_opcode.rbegin())) = c;
                  opcode->append(saved_opcode);
                }
                opcode->push_back(')');
                break;
              case '8':
                (*opcode) = "(";
                opcode->append(saved_opcode);
                static const char cc[] = {'9', 'a', 'b', 'c', 'd', 'e', 'f'};
                for (size_t c = 0; c < NACL_ARRAY_SIZE(cc); ++c) {
                  opcode->push_back('|');
                  (*(saved_opcode.rbegin())) = cc[c];
                  opcode->append(saved_opcode);
                }
                opcode->push_back(')');
                break;
              default:
                fprintf(stderr, "%s: error - can not use 'r' operand in "
                  "instruction '%s'", short_program_name, name_.c_str());
                exit(1);
            }
            /* x87 and MMX registers are not extended in x86-64 mode: there are
               only 8 of them.  But only x87 operands use opcode to specify
               register number.  */
            if (operand->size != "7") rex_.b_ = true;
            break;
          }
        }
        if (opcode == opcodes_.rend()) {
          fprintf(stderr, "%s: error - can not use 'r' operand in "
            "instruction '%s'", short_program_name, name_.c_str());
          exit(1);
        }
        break;
      }
    /* Some 'opcodes' include prefixes, move them there.  */
    for (;;) {
      if ((*opcodes_.begin()) == "rexw") {
        rex_.w_ = true;
      } else if ((*opcodes_.begin()) == "fwait") {
        fwait_ = true;
      } else {
        static const char* prefix_bytes[] = {
          "0x66", "data16",
          "0xf0", "lock",
          "0xf2", "repnz",
          "0xf3", "repz",
        };
        if (find_if(prefix_bytes, prefix_bytes + NACL_ARRAY_SIZE(prefix_bytes),
                    bind1st(std::equal_to<std::string>(), *opcodes_.begin())) ==
                                   prefix_bytes + NACL_ARRAY_SIZE(prefix_bytes))
          break;
        required_prefixes_.insert(*opcodes_.begin());
      }
      opcodes_.erase(opcodes_.begin());
    }
  }
  MarkedInstruction(const Instruction& instruction,
                    const std::multiset<std::string>& required_prefixes,
                    const std::multiset<std::string>& optional_prefixes,
                    const RexType& rex,
                    bool opcode_in_modrm,
                    bool opcode_in_imm,
                    bool fwait) :
    Instruction(instruction),
    required_prefixes_(required_prefixes),
    optional_prefixes_(optional_prefixes),
    rex_(rex),
    opcode_in_modrm_(opcode_in_modrm),
    opcode_in_imm_(opcode_in_imm),
    fwait_(fwait) {
  }
  MarkedInstruction with_name(const std::string& name) const {
    return MarkedInstruction(Instruction::with_name(name),
                             required_prefixes_,
                             optional_prefixes_,
                             rex_,
                             opcode_in_modrm_,
                             opcode_in_imm_,
                             fwait_);
  }
  MarkedInstruction with_operands(
      const std::vector<Operand>& operands) const {
    return MarkedInstruction(Instruction::with_operands(operands),
                             required_prefixes_,
                             optional_prefixes_,
                             rex_,
                             opcode_in_modrm_,
                             opcode_in_imm_,
                             fwait_);
  }
  MarkedInstruction with_opcodes(
      const std::vector<std::string>& opcodes) const {
    return MarkedInstruction(Instruction::with_opcodes(opcodes),
                             required_prefixes_,
                             optional_prefixes_,
                             rex_,
                             opcode_in_modrm_,
                             opcode_in_imm_,
                             fwait_);
  }
  MarkedInstruction with_flags(const std::set<std::string>& flags) const {
    return MarkedInstruction(Instruction::with_flags(flags),
                             required_prefixes_,
                             optional_prefixes_,
                             rex_,
                             opcode_in_modrm_,
                             opcode_in_imm_,
                             fwait_);
  }
  const std::multiset<std::string>& required_prefixes(void) const {
    return required_prefixes_;
  }
  MarkedInstruction add_requred_prefix(const std::string& string) const {
    MarkedInstruction result = *this;
    result.required_prefixes_.insert(string);
    if (string == "data16")
      for (std::vector<Operand>::iterator operand = result.operands_.begin();
           operand != result.operands_.end(); ++operand)
        if (operand->size == "v" || operand->size == "z")
          operand->size = "w";
        else if (operand->size == "y")
          operand->size = "d";
    return result;
  }
  const std::multiset<std::string>& optional_prefixes(void) const {
    return optional_prefixes_;
  }
  MarkedInstruction add_optional_prefix(const std::string& string) const {
    MarkedInstruction result = *this;
    result.optional_prefixes_.insert(string);
    return result;
  }
  bool rex_b(void) const {
    return rex_.b_;
  }
  MarkedInstruction set_rex_b(void) const {
    MarkedInstruction result = *this;
    result.rex_.b_ = true;
    return result;
  }
  bool rex_x(void) const {
    return rex_.x_;
  }
  MarkedInstruction set_rex_x(void) const {
    MarkedInstruction result = *this;
    result.rex_.x_ = true;
    return result;
  }
  bool rex_r(void) const {
    return rex_.r_;
  }
  MarkedInstruction set_rex_r(void) const {
    MarkedInstruction result = *this;
    result.rex_.r_ = true;
    return result;
  }
  bool rex_w(void) const {
    return rex_.w_;
  }
  MarkedInstruction set_rex_w(void) const {
    MarkedInstruction result = *this;
    result.rex_.w_ = true;
    for (std::vector<Operand>::iterator operand = result.operands_.begin();
         operand != result.operands_.end(); ++operand)
      if (operand->size == "v" || operand->size == "y")
        operand->size = "q";
      else if (operand->size == "z")
        operand->size = "d";
    return result;
  }
  MarkedInstruction clear_rex_w(void) const {
    MarkedInstruction result = *this;
    result.rex_.w_ = false;
    for (std::vector<Operand>::iterator operand = result.operands_.begin();
         operand != result.operands_.end(); ++operand)
      if (operand->size == "v" || operand->size == "y" || operand->size == "z")
        operand->size = "d";
    return result;
  }
  MarkedInstruction set_opcode_in_modrm(void) const {
    MarkedInstruction result = *this;
    result.opcode_in_modrm_ = true;
    return result;
  }
  bool opcode_in_modrm(void) const {
    return opcode_in_modrm_;
  }
  bool opcode_in_imm(void) const {
    return opcode_in_imm_;
  }
  bool fwait(void) const {
    return fwait_;
  }

 private:
  std::multiset<std::string> required_prefixes_;
  std::multiset<std::string> optional_prefixes_;
  RexType rex_;
  bool opcode_in_modrm_ : 1;
  bool opcode_in_imm_ : 1;
  bool fwait_ : 1;
};

/* mod_reg_is_used returns true if the instruction includes operands which is
   encoded in field “reg” in “ModR/M byte” and must be extended by “R” bit in
   REX/VEX prefix.  */
bool mod_reg_is_used(const MarkedInstruction& instruction) {
  const std::vector<MarkedInstruction::Operand>& operands =
    instruction.operands();
  for (std::vector<MarkedInstruction::Operand>::const_iterator
         operand = operands.begin(); operand != operands.end(); ++operand) {
    const char source = operand->type;
    const std::multiset<std::string>& prefixes =
      instruction.required_prefixes();
    /* Control registers can use 0xf0 prefix to select %cr8…%cr15.  */
    if ((source == 'C' && prefixes.find("0xf0") == prefixes.end()) ||
        source == 'G' || source == 'P' || source == 'V')
      return true;
  }
  return false;
}

/* memory_capable_operand returns true if “source” uses field “r/m” in
 * “ModR/M byte” and must be extended by “X” and/or “B” bits in REX/VEX prefix.
 */
bool memory_capable_operand(char source) {
  if (source == 'E' || source == 'M' || source == 'N' ||
      source == 'Q' || source == 'R' || source == 'U' || source == 'W')
    return true;
  else
    return false;
}

/* mod_rm_is_used returns true if the instruction includes operands which is
   encoded in field “r/m” in “ModR/M byte” and must be extended by “X” and/or
   “B” bits in REX/VEX prefix.  */
bool mod_rm_is_used(const MarkedInstruction& instruction) {
  const std::vector<MarkedInstruction::Operand>& operands =
    instruction.operands();
  for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
         operands.begin(); operand != operands.end(); ++operand) {
    const char source = operand->type;
    if (memory_capable_operand(source))
      return true;
  }
  return false;
}

bool first_delimiter = true;
/* print_operator_delimiter is a simple function: it starts a “ragel line”.

   When it's called for a first time it just starts a line, in all other
   cases it first finishes the previous line with “(”.  */
void print_operator_delimiter(void) {
  if (first_delimiter)
    fprintf(out_file, "\n    (");
  else
    fprintf(out_file, ") |\n    (");
  first_delimiter = false;
}

/* print_legacy_prefixes prints all possible combinations of legacy prefixes
   from required_prefixes and optional_prefixes sets passed to this function
   in the instruction argument.

   Right now we print all possible permutations of required_prefixes with
   all optional_prefixes in all positions but without repetitions.

   Later we may decide to implement other cases (such as: duplicated
   prefixes for decoder and/or prefixes in a single “preferred order”
   for strict validator).  */
void print_legacy_prefixes(const MarkedInstruction& instruction) {
  const std::multiset<std::string>& required_prefixes =
    instruction.required_prefixes();
  if (instruction.fwait()) {
    if (ia32_mode)
      fprintf(out_file, "0x9b ");
    else if ((required_prefixes.size() == 1) &&
             (*required_prefixes.begin()) == "data16")
      fprintf(out_file, "(REX_WRXB? 0x9b data16 | 0x9b ");
    else
      fprintf(out_file, "(REX_WRXB? 0x9b | 0x9b ");
  }
  const std::multiset<std::string>& optional_prefixes =
    instruction.optional_prefixes();
  if ((required_prefixes.size() == 1) &&
      (optional_prefixes.size() == 0)) {
    fprintf(out_file, "%s ", required_prefixes.begin()->c_str());
  } else if ((required_prefixes.size() == 0) &&
             (optional_prefixes.size() == 1)) {
    fprintf(out_file, "%s? ", optional_prefixes.begin()->c_str());
  } else if ((optional_prefixes.size() > 0) ||
             (required_prefixes.size() > 0)) {
    const char* delimiter = "(";
    size_t opt_start = required_prefixes.size() ? 0 : 1;
    size_t opt_end = 1 << optional_prefixes.size();
    for (size_t opt = opt_start; opt < opt_end; ++opt) {
      std::multiset<std::string> prefixes = required_prefixes;
      std::multiset<std::string>::const_iterator it = optional_prefixes.begin();
      for (size_t id = 1; id < opt_end; id <<= 1, ++it) {
        if (opt & id) {
          prefixes.insert(*it);
        }
      }
      if (prefixes.size() == 1) {
        fprintf(out_file, "%s%s", delimiter, prefixes.begin()->c_str());
        delimiter = " | ";
      } else {
        std::vector<std::string> permutations(prefixes.begin(), prefixes.end());
        do {
          fprintf(out_file, "%s", delimiter);
          delimiter = " | ";
          char delimiter = '(';
          for (std::vector<std::string>::const_iterator prefix =
                 permutations.begin(); prefix != permutations.end(); ++prefix) {
            fprintf(out_file, "%c%s", delimiter, prefix->c_str());
            delimiter = ' ';
          }
          fprintf(out_file, ")");
        } while (next_permutation(permutations.begin(), permutations.end()));
      }
    }
    if (opt_start)
      fprintf(out_file, ")? ");
    else
      fprintf(out_file, ") ");
  }
}

/* print_rex_prefix prints machine that accepts all permitted REX prefixes
   for the instruction.

   There are two complications:
    • ia32 mode does not support REX prefix.
    • REX prefix is incompatible with VEX/XOP prefixes.  */
void print_rex_prefix(const MarkedInstruction& instruction) {
  /* Prefix REX is not used in ia32 mode.  */
  if (ia32_mode || instruction.has_flag("norex"))
    return;
  const std::vector<std::string>& opcodes = instruction.opcodes();
  /* VEX/XOP instructions integrate REX bits and opcode bits.  They will
     be printed in print_opcode_nomodrm.  */
  if ((opcodes.size() >= 3) &&
      ((opcodes[0] == "0xc4") ||
       ((opcodes[0] == "0x8f") && (opcodes[1] != "/0"))))
    return;
  /* Allow any bits in  rex prefix for the compatibility. See
     http://code.google.com/p/nativeclient/issues/detail?id=2517 */
  if (instruction.rex_w())
    fprintf(out_file, "REXW_RXB");
  else
    fprintf(out_file, "REX_RXB?");
  if (instruction.fwait())
    fprintf(out_file, ")");
  fprintf(out_file, " ");
}

void print_third_byte_of_vex(const MarkedInstruction& instruction,
                             const std::string& third_byte) {
  fprintf(out_file, " b_");
  for (std::string::const_iterator c = third_byte.begin();
       c != third_byte.end(); ++c)
    if (*c == 'W')
      if (instruction.rex_w())
        fprintf(out_file, "1");
      else
        fprintf(out_file, "0");
    else if (*c == '.')
      fprintf(out_file, "_");
    else if (*c == 'x')
      fprintf(out_file, "0");
    else
      fprintf(out_file, "%c", *c);
}

/* print_vex_opcode prints prints machine that accepts the main opcode of the
   VEX/XOP instruction.

   There are quite a few twists here:
    • VEX can use two-byte 0xc4 VEX prefix and two-byte 0xc5 VEX prefix
    • VEX prefix combines opcode selection table with RXB VEX bits
    • one register can be embedded in a VEX prefix, too   */
void print_vex_opcode(const MarkedInstruction& instruction) {
  const std::vector<std::string>& opcodes = instruction.opcodes();
  bool c5_ok = (opcodes[0] == "0xc4") &&
               ((opcodes[1] == "RXB.01") ||
                (opcodes[1] == "RXB.00001")) &&
               ((*opcodes[2].begin() == '0') ||
                (*opcodes[2].begin() == 'x') ||
                (*opcodes[2].begin() == 'X'));
  if (c5_ok) fprintf(out_file, "((");
  bool rex_b = instruction.rex_b();
  bool rex_x = instruction.rex_x();
  bool rex_r = instruction.rex_r();
  fprintf(out_file, "(%s (VEX_", opcodes[0].c_str());
  if (ia32_mode || (!rex_r && !rex_x && !rex_b)) {
    fprintf(out_file, "NONE");
  } else {
    if (rex_r) fprintf(out_file, "R");
    if (rex_x) fprintf(out_file, "X");
    if (rex_b) fprintf(out_file, "B");
  }
  fprintf(out_file, " & VEX_map%s) ", opcodes[1].c_str() + 4);
  std::string third_byte = opcodes[2];
  static const char* symbolic_names[] = { "cntl", "dest", "src1", "src" };
  for (size_t name = 0; name < NACL_ARRAY_SIZE(symbolic_names); ++name)
    for (std::string::iterator it = third_byte.begin(); it != third_byte.end();
         ++it)
      if (static_cast<size_t>(third_byte.end() - it) >=
                                                 strlen(symbolic_names[name]) &&
          strncmp(&*it, symbolic_names[name], strlen(symbolic_names[name]))
                                                                         == 0) {
        third_byte.replace(it, it + strlen(symbolic_names[name]), "XXXX");
        break;
      }
  static const char* third_byte_check[] = {
    "01xXW", ".", "01xX", "01xX", "01xX", "01xX", ".", "01xX", ".", "01", "01"
  };
  bool third_byte_ok = NACL_ARRAY_SIZE(third_byte_check) == third_byte.length();
  if (third_byte_ok) {
    for (size_t set = 0; set < NACL_ARRAY_SIZE(third_byte_check); ++set) {
      if (!strchr(third_byte_check[set], third_byte[set])) {
        third_byte_ok = false;
        break;
      }
    }
    if (third_byte_ok) {
      if (ia32_mode && third_byte[2] == 'X')
        third_byte[2] = '1';
      print_third_byte_of_vex(instruction, third_byte);
      if (enabled(kVexPrefix))
        fprintf(out_file, " @vex_prefix3");
      if (c5_ok) {
        fprintf(out_file, ") | (0xc5 ");
        if (!ia32_mode && rex_r)
          third_byte[0] = 'X';
        else
          third_byte[0] = '1';
        print_third_byte_of_vex(instruction, third_byte);
        if (enabled(kVexPrefix))
          fprintf(out_file, " @vex_prefix_short");
        fprintf(out_file, "))");
      }
      for (std::vector<std::string>::const_iterator opcode =
             opcodes.begin() + 3; opcode != opcodes.end(); ++opcode)
        if (opcode->find('/') == opcode->npos)
          fprintf(out_file, " %s", opcode->c_str());
        else
          break;
      fprintf(out_file, ")");
    } else {
      fprintf(stderr, "%s: error - third byte of VEX/XOP command "
                      "is unparseable (%s) in instruction “%s”",
              short_program_name, third_byte.c_str(),
              instruction.name().c_str());
      exit(1);
    }
  } else {
    fprintf(stderr, "%s: error - third byte of VEX/XOP command "
                    "is too long (%s) in instruction “%s”",
            short_program_name, third_byte.c_str(),
            instruction.name().c_str());
    exit(1);
  }
}

/* print_opcode_nomodrm prints prints machine that accepts the main opcode of
   the instruction.

   There are quite a few twists here:
    • opcode may include register (e.g. “push” and “pop” instructions)
    • VEX/XOP mix together opcode and prefix (see print_vex_opcode)
    • opcode can be embedded in “ModR/M byte” and “imm” parts of the
      instruction.  These are NOT printed here.  */
void print_opcode_nomodrm(const MarkedInstruction& instruction) {
  const std::vector<std::string>& opcodes = instruction.opcodes();
  if ((opcodes.size() == 1) ||
      ((opcodes.size() == 2) &&
       (opcodes[1].find('/') != opcodes[1].npos))) {
    if (opcodes[0].find('/') == opcodes[0].npos)
      fprintf(out_file, "%s", opcodes[0].c_str());
  } else if ((opcodes.size() >= 3) &&
             ((opcodes[0] == "0xc4") ||
              ((opcodes[0] == "0x8f") && (opcodes[1] != "/0"))) &&
             (opcodes[1].substr(0, 4) == "RXB.")) {
    print_vex_opcode(instruction);
  } else {
    char delimiter = '(';
    for (std::vector<std::string>::const_iterator opcode = opcodes.begin();
         opcode != opcodes.end(); ++opcode)
      if (opcode->find('/') == opcode->npos) {
        fprintf(out_file, "%c%s", delimiter, opcode->c_str());
        delimiter = ' ';
      } else {
        break;
      }
    fprintf(out_file, ")");
  }
  if (enabled(kOpcode))
    fprintf(out_file, " >begin_opcode");
  const std::vector<MarkedInstruction::Operand>& operands =
    instruction.operands();
  if (enabled(kParseOperands)) {
    size_t operand_index = 0;
    for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
           operands.begin(); operand != operands.end(); ++operand) {
      if (operand->enabled && operand->type == 'r') {
        if (operand->size == "x87")
          fprintf(out_file, " @operand%" NACL_PRIuS "_from_opcode_x87",
                  operand_index);
        else
          fprintf(out_file, " @operand%" NACL_PRIuS "_from_opcode",
                  operand_index);
      }
      if (operand->enabled || enabled(kParseOperandPositions))
        ++operand_index;
    }
  }
}

/* print_immediate_opcode is only used when opcode is embedded in “imm”
   field.

   Prints machine for this last part of the opcode.  */
void print_immediate_opcode(const MarkedInstruction& instruction) {
  bool print_opcode = false;
  const std::vector<std::string>& opcodes = instruction.opcodes();
  for (std::vector<std::string>::const_iterator opcode = opcodes.begin();
       opcode != opcodes.end(); ++opcode)
    if (*opcode == "/")
      print_opcode = true;
    else if (print_opcode)
      fprintf(out_file, " %s", opcode->c_str());
}

/* print_opcode_recognition prints appropriate actions for the case where
   opcode is recognized.

   This may happen in three positions:
    • after “official opcode bytes” - normal case.
    • after “ModR/M byte”.
    • after “imm byte”.

   In all cases we need to store information about the detected instruction:
    • name of the instruction.
    • number of operands.
    • sizes of the operands.
    • recognition of implied operands (e.g. %rax or %ds:(%rsi).  */
void print_opcode_recognition(const MarkedInstruction& instruction,
                              bool memory_access) {
  const std::vector<std::string>& opcodes = instruction.opcodes();
  if (instruction.opcode_in_modrm()) {
    fprintf(out_file, " (");
    for (std::vector<std::string>::const_reverse_iterator opcode =
           opcodes.rbegin(); opcode != opcodes.rend(); ++opcode)
      if ((*opcode) != "/" && opcode->find('/') != opcode->npos)
        fprintf(out_file, "opcode_%s", opcode->c_str() + 1);
  }
  if (enabled(kOpcode))
    fprintf(out_file, " @end_opcode");
  const std::multiset<std::string>& required_prefixes =
    instruction.required_prefixes();
  for (std::multiset<std::string>::const_iterator required_prefix =
         required_prefixes.begin(); required_prefix != required_prefixes.end();
         ++required_prefix)
    if (*required_prefix == "0x66") {
      fprintf(out_file, " @not_data16_prefix");
      break;
    }
  for (std::multiset<std::string>::const_iterator required_prefix =
         required_prefixes.begin(); required_prefix != required_prefixes.end();
         ++required_prefix)
    if (*required_prefix == "0xf2") {
      fprintf(out_file, " @not_repnz_prefix");
      break;
    }
  for (std::multiset<std::string>::const_iterator required_prefix =
         required_prefixes.begin(); required_prefix != required_prefixes.end();
         ++required_prefix)
    if (*required_prefix == "0xf3") {
      fprintf(out_file, " @not_repz_prefix");
      break;
    }
  if (enabled(kInstructionName))
    fprintf(out_file, " @instruction_%s",
            c_identifier(instruction.name()).c_str());
  else if (instruction.opcode_in_imm())
    fprintf(out_file, " @last_byte_is_not_immediate");
  else if (instruction.has_flag("nacl-amd64-modifiable") &&
           enabled(kParseOperands) &&
           !enabled(kParseOperandPositions))
    fprintf(out_file, " @modifiable_instruction");
  const std::set<std::string>& flags = instruction.flags();
  for (std::set<std::string>::const_iterator flag = flags.begin();
       flag != flags.end(); ++flag)
    if (!strncmp(flag->c_str(), "CPUFeature_", 11))
      fprintf(out_file, " @%s", flag->c_str());
  const std::vector<MarkedInstruction::Operand>& operands =
    instruction.operands();
  if (enabled(kParseOperands)) {
    if (enabled(kParseOperandPositions))
      fprintf(out_file, " @operands_count_is_%" NACL_PRIuS, operands.size());
    int operand_index = 0;
    for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
           operands.begin(); operand != operands.end(); ++operand) {
      if (operand->enabled)
        if (enabled(kParseOperandPositions) ||
            !memory_capable_operand(operand->type) ||
            !memory_access)
          fprintf(out_file, " @operand%d_%s", operand_index,
                  operand->size.c_str());
      static const struct { const char type, *dfa_type; } operand_types[] = {
        { '1', "one"              },
        { 'a', "rax"              },
        { 'c', "rcx"              },
        { 'd', "rdx"              },
        { 'i', "second_immediate" },
        { 'o', "port_dx"          },
        { 't', "st"               },
        { 'x', "ds_rbx"           },
        { 'B', "from_vex"         },
        { 'H', "from_vex"         },
        { 'I', "immediate"        },
        { 'O', "absolute_disp"    },
        { 'X', "ds_rsi"           },
        { 'Y', "es_rdi"           }
      };
      size_t n = 0;
      while (n < sizeof operand_types/sizeof operand_types[0] &&
             operand_types[n].type != operand->type)
        ++n;
      if (n == sizeof operand_types/sizeof operand_types[0]) {
        if (operand->enabled)
          if (enabled(kParseOperandPositions) ||
              !memory_capable_operand(operand->type) ||
              !memory_access)
            ++operand_index;
        continue;
      }
      if (operand->enabled) {
        fprintf(out_file, " @operand%d_%s", operand_index,
                operand_types[n].dfa_type);
        ++operand_index;
      }
    }
  }
  if (enabled(kParseOperandsStates)) {
    size_t operand_index = 0;
    for (std::vector<MarkedInstruction::Operand>::const_iterator
           operand = operands.begin(); operand != operands.end(); ++operand) {
      if (operand->enabled) {
        if (operand->write)
          if (operand->read)
            fprintf(out_file, " @operand%" NACL_PRIuS "_readwrite",
                    operand_index);
          else
            fprintf(out_file, " @operand%" NACL_PRIuS "_write", operand_index);
        else
          if (operand->read)
            fprintf(out_file, " @operand%" NACL_PRIuS "_read", operand_index);
          else
            fprintf(out_file, " @operand%" NACL_PRIuS "_unused", operand_index);
      }
      if (operand->enabled || enabled(kParseOperandPositions))
        ++operand_index;
    }
  }
  if (instruction.opcode_in_modrm())
    fprintf(out_file, " any* &");
}

/* print_immediate_arguments prints prints machine that accepts immediates.

   This includes “normal immediates”, “relative immediates” (for call/j*),
   and “32bit/64bit offset” (e.g. in “movabs” instruction).  */
void print_immediate_arguments(const MarkedInstruction& instruction) {
  const std::vector<MarkedInstruction::Operand>& operands =
    instruction.operands();
  size_t operand_index = 0;
  for (std::vector<MarkedInstruction::Operand>::const_iterator
         operand = operands.begin(); operand != operands.end(); ++operand)
    if (operand->enabled || enabled(kParseOperandPositions))
      ++operand_index;
  for (std::vector<MarkedInstruction::Operand>::const_reverse_iterator
         operand = operands.rbegin(); operand != operands.rend(); ++operand) {
    if (operand->type == 'i') {
      if (operand->size == "8bit") {
        fprintf(out_file, " imm8n2");
      } else if (operand->size == "16bit") {
        fprintf(out_file, " imm16n2");
      } else {
        fprintf(stderr, "%s: error - can not determine immediate size: %c%s",
                short_program_name, operand->type, operand->size.c_str());
        exit(1);
      }
    } else if (operand->type == 'I') {
      if (operand->size == "2bit") {
        fprintf(out_file, " imm2");
      } else if (operand->size == "8bit") {
        fprintf(out_file, " imm8");
      } else if (operand->size == "16bit") {
        fprintf(out_file, " imm16");
      } else if (operand->size == "32bit") {
        fprintf(out_file, " imm32");
      } else if (operand->size == "64bit") {
        fprintf(out_file, " imm64");
      } else {
        fprintf(stderr, "%s: error - can not determine immediate size: %c%s",
                short_program_name, operand->type, operand->size.c_str());
        exit(1);
      }
    } else if (operand->type == 'J') {
      if (operand->size == "8bit") {
        fprintf(out_file, " rel8");
      } else if (operand->size == "16bit") {
        fprintf(out_file, " rel16");
      } else if (operand->size == "32bit") {
        fprintf(out_file, " rel32");
      } else {
        fprintf(stderr, "%s: error - can not determine immediate size: %c%s",
                short_program_name, operand->type, operand->size.c_str());
        exit(1);
      }
    } else if (operand->type == 'L') {
      if (operands.size() == 4) {
        fprintf(out_file, " b_%cxxx_0000", ia32_mode ? '0' : 'x');
        if (!enabled(kInstructionName))
          fprintf(out_file, " @last_byte_is_not_immediate");
      }
      if (operand->enabled && enabled(kParseOperands))
        fprintf(out_file, " @operand%" NACL_PRIuS "_from_is4",
                operand_index - 1);
    } else if (operand->type == 'O') {
      fprintf(out_file, ia32_mode ? " disp32" : " disp64");
    }
    if (operand->enabled || enabled(kParseOperandPositions))
      --operand_index;
  }
  const std::multiset<std::string>& required_prefixes =
    instruction.required_prefixes();
  for (std::multiset<std::string>::const_iterator required_prefix =
         required_prefixes.begin(); required_prefix != required_prefixes.end();
         ++required_prefix)
    if (*required_prefix == "0xf0") {
      for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
             operands.begin(); operand != operands.end(); ++operand)
        if (operand->type == 'C') {
          fprintf(out_file, " @not_lock_prefix%" NACL_PRIuS,
                  operand - operands.begin());
          break;
        }
      break;
    }
}

/* print_instruction_end_actions prints actions which must be triggered at
   the end of instruction.

   This function is only used when we don't parse all the operands: in this
   case we can not just use uniform instruction-independent action. We use
   one of @process_0_operands, @process_1_operand, or @process_2_operands
   and if instruction stores something to 32bit register we add _zero_extends
   to the name of action (e.g.: @process_1_operand_zero_extends).  */
void print_instruction_end_actions(const MarkedInstruction& instruction,
                                   bool memory_access) {
  if (enabled(kParseOperands)) {
    if (!enabled(kParseOperandPositions)) {
      int operands_count = 0;
      bool zero_extends = false;
      const std::vector<MarkedInstruction::Operand>& operands =
        instruction.operands();
      for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
             operands.begin(); operand != operands.end(); ++operand)
        if (operand->enabled &&
            (!memory_capable_operand(operand->type) ||
             !memory_access)) {
          ++operands_count;
          if (operand->size == "32bit" &&
              instruction.has_flag("nacl-amd64-zero-extends"))
            zero_extends = true;
        }
      if (operands_count == 1)
        fprintf(out_file, " @process_1_operand");
      else
        fprintf(out_file, " @process_%d_operands", operands_count);
      if (zero_extends) fprintf(out_file, "_zero_extends");
    }
  }
}

/* print_one_size_definition_nomodrm prints full definition of one single
   instruction which does not include “ModR/M byte”.

   The only twist here is the 0x90 opcode: it requires special handling
   because all combinations except straight “0x90” and “0x48 0x90” are
   handled as normal “xchg” instruction, but “0x90” and “0x48 0x90” are
   treated specially to make sure there are one-byte “nop” in the
   instruction set.  */
void print_one_size_definition_nomodrm(const MarkedInstruction& instruction) {
  print_operator_delimiter();
  std::vector<std::string> opcodes = instruction.opcodes();
  if (opcodes[0] == "(0x90|0x91|0x92|0x93|0x94|0x95|0x96|0x97)")
    fprintf(out_file, "((");
  print_legacy_prefixes(instruction);
  print_rex_prefix(instruction);
  print_opcode_nomodrm(instruction);
  if (instruction.opcode_in_imm()) {
    print_immediate_opcode(instruction);
    print_opcode_recognition(instruction, false);
  } else {
    print_opcode_recognition(instruction, false);
    print_immediate_arguments(instruction);
  }
  if (opcodes[0] == "(0x90|0x91|0x92|0x93|0x94|0x95|0x96|0x97)")
    fprintf(out_file, ") - (0x90|0x48 0x90))");
  print_instruction_end_actions(instruction, false);
}

/* print_one_size_definition_modrm_register prints full definition of one
   single which does include “ModR/M byte” but which is not used to access
   memory.

   This function should handle two corner cases: when field “reg” in
   “ModR/M byte” is used to extend opcode and when “imm” field is used to
   extend opcode.  These two cases are never combined.  */
void print_one_size_definition_modrm_register(
    const MarkedInstruction& instruction) {
  print_operator_delimiter();
  MarkedInstruction adjusted_instruction = instruction;
  if (mod_reg_is_used(adjusted_instruction))
    adjusted_instruction = adjusted_instruction.set_rex_r();
  if (mod_rm_is_used(adjusted_instruction))
    adjusted_instruction = adjusted_instruction.set_rex_b();
  print_legacy_prefixes(adjusted_instruction);
  print_rex_prefix(adjusted_instruction);
  print_opcode_nomodrm(adjusted_instruction);
  if (!adjusted_instruction.opcode_in_imm())
    print_opcode_recognition(adjusted_instruction, false);
  fprintf(out_file, " modrm_registers");
  if (enabled(kParseOperands)) {
    size_t operand_index = 0;
    const std::vector<MarkedInstruction::Operand>& operands =
      adjusted_instruction.operands();
    for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
           operands.begin(); operand != operands.end(); ++operand) {
      const char* operand_type;
      switch (operand->type) {
        case 'C':
        case 'D':
        case 'G':
        case 'P':
        case 'V':
          operand_type = "reg";
          break;
        case 'S':
          operand_type = "reg_norex";
          break;
        case 'E':
        case 'M':
        case 'N':
        case 'Q':
        case 'R':
        case 'U':
        case 'W':
           operand_type = "rm";
           break;
        default:
          if (operand->enabled || enabled(kParseOperandPositions))
            ++operand_index;
          continue;
      }
      if (operand->enabled)
        fprintf(out_file, " @operand%" NACL_PRIuS "_from_modrm_%s",
                operand_index, operand_type);
      if (operand->enabled || enabled(kParseOperandPositions))
        ++operand_index;
    }
  }
  if (adjusted_instruction.opcode_in_modrm())
    fprintf(out_file, ")");
  if (adjusted_instruction.opcode_in_imm()) {
    print_immediate_opcode(adjusted_instruction);
    print_opcode_recognition(adjusted_instruction, false);
  } else {
    print_immediate_arguments(adjusted_instruction);
  }
  print_instruction_end_actions(adjusted_instruction, false);
}

/* print_one_size_definition_modrm_memory prints full definition of one
   single which does include “ModR/M byte” which is used to access memory.

   This is the most complicated and expensive variant to parse because there
   are so many variants: with and without base, without and without index and,
   accordingly, with zero, one, or two bits used from REX/VEX byte.

   Additionally function should handle two corner cases: when field “reg” in
   “ModR/M byte” is used to extend opcode and when “imm” field is used to
   extend opcode.  These two cases are never combined.  */
void print_one_size_definition_modrm_memory(
    const MarkedInstruction& instruction) {
  static const struct { const char* mode; bool rex_x; bool rex_b; } modes[] = {
    { " operand_disp",           false, true  },
    { " operand_rip",            false, false },
    { " single_register_memory", false, true  },
    { " operand_sib_pure_index", true,  false },
    { " operand_sib_base_index", true,  true  }
  };
  for (size_t mode = 0; mode < sizeof modes/sizeof modes[0]; ++mode) {
    print_operator_delimiter();
    MarkedInstruction adjusted_instruction = instruction;
    if (mod_reg_is_used(adjusted_instruction))
      adjusted_instruction = adjusted_instruction.set_rex_r();
    if (mod_rm_is_used(adjusted_instruction)) {
      if (modes[mode].rex_x)
        adjusted_instruction = adjusted_instruction.set_rex_x();
      if (modes[mode].rex_b)
        adjusted_instruction = adjusted_instruction.set_rex_b();
    }
    print_legacy_prefixes(adjusted_instruction);
    print_rex_prefix(adjusted_instruction);
    print_opcode_nomodrm(adjusted_instruction);
    if (!adjusted_instruction.opcode_in_imm())
      print_opcode_recognition(adjusted_instruction, true);
    if (adjusted_instruction.opcode_in_modrm())
      fprintf(out_file, " any");
    else
      fprintf(out_file, " (any");
    if (enabled(kParseOperands)) {
      size_t operand_index = 0;
      const std::vector<MarkedInstruction::Operand>& operands =
        adjusted_instruction.operands();
      for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
             operands.begin(); operand != operands.end(); ++operand) {
        const char* operand_type;
        switch (operand->type) {
          case 'C':
          case 'D':
          case 'G':
          case 'P':
          case 'V':
            operand_type = "from_modrm_reg";
            break;
          case 'S':
            operand_type = "from_modrm_reg_norex";
            break;
          case 'E':
          case 'M':
          case 'N':
          case 'Q':
          case 'R':
          case 'U':
          case 'W':
            operand_type = "rm";
            break;
          default:
            if (operand->enabled || enabled(kParseOperandPositions))
              ++operand_index;
            continue;
        }
        if (operand->enabled) {
          if (strcmp(operand_type, "rm") != 0 ||
              enabled(kParseOperandPositions)) {
            fprintf(out_file, " @operand%" NACL_PRIuS "_%s", operand_index,
                    operand_type);
            ++operand_index;
          }
        } else if (enabled(kParseOperandPositions)) {
          ++operand_index;
        }
      }
    }
    fprintf(out_file, " . any* &%s", modes[mode].mode);
    if (enabled(kCheckAccess) &&
        !adjusted_instruction.has_flag("no_memory_access"))
      fprintf(out_file, " @check_access");
    fprintf(out_file, ")");
    if (adjusted_instruction.opcode_in_imm()) {
      print_immediate_opcode(adjusted_instruction);
      print_opcode_recognition(adjusted_instruction, true);
    } else {
      print_immediate_arguments(adjusted_instruction);
    }
    print_instruction_end_actions(adjusted_instruction, true);
  }
}

/* Convert sizes from AMD/Intel manual format to human-readable format. I.e.
   instead of “pqwx” we get either “ymm” (if operand is register) or “256bit”
   (if operand is memory).  All instructions “downstream” (that is: above this
   line) use this description.  */
MarkedInstruction expand_sizes(const MarkedInstruction& instruction,
                               bool memory_access) {
  bool opcode_in_modrm = instruction.opcode_in_modrm();
  bool memory_operands = false;
  std::vector<MarkedInstruction::Operand> operands =
    instruction.operands();
  for (std::vector<MarkedInstruction::Operand>::iterator operand =
         operands.begin(); operand != operands.end(); ++operand) {
    struct { const char type, *size, *register_size, *memory_size; }
                                                             operand_sizes[] = {
      /* 8/16/32/64bits: mostly GP registers.  */
      { ' ', "b",    "8bit",    "8bit"       },
        /* Sw is segment register. */
      { 'S', "w",    "segreg",  NULL         },
      { ' ', "w",    "16bit",   "16bit"      },
      { ' ', "d",    "32bit",   "32bit"      },
        /* q may be %rXX, %mmX or %xmmX.  */
      { 'N', "q",    "mmx",     "64bit"      },
      { 'P', "q",    "mmx",     "64bit"      },
      { 'Q', "q",    "mmx",     "64bit"      },
      { 'U', "q",    "xmm",     "64bit"      },
      { 'V', "q",    "xmm",     "64bit"      },
      { 'W', "q",    "xmm",     "64bit"      },
      { ' ', "q",    "64bit",   "64bit"      },
      /* GP registers & memory/control registers/debug registers.  */
        /* Control registers.  */
      { 'C', "r",    "creg",    NULL         },
        /* Debug registers.    */
      { 'D', "r",    "dreg",    NULL         },
        /* Test registers.    */
      { 'T', "r",    "treg",    NULL         },
        /* 32bit register in ia32 mode, 64bit register in amd64 mode.  */
      { ' ', "r",    "regsize", "regsize"    },
      /* XMM registers/128bit memory.  */
      { ' ', "",     "xmm",     "128bit"     },
      { ' ', "o",    "xmm",     "128bit"     },
      { ' ', "dq",   "xmm",     "128bit"     },
      /* YMM registers/256bit memory.  */
      { ' ', "do",   "ymm",     "256bit"     },
      { ' ', "fq",   "ymm",     "256bit"     },
      { ' ', "pdwx", "ymm",     "256bit"     },
      { ' ', "pdx",  "ymm",     "256bit"     },
      { ' ', "phx",  "ymm",     "256bit"     },
      { ' ', "pix",  "ymm",     "256bit"     },
      { ' ', "pjx",  "ymm",     "256bit"     },
      { ' ', "pkx",  "ymm",     "256bit"     },
      { ' ', "pqwx", "ymm",     "256bit"     },
      { ' ', "pqx",  "ymm",     "256bit"     },
      { ' ', "psx",  "ymm",     "256bit"     },
      { ' ', "pwdx", "ymm",     "256bit"     },
      { ' ', "pwx",  "ymm",     "256bit"     },
      { ' ', "x",    "ymm",     "256bit"     },
      /* MMX/XXM registers, 64bit/128bit in memory.  */
#define MMX_XMM_registers(size)                                               \
      { 'a', size,   "xmm",     NULL         }, /* %xmm0 */                   \
      { 'H', size,   "xmm",     NULL         },                               \
      { 'L', size,   "xmm",     NULL         },                               \
      { 'M', size,   NULL,      "128bit"     },                               \
      { 'N', size,   "mmx",     NULL         },                               \
      { 'P', size,   "mmx",     NULL         },                               \
      { 'Q', size,   "mmx",     "64bit"      },                               \
      { 'U', size,   "xmm",     NULL         },                               \
      { 'V', size,   "xmm",     NULL         },                               \
      { 'W', size,   "xmm",     "128bit"     }
      MMX_XMM_registers("pb"),
      MMX_XMM_registers("pd"),
      MMX_XMM_registers("pdw"),
      MMX_XMM_registers("ph"),
      MMX_XMM_registers("phi"),
      MMX_XMM_registers("pi"),
      MMX_XMM_registers("pj"),
      MMX_XMM_registers("pk"),
      MMX_XMM_registers("pq"),
      MMX_XMM_registers("pqw"),
      MMX_XMM_registers("ps"),
      MMX_XMM_registers("pw"),
      /* XMM registers, floating point operands in memory.  */
      { 'H', "sd",   "xmm",     "float64bit" },
      { 'L', "sd",   "xmm",     "float64bit" },
      { 'U', "sd",   "xmm",     "float64bit" },
      { 'V', "sd",   "xmm",     "float64bit" },
      { 'W', "sd",   "xmm",     "float64bit" },
      { ' ', "sd",   NULL,      "float64bit" },
      { 'H', "ss",   "xmm",     "float32bit" },
      { 'L', "ss",   "xmm",     "float32bit" },
      { 'U', "ss",   "xmm",     "float32bit" },
      { 'V', "ss",   "xmm",     "float32bit" },
      { 'W', "ss",   "xmm",     "float32bit" },
      { ' ', "ss",   NULL,      "float32bit" },
      /* 2bit immediate.  */
      { ' ', "2",    "2bit",    NULL         },
      /* x87 register.  */
      { ' ', "7",    "x87",     NULL         },
      /* Various structures in memory.  */
      { ' ', "p",    NULL,      "farptr"     },
      { ' ', "s",    NULL,      "selector"   },
      { ' ', "sb",   NULL,      "x87_bcd"    },
      { ' ', "se",   NULL,      "x87_env"    },
      { ' ', "si",   NULL,      "x87_32bit"  },
      { ' ', "sq",   NULL,      "x87_64bit"  },
      { ' ', "sr",   NULL,      "x87_state"  },
      { ' ', "st",   NULL,      "float80bit" },
      { ' ', "sw",   NULL,      "x87_16bit"  },
      { ' ', "sx", NULL, "x87_mmx_xmm_state" }
    };
    size_t n = 0;
    for (n = 0; n < sizeof operand_sizes/sizeof operand_sizes[0]; ++n)
      if ((operand_sizes[n].type == operand->type ||
           operand_sizes[n].type == ' ') &&
           operand_sizes[n].size == operand->size)
        break;
    if (n != sizeof operand_sizes/sizeof operand_sizes[0]) {
      const char* register_size = operand_sizes[n].register_size;
      const char* memory_size = operand_sizes[n].memory_size;
      bool memory_or_register = operand->type == 'E' ||
                                operand->type == 'Q' ||
                                operand->type == 'W';
      bool always_memory = operand->type == 'M';
      bool memory_operand = memory_access &&
                            (memory_or_register || always_memory);
      const char* operand_size = memory_operand ? memory_size : register_size;
      bool memory_ne_register = memory_or_register &&
                                strcmp(memory_size, register_size) != 0;
      operand->size = operand_size;
      if (operand_size != NULL) {
        if ((!enabled(kParseImmediateOperands) &&
             (operand->type == 'I' || operand->type == 'i')) ||
            (!enabled(kParseRelativeOperands) &&
              (operand->type == 'J' || operand->type == 'O')) ||
            (!enabled(kParseX87Operands) &&
              (strncmp(operand_size, "x87", 3) == 0 ||
               ((strncmp(operand_size, "float", 5) == 0) && !register_size))) ||
            (!enabled(kParseMMXOperands) &&
              (strcmp(operand_size, "mmx") == 0 ||
              (strcmp(operand_size, "64bit") == 0 && register_size &&
               strcmp(register_size, "mmx") == 0))) ||
            (!enabled(kParseXMMOperands) &&
              (strcmp(operand_size, "xmm") == 0 ||
              ((strcmp(operand_size, "32bit") == 0 ||
                strcmp(operand_size, "64bit") == 0 ||
                strcmp(operand_size, "128bit") == 0) && register_size &&
                strcmp(register_size, "xmm") == 0) ||
              (strncmp(operand_size, "float", 5) == 0 && register_size &&
               strcmp(register_size, "xmm") == 0))) ||
            (!enabled(kParseYMMOperands) &&
              (strcmp(operand_size, "ymm") == 0 ||
              (strcmp(operand_size, "256bit") == 0 && register_size &&
               strcmp(register_size, "ymm") == 0))) ||
            (!enabled(kParseNonWriteRegisters) &&
              !operand->write)) {
          operand->enabled = false;
        } else if (enabled(kParseOperands) && memory_or_register) {
          opcode_in_modrm |= memory_ne_register;
          memory_operands |= memory_operand;
        }
      } else {
        fprintf(stderr, "%s: error - can not determine operand size: %c%s",
                short_program_name, operand->type, operand->size.c_str());
        exit(1);
      }
    } else {
      fprintf(stderr, "%s: error - can not determine operand size: %c%s",
              short_program_name, operand->type, operand->size.c_str());
      exit(1);
    }
  }
  if (opcode_in_modrm &&
      !instruction.opcode_in_modrm() && !instruction.opcode_in_imm()) {
    std::vector<std::string> opcodes = instruction.opcodes();
    opcodes.push_back(memory_operands ? "/m" : "/r");
    return instruction.set_opcode_in_modrm().with_operands(operands).
           with_opcodes(opcodes);
  } else {
    return instruction.with_operands(operands);
  }
}

/* print_one_size_definition prints definition for one single operands size.

   Note that 64bit operands are illegal in ia32 mode (duh).  This function
   does not print anything in this case.  */
void print_one_size_definition(const MarkedInstruction& instruction) {
  /* 64bit commands are not supported in ia32 mode.  */
  if (ia32_mode && instruction.rex_w()) return;

  bool modrm_register_only = false;
  bool modrm_memory = false;
  bool modrm_register = false;
  char operand_source = ' ';
  const std::vector<MarkedInstruction::Operand>& operands =
    instruction.operands();
  for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
         operands.begin(); operand != operands.end(); ++operand)
    switch (operand->type) {
      case 'E':
      case 'Q':
      case 'W':
        modrm_register = true;
        /* Fallthrough.  */
      case 'M':
        /* Two different operands can not use “r/m” field in “ModR/M” byte.  */
        if (modrm_memory) {
          fprintf(stderr,
                  "%s: error - conflicting operand sources: '%c' and '%c'",
                  short_program_name, operand_source, operand->type);
          exit(1);
        }
        modrm_memory = true;
        operand_source = operand->type;
        break;
      case 'N':
      case 'R':
      case 'U':
        /* Two different operands can not use “reg” field in “ModR/M” byte.  */
        if (modrm_register_only) {
          fprintf(stderr,
                  "%s: error - conflicting operand sources: '%c' and '%c'",
                  short_program_name, operand_source, operand->type);
          exit(1);
        }
        modrm_register_only = true;
        modrm_register = true;
        operand_source = operand->type;
        break;
      default:
        break;
    }
  if (modrm_memory || modrm_register) {
    if (modrm_memory) {
      MarkedInstruction instruction_with_expanded_sizes =
        expand_sizes(instruction, true);
      print_one_size_definition_modrm_memory(instruction_with_expanded_sizes);
      if (instruction.has_flag("lock"))
        print_one_size_definition_modrm_memory(
          instruction_with_expanded_sizes.add_requred_prefix("lock"));
    }
    if (modrm_register)
      print_one_size_definition_modrm_register(
        expand_sizes(instruction, false));
  } else {
    print_one_size_definition_nomodrm(expand_sizes(instruction, false));
  }
}

/* print_instruction_px prints “xmm” and “ymm” versions of the instructions.

   If there are no “L” bit then it just calls print_one_size_definition  */
void print_instruction_px(const MarkedInstruction& instruction) {
  std::vector<MarkedInstruction::Operand> operands =
    instruction.operands();
  for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
         operands.begin(); operand != operands.end(); ++operand)
    if ((*operand->size.rbegin() == 'x') &&
        ((operand->size.length() == 1) || (*operand->size.begin() == 'p'))) {
      for (std::vector<MarkedInstruction::Operand>::iterator operand =
             operands.begin(); operand != operands.end();
           ++operand)
        if ((*operand->size.rbegin() == 'x') &&
            ((operand->size.length() == 1) || (*operand->size.begin() == 'p')))
          operand->size.resize(operand->size.length() - 1);
      std::vector<std::string> opcodes = instruction.opcodes();
      for (std::vector<std::string>::iterator opcode = opcodes.begin();
           opcode != opcodes.end(); ++opcode) {
        size_t Lbit = opcode->find(".L.");
        if (Lbit != opcode->npos) {
          opcode->at(++Lbit) = '0';
          print_one_size_definition(instruction.
                                    with_opcodes(opcodes).
                                    with_operands(operands));
          opcode->at(Lbit) = '1';
          print_one_size_definition(instruction.with_opcodes(opcodes));
          return;
        }
      }
      fprintf(stderr, "%s: error - can not set 'L' bit in instruction '%s'",
              short_program_name, instruction.name().c_str());
      exit(1);
    }
  print_one_size_definition(instruction);
}

/* print_instruction_vyz splits 16bit/32bit/64bit versions of instruction.

   If there are no “z” (16bit/32bit) operands, “y” (32bit/64bit) operands, or
   “v” (16bit/32bit/64bit) operands then it calls print_instruction_px twice:
   once with W cleared and once with W set.  If flag is “norexw” is used or if
   “rexw” pseudo-prefix is used then this second call does not happen.  */
void print_instruction_vyz(const MarkedInstruction& instruction) {
  const std::vector<MarkedInstruction::Operand>& operands =
    instruction.operands();
  for (std::vector<MarkedInstruction::Operand>::const_iterator
         operand = operands.begin(); operand != operands.end(); ++operand)
    if (operand->size == "v" || operand->size == "z") {
      print_instruction_px(instruction.add_requred_prefix("data16"));
      print_instruction_px(instruction.clear_rex_w());
      MarkedInstruction instruction_w = instruction.set_rex_w();
      print_instruction_px(instruction_w);
      if (enabled(kNaClForbidden))
        print_instruction_px(instruction_w.add_requred_prefix("data16"));
      return;
    }
  for (std::vector<MarkedInstruction::Operand>::const_iterator
         operand = operands.begin(); operand != operands.end(); ++operand)
    if (operand->size == "y" ||
        (operand->type == 'S' && operand->size == "w")) {
      print_instruction_px(instruction.clear_rex_w());
      MarkedInstruction instruction_w = instruction.set_rex_w();
      print_instruction_px(instruction.set_rex_w());
      return;
    }
  print_instruction_px(instruction);
  const std::multiset<std::string>& required_prefixes =
    instruction.required_prefixes();
  if (!instruction.has_flag("norex") && !instruction.has_flag("norexw") &&
      !instruction.rex_w() &&
      required_prefixes.find("data16") == required_prefixes.end())
    print_instruction_px(instruction.set_rex_w());
}

/* print_one_instruction_definition prints definition for the instruction.

   It processes instructions one-by-one and does first preliminary split:
   non-marked operands (which means they are 8bit/16bit/32bit/64bit operands)
   are processed as two separate instructions—once as 8bit operand and once
   as 16bit/32bit/64bit operand (16bit/32bit for immediates).  */
void print_one_instruction_definition(void) {
  for (std::vector<Instruction>::const_iterator
         instruction = instructions.begin(); instruction != instructions.end();
         ++instruction) {
    MarkedInstruction marked_instruction(*instruction);
    std::vector<MarkedInstruction::Operand> operands =
      marked_instruction.operands();
    bool found_operand_with_empty_size = false;
    for (std::vector<MarkedInstruction::Operand>::const_iterator
           operand = operands.begin(); operand != operands.end(); ++operand) {
      if (operand->size == "") {
        found_operand_with_empty_size = true;
        break;
      }
    }
    if (found_operand_with_empty_size) {
      for (std::vector<MarkedInstruction::Operand>::iterator operand =
           operands.begin(); operand != operands.end(); ++operand)
        if (operand->size == "") operand->size = "b";
      print_instruction_vyz(marked_instruction.with_operands(operands));
      std::vector<std::string> opcodes = marked_instruction.opcodes();
      for (std::vector<std::string>::reverse_iterator opcode = opcodes.rbegin();
           opcode != opcodes.rend(); ++opcode)
        if (opcode->find('/') == opcode->npos) {
          /* 'w' bit is last bit both in binary and textual form.  */
          *(opcode->rbegin()) += 0x1;
          break;
        }
      operands = marked_instruction.operands();
      for (std::vector<MarkedInstruction::Operand>::iterator
             operand = operands.begin(); operand != operands.end(); ++operand)
        if (operand->size == "") {
          if (operand->type == 'I')
            operand->size = "z";
          else
            operand->size = "v";
        }
      print_instruction_vyz(marked_instruction.
                            with_opcodes(opcodes).
                            with_operands(operands));
    } else {
      print_instruction_vyz(marked_instruction);
    }
  }
}

} // namespace

int main(int argc, char* argv[]) {
  /* basename(3) may change the passed argument thus we are using copy
     of argv[0].  This creates tiny memory leak but since we only do that
     once per program invocation it's contained.  */
  short_program_name = basename(strdup(argv[0]));

  current_dir_name = get_current_dir_name();
  size_t current_dir_name_len = strlen(current_dir_name);

  for (;;) {
    int option_index;

    int option = getopt_long(argc, argv, "c:d:hm:o:v",
                             kProgramOptions, &option_index);

    if (option == -1) {
      break;
    }

    switch (option) {
      case 'c': {
        const_file_name = optarg;
        break;
      }
      case 'd': {
        for (char* action_to_disable = strtok(optarg, ",");
             action_to_disable;
             action_to_disable = strtok(NULL, ",")) {
          const char** action_number;
          for (action_number = kDisablableActionsList; action_number !=
               kDisablableActionsList + NACL_ARRAY_SIZE(kDisablableActionsList);
               ++action_number) {
            if (strcmp(*action_number, action_to_disable) == 0)
              break;
          }
          if (action_number != kDisablableActionsList +
                                      NACL_ARRAY_SIZE(kDisablableActionsList)) {
            disabled_actions[action_number - kDisablableActionsList] = true;
          } else {
            fprintf(stderr, "%s: action '%s' is unknown\n",
                    short_program_name, action_to_disable);
            return 1;
          }
        }
        break;
      }
      case 'm': {
        if (!strcmp(optarg, "ia32")) {
          ia32_mode = true;
        } else if (!strcmp(optarg, "amd64")) {
          ia32_mode = false;
        } else {
          fprintf(stderr, "%s: mode '%s' is unknown\n",
                  short_program_name, optarg);
          return 1;
        }
        break;
      }
      case 'o':
        out_file_name = optarg;
        break;
      case 'h':
        printf(kProgramHelp, short_program_name);
        break;
      case 'v':
        printf(kVersionHelp, short_program_name, kVersion);
        break;
      case '?':
        /* getopt_long already printed an error message.  */
        return 1;
      default:
        assert(false);
    }
  }

  for (int i = optind; i < argc; ++i) {
    parse_instructions(argv[i]);
  }

  if (out_file_name) {
    out_file = fopen(out_file_name, "w");
    if (!out_file) {
      fprintf(stderr,
              "%s: can not open '%s' file (%s)\n",
              short_program_name, out_file_name, strerror(errno));
      return 1;
    }
    fprintf(out_file, "/* native_client/%s\n"
" * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.\n"
" * Compiled for %s mode.\n"
" */\n\n", strncmp(current_dir_name, out_file_name, current_dir_name_len) ?
             out_file_name :
             out_file_name + current_dir_name_len + 1,
           ia32_mode ? "ia32" : "x86-64");
  }

  if ((out_file_name || const_file_name) &&
      (enabled(kInstructionName) ||
      enabled(kParseOperands))) {
    size_t const_name_len = 0;
    if (out_file_name && !const_file_name) {
      const_name_len = strlen(out_file_name) + 10;
      const_file_name = static_cast<char*>(malloc(const_name_len));
      strcpy(const_cast<char*>(const_file_name), out_file_name);
      const char* dot_position = strrchr(const_file_name, '.');
      if (!dot_position) {
        dot_position = strrchr(const_file_name, '\0');
      }
      strcpy(const_cast<char*>(dot_position), "_consts.c");
    }
    const_file = fopen(const_file_name, "w");
    if (!const_file) {
      fprintf(stderr, "%s: can not open '%s' file (%s)\n",
              short_program_name, const_file_name, strerror(errno));
       return 1;
    }
    fprintf(const_file, "/* native_client/%s\n"
" * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.\n"
" * Compiled for %s mode.\n"
" */\n\n", strncmp(current_dir_name, const_file_name, current_dir_name_len) ?
             const_file_name :
             const_file_name + current_dir_name_len + 1,
           ia32_mode ? "ia32" : "x86-64");
    if (const_name_len) {
      free(const_cast<char*>(const_file_name));
      const_file_name = NULL;
    }
  }

  if (enabled(kInstructionName) ||
      enabled(kParseOperands)) {
    print_consts();

    if (out_file == const_file) {
      for (size_t i = 0; i < 80; ++i) {
        fputc('#', out_file);
      }
      fputc('\n', out_file);
    }
  }

  if (ia32_mode) {
    fprintf(out_file, "%%%%{\n"
"  machine decode_x86_32;\n"
"  alphtype unsigned char;\n"
"");
  } else {
    fprintf(out_file, "%%%%{\n"
"  machine decode_x86_64;\n"
"  alphtype unsigned char;\n"
"");
  }

  if (enabled(kInstructionName)) {
    print_name_actions();
  }

  fprintf(out_file, "\n  one_instruction =");

  print_one_instruction_definition();

  fprintf(out_file, ");\n"
"}%%%%\n"
"");
  return 0;
}
