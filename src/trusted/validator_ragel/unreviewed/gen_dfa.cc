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

std::set<std::string> instruction_names;

static void CheckFlagValid(const std::string& flag) {
  static const char* all_instruction_flags[] = {
    // Parsing flags.
    "branch_hint",
    "condrep",
    "lock",
    "no_memory_access",
    "norex",
    "norexw",
    "rep",

    // CPUID flags.
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

     // Flags for enabling/disabling based on architecture and validity.
     "ia32",
     "amd64",
     "nacl-ia32-forbidden",
     "nacl-amd64-forbidden",
     "nacl-forbidden",
     "nacl-amd64-zero-extends",
     "nacl-amd64-modifiable",

     // AT&T Decoder flags.
     "att-show-memory-suffix-b",
     "att-show-memory-suffix-l",
     "att-show-memory-suffix-ll",
     "att-show-memory-suffix-t",
     "att-show-memory-suffix-s",
     "att-show-memory-suffix-q",
     "att-show-memory-suffix-x",
     "att-show-memory-suffix-y",
     "att-show-memory-suffix-w",
     "att-show-name-suffix-b",
     "att-show-name-suffix-l",
     "att-show-name-suffix-ll",
     "att-show-name-suffix-t",
     "att-show-name-suffix-s",
     "att-show-name-suffix-q",
     "att-show-name-suffix-x",
     "att-show-name-suffix-y",
     "att-show-name-suffix-w",

     // Spurious REX.W bits (instructions “in”, “out”, “nop”, etc).
     "spurious-rex.w"
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

// Raw data about instruction pulled from .def file.  This class is never used
// directly, we do all the processing using it's descendants.
class Instruction {
 public:
  struct Operand {
    char type;
    std::string size;
    bool enabled;
    bool read;
    bool write;
    bool implicit;
    Operand(char type_, const std::string& size_, bool enabled_,
            bool read_, bool write_, bool implicit_) :
      type(type_),
      size(size_),
      enabled(enabled_),
      read(read_),
      write(write_),
      implicit(implicit_) {
    }
    Operand() {
    }
    char ReadWriteTextPrefix() const {
      if (read)
        if (write)
          return '&';
        else
          return '=';
      else
        if (write)
          return '!';
        else
          return '\'';
    }
  };

  Instruction(const std::string& name,
              const std::vector<Operand>& operands,
              const std::vector<std::string>& opcodes,
              const std::set<std::string>& flags) :
      name_(name),
      operands_(operands),
      opcodes_(opcodes),
      flags_(flags) {
  }

  const std::string& name(void) const {
    return name_;
  }

  const std::vector<Operand>& operands(void) const {
    return operands_;
  }

  const std::vector<std::string>& opcodes(void) const {
    return opcodes_;
  }

  const std::set<std::string>& flags(void) const {
    return flags_;
  }

  bool has_flag(const std::string& flag) const {
    return flags_.find(flag) != flags_.end();
  }

 protected:
  std::string name_;
  std::vector<Operand> operands_;
  std::vector<std::string> opcodes_;
  std::set<std::string> flags_;

  Instruction with_name(const std::string& name) const {
    Instruction result = *this;
    result.name_ = name;
    return result;
  }

  Instruction with_operands(const std::vector<Operand>& operands) const {
    Instruction result = *this;
    result.operands_ = operands;
    return result;
  }

  Instruction with_opcodes(const std::vector<std::string>& opcodes) const {
    Instruction result = *this;
    result.opcodes_ = opcodes;
    return result;
  }

  Instruction with_flags(const std::set<std::string>& flags) const {
    Instruction result = *this;
    result.flags_ = flags;
    return result;
  }

 private:
  void add_flag(const std::string& flag) {
    CheckFlagValid(flag);
    flags_.insert(flag);
  }

  friend void ParseDefFile(const char*);
  Instruction() {}
};

std::vector<Instruction> instructions;

FILE* out_file = stdout;
const char* out_file_name = NULL;

bool ia32_mode = true;

// ReadFile reads the filename and returns it's contents.  We don't try to
// save memory by using line-by-line processing.
std::string ReadFile(const char* filename) {
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
        // This may be MacOS EOL, or Windows one.  Assume MacOS for now.
        if (mac_eol_found)
          file_content.push_back('\n');
        else
          mac_eol_found = true;
      } else if (*it == '\n') {
        // '\r\n' is Windows EOL, not Mac's one.
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

// SplitTillComma advances the iterator till the next comma or line end,
// splits the traversed text by whitespaces and returns it.
//
// Respects quoted text.
std::vector<std::string> SplitTillComma(
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

// FindTheEndOfLine looks for the end of line or the end of file (whichever
// comes first).
std::string::const_iterator FindTheEndOfLine(std::string::const_iterator it,
                                             std::string::const_iterator eof) {
  while (it != eof && *it != '\n')
    ++it;
  return it;
}

// IsSupportedInstruction returns true if instruction is supported in a
// current mode (some instructions are only supported in ia32 mode or x86-64
// mode, some are excluded from validator's reduced DFA).
bool IsSupportedInstruction(const Instruction& instruction) {
  // This is “ia32”-exclusive instruction and we are in not “ia32” mode.
  if (instruction.has_flag("ia32") && !ia32_mode)
    return false;
  // This is “amd64”-exclusive instruction and we are in not in “amd4” mode.
  if (instruction.has_flag("amd64") && ia32_mode)
    return false;
  // If “nacl-forbidden” mode is not activated then we are done with checks.
  if (enabled(kNaClForbidden))
    return true;
  // “nacl-forbidden” instructions are not available with “nacl-forbidden” flag.
  if (instruction.has_flag("nacl-forbidden"))
    return false;
  // “nacl-ia32-forbidden” instructions are not available in “ia32” mode.
  if (instruction.has_flag("nacl-ia32-forbidden") && ia32_mode)
    return false;
  // “nacl-amd64-forbidden” instructions are not available in “amd64” mode.
  if (instruction.has_flag("nacl-amd64-forbidden") && !ia32_mode)
    return false;
  // No special flags, instruction is allowed.
  return true;
}

// parse_instructions parses the given *.def file and adds instruction
// definitions to the instructions vector and their names to instruction_names
// set.
//
// Note: it only accepts flags listed in CheckFlagValid.
void ParseDefFile(const char* filename) {
  const std::string file_content = ReadFile(filename);
  std::string::const_iterator it = file_content.begin();
  std::string::const_iterator eof = file_content.end();

  while (it != eof) {
    std::string::const_iterator line_end = FindTheEndOfLine(it, eof);
    if (it == line_end) {
      ++it;
      continue;
    }
    // If line starts with '#' then it's a comment.
    if (*it == '#') {
      it = line_end;
      continue;
    }
    Instruction instruction;
    std::vector<std::string> operation = SplitTillComma(&it, line_end);
    // Line with just whitespaces is ignored.
    if (operation.size() == 0)
      continue;
    // First word is operation name, other words are operands.
    for (std::vector<std::string>::reverse_iterator string = operation.rbegin();
         string != operation.rend() - 1; ++string) {
      Instruction::Operand operand;
      // Most times we can determine whether operand is used for reading or
      // writing using its position and number of operands (default case in this
      // switch).  In a few exceptions we add read/write annotations that
      // override default behavior. All such annotations are created by hand.
      switch (string->at(0)) {
        case '\'':
          operand = Instruction::Operand(string->at(1), string->substr(2),
                                         true, false, false, false);
          break;
        case '=':
          operand = Instruction::Operand(string->at(1), string->substr(2),
                                         true, true, false, false);
          break;
        case '!':
          operand = Instruction::Operand(string->at(1), string->substr(2),
                                         true, false, true, false);
          break;
        case '&':
          operand = Instruction::Operand(string->at(1), string->substr(2),
                                         true, true, true, false);
          break;
        default:
          if (string == operation.rbegin()) {
            if (operation.size() <= 3) {
              operand = Instruction::Operand(string->at(0), string->substr(1),
                                             true, true, true, false);
            } else {
              operand = Instruction::Operand(string->at(0), string->substr(1),
                                             true, false, true, false);
            }
          } else {
              operand = Instruction::Operand(string->at(0), string->substr(1),
                                             true, true, false, false);
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
      if (it == line_end) line_end = FindTheEndOfLine(++it, eof);
      instruction.opcodes_ = SplitTillComma(&it, line_end);
      if (*it == ',') {
        ++it;
        if (it == line_end) line_end = FindTheEndOfLine(++it, eof);
        const std::vector<std::string>& flags = SplitTillComma(&it, line_end);
        for (std::vector<std::string>::const_iterator flag = flags.begin();
             flag != flags.end(); ++flag)
          instruction.add_flag(*flag);
        if (!IsSupportedInstruction(instruction))
          enabled_instruction = false;
      }
    }
    if (enabled_instruction) {
      instruction.name_ = operation[0];
      instructions.push_back(instruction);
      instruction_names.insert(instruction.name());
    }
    if (FindTheEndOfLine(it, eof) != it) {
      fprintf(stderr, "%s: definition files must have three columns",
              short_program_name);
      exit(1);
    }
  }
}

// ToCIdentifier generates action name by replacing all characters except
// letters and numbers with underscores.  This may lead to the collisions,
// but these will be detected by ragel thus they can not lead to runtime
// errors.
std::string ToCIdentifier(std::string text) {
  std::string name;
  for (std::string::const_iterator c = text.begin(); c != text.end(); ++c)
    if (('A' <= *c && *c <= 'Z') || ('a' <= *c && *c <= 'z') ||
        ('0' <= *c && *c <= '9'))
      name.push_back(*c);
    else
      name.push_back('_');
  return name;
}

// PrintNameActions prints set of actions which save name of the instruction
// in instruction_name variable.
void PrintNameActions(void) {
  for (std::set<std::string>::const_iterator name =
         instruction_names.begin(); name != instruction_names.end(); ++name)
    fprintf(out_file, "  action instruction_%s"
            " { SET_INSTRUCTION_NAME(\"%s\"); }\n",
            ToCIdentifier(*name).c_str(), name->c_str());
}

// ExpandRegisterOpcode expands opcode for the instruction which embeds number
// of register in its opcode.
//
// The string sequence [“0xd8”, “0xf8”, "/", "0x10"] is rewritten to be
// [“0xd8”, “(0xf8|0xf9|0xfa|0xfb|0xfc|0xfd|0xfe|0xff)”, "/", "0x10"].
//
// Only one byte is changed and this is the last “regular” byte (i.e. not a
// MobR/M opcode extension or opcode-in-immediate byte).
//
// Note that this function modifies the opcode vector directly.
void ExpandRegisterByte(const std::string& name,
                        std::vector<std::string>* opcodes) {
  std::vector<std::string>::reverse_iterator modifiable_opcode =
    opcodes->rbegin();
  for (std::vector<std::string>::iterator opcode = opcodes->begin();
       opcode != opcodes->end(); opcode++)
    if ((*opcode->begin()) == '/') {
      modifiable_opcode = std::vector<std::string>::reverse_iterator(opcode);
      if (modifiable_opcode == opcodes->rend()) {
        fprintf(stderr, "%s: error - can not use 'r' operand in "
                        "instruction '%s'", short_program_name, name.c_str());
        exit(1);
      }
      break;
    }
  // Note that we need to output eight possibilities and separate them with
  // seven “|” separators thus we handle first case separately.
  size_t start_position;
  if ((*(modifiable_opcode->rbegin())) == '0') {
    start_position = 1;
  } else if (*(modifiable_opcode->rbegin()) == '8') {
    start_position = 9;
  } else {
    fprintf(stderr, "%s: error - can not use 'r' operand in "
                    "instruction '%s'", short_program_name, name.c_str());
    exit(1);
  }
  std::string saved_modifiable_opcode = *modifiable_opcode;
  (*modifiable_opcode) = "(";
  modifiable_opcode->append(saved_modifiable_opcode);
  for (size_t position = start_position; position < start_position + 7;
       ++position) {
    modifiable_opcode->push_back('|');
    (*(saved_modifiable_opcode.rbegin())) = "0123456789abcdef"[position];
    modifiable_opcode->append(saved_modifiable_opcode);
  }
  modifiable_opcode->push_back(')');
}

class MarkedInstruction : public Instruction {
 public:
  explicit MarkedInstruction(const Instruction& instruction_) :
      Instruction(instruction_),
      required_prefixes_(),
      optional_prefixes_(),
      rex_(),
      signature_after_modrm_(false),
      signature_after_imm_(false) {
    if (has_flag("branch_hint")) optional_prefixes_.insert("branch_hint");
    if (has_flag("condrep")) optional_prefixes_.insert("condrep");
    if (has_flag("rep")) optional_prefixes_.insert("rep");
    for (std::vector<std::string>::const_iterator opcode = opcodes_.begin();
         opcode != opcodes_.end(); ++opcode)
      if (*opcode == "/") {
        signature_after_imm_ = true;
        break;
      } else if (opcode->find('/') != opcode->npos) {
        signature_after_modrm_ = true;
        break;
      }
    // If register is stored in opcode we need to expand opcode now.
    for (std::vector<Operand>::const_iterator operand = operands_.begin();
         operand != operands_.end(); ++operand)
      if (operand->type == 'r') {
        ExpandRegisterByte(name_, &opcodes_);
        // x87 and MMX registers are not extended in x86-64 mode: there are
        // only 8 of them.  But only x87 operands use opcode to specify
        // register number.
        if (operand->size != "7") rex_.b = true;
        break;
      }
    // Some 'opcodes' include prefixes, move them there.
    for (;;) {
      if ((*opcodes_.begin()) == "rexw") {
        rex_.w_required = true;
      } else if ((*opcodes_.begin()) == "fwait") {
        fprintf(stderr, "%s: fwait pseudoprefix is not supported (%s)\n",
                short_program_name, name().c_str());
        exit(1);
      } else {
        static const char* prefix_bytes[] = {
          "0x66", "data16",
          "0xf0", "lock",
          "0xf2", "repnz", "rep",
          "0xf3", "repz", "condrep", "branch_hint"
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

  MarkedInstruction with_name(const std::string& name) const {
    return MarkedInstruction(Instruction::with_name(name),
                             required_prefixes_,
                             optional_prefixes_,
                             rex_,
                             signature_after_modrm_,
                             signature_after_imm_);
  }

  MarkedInstruction with_operands(
      const std::vector<Operand>& operands) const {
    return MarkedInstruction(Instruction::with_operands(operands),
                             required_prefixes_,
                             optional_prefixes_,
                             rex_,
                             signature_after_modrm_,
                             signature_after_imm_);
  }

  MarkedInstruction with_opcodes(
      const std::vector<std::string>& opcodes) const {
    return MarkedInstruction(Instruction::with_opcodes(opcodes),
                             required_prefixes_,
                             optional_prefixes_,
                             rex_,
                             signature_after_modrm_,
                             signature_after_imm_);
  }

  MarkedInstruction with_flags(const std::set<std::string>& flags) const {
    return MarkedInstruction(Instruction::with_flags(flags),
                             required_prefixes_,
                             optional_prefixes_,
                             rex_,
                             signature_after_modrm_,
                             signature_after_imm_);
  }

  MarkedInstruction with_flag(std::string flag) const {
    std::set<std::string> flags = this->flags();
    flags.insert(flag);
    return with_flags(flags);
  }

  const std::multiset<std::string>& required_prefixes(void) const {
    return required_prefixes_;
  }

  MarkedInstruction add_required_prefix(const std::string& prefix) const {
    MarkedInstruction result = *this;
    result.required_prefixes_.insert(prefix);
    if (prefix == "data16")
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

  MarkedInstruction add_optional_prefix(const std::string& prefix) const {
    MarkedInstruction result = *this;
    result.optional_prefixes_.insert(prefix);
    return result;
  }

  bool rex_b(void) const {
    return rex_.b;
  }

  MarkedInstruction set_rex_b(void) const {
    MarkedInstruction result = *this;
    result.rex_.b = true;
    return result;
  }

  bool rex_x(void) const {
    return rex_.x;
  }

  MarkedInstruction set_rex_x(void) const {
    MarkedInstruction result = *this;
    result.rex_.x = true;
    return result;
  }

  bool rex_r(void) const {
    return rex_.r;
  }

  MarkedInstruction set_rex_r(void) const {
    MarkedInstruction result = *this;
    result.rex_.r = true;
    return result;
  }

  bool rex_w(void) const {
    return rex_.w;
  }

  MarkedInstruction clear_rex_w(void) const {
    MarkedInstruction result = *this;
    result.rex_.w = false;
    return result;
  }

  bool rex_w_required(void) const {
    return rex_.w_required;
  }

  MarkedInstruction set_rex_w_required(void) const {
    MarkedInstruction result = *this;
    result.rex_.w_required = true;
    for (std::vector<Operand>::iterator operand = result.operands_.begin();
         operand != result.operands_.end(); ++operand)
      if (operand->size == "v" || operand->size == "y")
        operand->size = "q";
      else if (operand->size == "z")
        operand->size = "d";
    return result;
  }

  MarkedInstruction clear_rex_w_required(void) const {
    MarkedInstruction result = *this;
    result.rex_.w_required = false;
    for (std::vector<Operand>::iterator operand = result.operands_.begin();
         operand != result.operands_.end(); ++operand)
      if (operand->size == "v" || operand->size == "y" || operand->size == "z")
        operand->size = "d";
    return result;
  }

  MarkedInstruction set_opcode_in_modrm(void) const {
    MarkedInstruction result = *this;
    result.signature_after_modrm_ = true;
    return result;
  }

  bool signature_after_modrm(void) const {
    return signature_after_modrm_;
  }

  bool signature_after_imm(void) const {
    return signature_after_imm_;
  }

 private:
  // Prefixes which can affect the decoder state for this instruction, i.e. they
  // may make instruction use different size of immediate, different register or
  // even change the instruction completely.  For example “data16” prefix must
  // always be present in “mov $imm16, %ax” instruction, otherwise instruction
  // will be different and will include not two-byte  immediate, but four byte
  // immediate.  Another example: “movbe %eax,(%rax)” becomes totally different
  // instruction “crc32l (%rax),%eax” if you add “0xf2” (aka “repnz”) prefix.
  // These two instructions have nothing  to do with each other even if they
  // share the opcode.  Not only the semantic differs, but there are CPUs where
  // one of them is valid and accepted and another is invalid (all four
  // possibilities are actually realized).
  std::multiset<std::string> required_prefixes_;

  // Optional prefixes for this instruction.  Don't affect the instruction
  // decoding but may affect the instruction semantic.
  // For example “branch_hints”, “repnz”/“repz” in a string instructions, etc.
  // Note that these same prefixes may be used as a “required prefixes” in some
  // other instructions!
  std::multiset<std::string> optional_prefixes_;

  // Describes REX.B, REX.X, REX.R, and REX.W bits in REX/VEX instruction prefix
  // (see AMD/Intel documentation for details for when these bits can/should be
  // allowed and when they are correct/incorrect).
  //
  // REX.B/REX.X/REX.R never affect decoding of non-VEX/XOP-instructions but may
  // affect semantics.  REX.W may affect decoding, too, thus we have rex_.w and
  // rex_.w_required (which is set if the variant of the instruction in question
  // needs REX.W bit set).
  struct RexType {
    bool b : 1;
    bool x : 1;
    bool r : 1;
    bool w : 1;
    bool w_required : 1;
    RexType(void) : b(false), x(false), r(false), w(true), w_required(false) {
    }
  } rex_;

  // True iff "reg" field in "ModR/M byte" is used as an opcode extension.
  // Note that this covers cases where "reg-to-reg" instruction and
  // "reg-to-memory" instruction have diffrently-sized operands, e.g. 128bit
  // XMM register and 32bit single-precision float.
  // If this flag is set, we can't tell instruction opcode and/or operand sizes
  // until we consume ModR/M byte.
  bool signature_after_modrm_ : 1;

  // True iff "imm" byte is used as an opcode extension.  Used mostly by 3D Now!
  // instructions, but there are some multimedia instructions which use "imm"
  // byte in a similar fashion, such as "vcmppd". In such cases we don't know
  // what instruction we are dealing with until we consumed imm byte.
  bool signature_after_imm_ : 1;

  MarkedInstruction(const Instruction& instruction,
                    const std::multiset<std::string>& required_prefixes,
                    const std::multiset<std::string>& optional_prefixes,
                    const RexType& rex,
                    bool signature_after_modrm,
                    bool signature_after_imm) :
    Instruction(instruction),
    required_prefixes_(required_prefixes),
    optional_prefixes_(optional_prefixes),
    rex_(rex),
    signature_after_modrm_(signature_after_modrm),
    signature_after_imm_(signature_after_imm) {
  }
};

// IsModRMRegUsed returns true if the instruction includes operands which is
// encoded in field “reg” in “ModR/M byte” and must be extended by “R” bit in
// REX/VEX prefix.
bool IsModRMRegUsed(const MarkedInstruction& instruction) {
  const std::vector<MarkedInstruction::Operand>& operands =
    instruction.operands();
  for (std::vector<MarkedInstruction::Operand>::const_iterator
         operand = operands.begin(); operand != operands.end(); ++operand) {
    const char source = operand->type;
    const std::multiset<std::string>& prefixes =
      instruction.required_prefixes();
    // Control registers can use 0xf0 prefix to select %cr8…%cr15.
    if ((source == 'C' && prefixes.find("0xf0") == prefixes.end()) ||
        source == 'R' || source == 'G' || source == 'P' || source == 'V')
      return true;
  }
  return false;
}

// IsMemoryCapableOperand returns true if “source” uses field “r/m” in
// “ModR/M byte” and must be extended by “X” and/or “B” bits in REX/VEX prefix.
bool IsMemoryCapableOperand(char source) {
  if (source == 'E' || source == 'M' || source == 'N' ||
      source == 'Q' || source == 'R' || source == 'U' || source == 'W')
    return true;
  else
    return false;
}

// IsModRMRMIsUsed returns true if the instruction includes operands which is
// encoded in field “r/m” in “ModR/M byte” and must be extended by “X” and/or
// “B” bits in REX/VEX prefix.
bool IsModRMRMIsUsed(const MarkedInstruction& instruction) {
  const std::vector<MarkedInstruction::Operand>& operands =
    instruction.operands();
  for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
         operands.begin(); operand != operands.end(); ++operand) {
    const char source = operand->type;
    if (IsMemoryCapableOperand(source))
      return true;
  }
  return false;
}

bool instruction_definition_started = false;

// PrintOperatorDelimiter is a simple function: it starts a “ragel line”.
//
// When it's called for a first time it just starts a line, in all other
// cases it first finishes the previous line with “(”.
void PrintOperatorDelimiter(void) {
  if (instruction_definition_started)
    fprintf(out_file, ") |\n    (");
  else
    fprintf(out_file, "\n    (");
  instruction_definition_started = true;
}

// PrintLegacyPrefixes prints all possible combinations of legacy prefixes from
// required_prefixes and optional_prefixes sets passed to this function in the
// instruction argument.
//
// Right now we print all possible permutations of required_prefixes with
// all optional_prefixes in all positions but without repetitions.
//
// Later we may decide to implement other cases (such as: duplicated
// prefixes for decoder and/or prefixes in a single “preferred order”
// for strict validator).
void PrintLegacyPrefixes(const MarkedInstruction& instruction) {
  const std::multiset<std::string>& required_prefixes =
    instruction.required_prefixes();
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

// PrintREXPrefix prints machine that accepts all permitted REX prefixes for the
// instruction.
//
// There are two complications:
//  • ia32 mode does not support REX prefix.
//  • REX prefix is incompatible with VEX/XOP prefixes.
void PrintREXPrefix(const MarkedInstruction& instruction) {
  // Prefix REX is not used in ia32 mode.
  if (ia32_mode || instruction.has_flag("norex"))
    return;
  const std::vector<std::string>& opcodes = instruction.opcodes();
  // VEX/XOP instructions integrate REX bits and opcode bits.  They will
  // be printed in print_opcode_nomodrm.
  if (opcodes.size() >= 3 &&
      (opcodes[0] == "0xc4" ||
       (opcodes[0] == "0x8f" && opcodes[1] != "/0")))
    return;
  // Allow any bits in  rex prefix for the compatibility.
  // http://code.google.com/p/nativeclient/issues/detail?id=2517
  if (instruction.rex_w_required())
    fprintf(out_file, "REXW_RXB");
  else
    fprintf(out_file, "REX_RXB?");
  fprintf(out_file, " ");
}

// PrintThirdByteOfVEX prints byte machine which corresponds to the AMD manual
// depiction of the third byte.
//
// For example W.XXXX.X.01 becomes b_1_XXXX_X_01 or b_0_XXXX_X_01.  There are
// two non-trivial things:
//  • Bit “W” becomes either 0 or 1 depending on the state of REX.W flag.
//  • CPU-ignored bits (marked as “x”) are turned to 0.
void PrintThirdByteOfVEX(const MarkedInstruction& instruction,
                         const std::string& third_byte) {
  fprintf(out_file, " b_");
  for (std::string::const_iterator c = third_byte.begin();
       c != third_byte.end(); ++c)
    if (*c == 'W')
      if (instruction.rex_w_required())
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

// PrintVEXOpcode prints prints machine that accepts the main opcode of the
// VEX/XOP instruction.
//
// There are quite a few twists here:
//  • VEX can use two-byte 0xc4 VEX prefix and two-byte 0xc5 VEX prefix
//  • VEX prefix combines opcode selection table with RXB VEX bits
//  • one register can be embedded in a VEX prefix, too
void PrintVEXOpcode(const MarkedInstruction& instruction) {
  const std::vector<std::string>& opcodes = instruction.opcodes();
  bool c5_ok = opcodes[0] == "0xc4" &&
               (opcodes[1] == "RXB.01" ||
                opcodes[1] == "RXB.00001") &&
               (*opcodes[2].begin() == '0' ||
                *opcodes[2].begin() == 'x' ||
                *opcodes[2].begin() == 'X' ||
                (*opcodes[2].begin() == 'W' && !instruction.rex_w_required()));
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
      PrintThirdByteOfVEX(instruction, third_byte);
      if (enabled(kVexPrefix))
        fprintf(out_file, " @vex_prefix3");
      if (c5_ok) {
        fprintf(out_file, ") | (0xc5 ");
        if (!ia32_mode && rex_r)
          third_byte[0] = 'X';
        else
          third_byte[0] = '1';
        PrintThirdByteOfVEX(instruction, third_byte);
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

// PrintMainOpcodePart prints prints machine that accepts the main opcode of
// the instruction.
//
// There are quite a few twists here:
//  • opcode may include register (e.g. “push” and “pop” instructions)
//  • VEX/XOP mix together opcode and prefix (see PrintVEXOpcode)
//  • opcode can be embedded in “ModR/M byte” and “imm” parts of the
//    instruction.  These are NOT printed here.
void PrintMainOpcodePart(const MarkedInstruction& instruction) {
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
    PrintVEXOpcode(instruction);
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

// PrintOpcodeInImmediate is only used when opcode is embedded in “imm”
// field.
//
// Prints machine for this last part of the opcode.
void PrintOpcodeInImmediate(const MarkedInstruction& instruction) {
  bool print_opcode = false;
  const std::vector<std::string>& opcodes = instruction.opcodes();
  for (std::vector<std::string>::const_iterator opcode = opcodes.begin();
       opcode != opcodes.end(); ++opcode)
    if (*opcode == "/")
      print_opcode = true;
    else if (print_opcode)
      fprintf(out_file, " %s", opcode->c_str());
}

// PrintSignature prints actions describing instruction and its operands' types:
//  * name of the instruction
//  * number of operands
//  * types of the operands
//  * whether each operand is read and whether each operand is written
//
// Additionally, for implied operands (like %rax or %ds:(%rsi)) actions
// responsible for recognition of these operands are printed.
//
// Example signature:
//   @instruction_add
//   @operands_count_is_2
//   @operand0_8bit
//   @operand1_8bit
//   @operand0_readwrite
//   @operand1_read
//
// This function is called as soon as we get all required information. There
// are three possibilities depending on the encoding:
//  * after "official opcode bytes" - normal case.
//  * after "ModR/M byte".
//  * after "imm byte".
// These cases are discriminated by signature_after_modrm and
// signature_after_imm fields.
void PrintSignature(const MarkedInstruction& instruction,
                    bool memory_access) {
  const std::vector<std::string>& opcodes = instruction.opcodes();
  if (instruction.signature_after_modrm()) {
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
  if (enabled(kInstructionName)) {
    fprintf(out_file, " @instruction_%s",
            ToCIdentifier(instruction.name()).c_str());
  } else {
    if (instruction.signature_after_imm())
      fprintf(out_file, " @last_byte_is_not_immediate");
    else if (instruction.has_flag("nacl-amd64-modifiable") &&
             enabled(kParseOperands) &&
             !enabled(kParseOperandPositions))
      fprintf(out_file, " @modifiable_instruction");
  }
  const std::vector<MarkedInstruction::Operand>& operands =
    instruction.operands();
  const std::set<std::string>& flags = instruction.flags();
  for (std::set<std::string>::const_iterator flag = flags.begin();
       flag != flags.end(); ++flag) {
    if (strncmp(flag->c_str(), "CPUFeature_", 11) == 0)
      fprintf(out_file, " @%s", flag->c_str());
    if (enabled(kInstructionName)) {
      if (strncmp(flag->c_str(), "att-show-name-suffix-", 21) == 0) {
        fprintf(out_file, " @att_show_name_suffix_%s", flag->c_str() + 21);
      } else if (strncmp(flag->c_str(), "att-show-memory-suffix-", 23) == 0) {
        bool show_memory_suffix = memory_access;
        if (!show_memory_suffix)
          for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
                 operands.begin(); operand != operands.end(); ++operand)
            if (operand->type == 'o' ||
                operand->type == 'X' || operand->type == 'Y') {
              show_memory_suffix = true;
              break;
            }
        if (show_memory_suffix)
          fprintf(out_file, " @att_show_name_suffix_%s", flag->c_str() + 23);
      }
    }
  }
  if (enabled(kParseOperands)) {
    if (enabled(kParseOperandPositions))
      fprintf(out_file, " @operands_count_is_%" NACL_PRIuS, operands.size());
    int operand_index = 0;
    for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
           operands.begin(); operand != operands.end(); ++operand) {
      if (operand->enabled)
        if (enabled(kParseOperandPositions) ||
            !IsMemoryCapableOperand(operand->type) ||
            !memory_access)
          fprintf(out_file, " @operand%d_%s", operand_index,
                  operand->size.c_str());
      static const struct { const char type, *dfa_type; } operand_types[] = {
        { '1', "one"              },
        { 'a', "rax"              },
        { 'c', "rcx"              },
        { 'd', "rdx"              },
        { 'b', "rbx"              },
        { 'f', "rsp"              },
        { 'g', "rbp"              },
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
              !IsMemoryCapableOperand(operand->type) ||
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
  if (enabled(kInstructionName) &&
      !ia32_mode &&
      (opcodes.size() < 3 ||
       (opcodes[0] != "0xc4" &&
        (opcodes[0] != "0x8f" || opcodes[1] == "/0")))) {
    if (!instruction.signature_after_imm() && IsModRMRMIsUsed(instruction)) {
      if (instruction.signature_after_modrm())
        fprintf(out_file, " any* & ");
      else
        fprintf(out_file, " (");
      fprintf(out_file, "((any - b_00_xxx_100) | (b_00_xxx_100 any))");
    }
    if (!instruction.rex_b())
      fprintf(out_file, " @set_spurious_rex_b");
    if (!instruction.rex_x())
      fprintf(out_file, " @set_spurious_rex_x");
    if (!instruction.rex_r())
      fprintf(out_file, " @set_spurious_rex_r");
    if (!instruction.rex_w())
      fprintf(out_file, " @set_spurious_rex_w");
    if (!instruction.signature_after_imm() && IsModRMRMIsUsed(instruction))
      fprintf(out_file, " any* &");
  } else if (instruction.signature_after_modrm())
    fprintf(out_file, " any* &");
}

// PrintImmediateArguments prints prints machine that accepts immediates.
//
// This includes “normal immediates”, “relative immediates” (for call/j*),
// and “32bit/64bit offset” (e.g. in “movabs” instruction).
void PrintImmediateArguments(const MarkedInstruction& instruction) {
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
      if (operand->enabled && enabled(kParseOperands))
        fprintf(out_file, " @operand%" NACL_PRIuS "_jmp_to", operand_index - 1);
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

// PrintInstructionsEndActions prints actions which must be triggered at the
// end of instruction.
//
// This function is only used when we don't parse all the operands: in this
// case we can not just use uniform instruction-independent action.  We use
// one of @process_0_operands, @process_1_operand, or @process_2_operands
// and if instruction stores something to 32bit register we add _zero_extends
// to the name of action (e.g.: @process_1_operand_zero_extends).
void PrintInstructionsEndActions(const MarkedInstruction& instruction,
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
            (!IsMemoryCapableOperand(operand->type) ||
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

// PrintOneSizeDefinitionNoModRM prints full definition of one single
// instruction which does not include “ModR/M byte”.
//
// The only twist here is the 0x90 opcode: it requires special handling
// because all combinations except straight “0x90” and “0x48 0x90” are
// handled as normal “xchg” instruction, but “0x90” and “0x48 0x90” are
// treated specially to make sure there are one-byte “nop” in the
// instruction set.
void PrintOneSizeDefinitionNoModRM(const MarkedInstruction& instruction) {
  PrintOperatorDelimiter();
  std::vector<std::string> opcodes = instruction.opcodes();
  if (opcodes[0] == "(0x90|0x91|0x92|0x93|0x94|0x95|0x96|0x97)")
    fprintf(out_file, "((");
  PrintLegacyPrefixes(instruction);
  PrintREXPrefix(instruction);
  PrintMainOpcodePart(instruction);
  if (instruction.signature_after_imm()) {
    PrintOpcodeInImmediate(instruction);
    PrintSignature(instruction, false);
  } else {
    PrintSignature(instruction, false);
    PrintImmediateArguments(instruction);
  }
  if (opcodes[0] == "(0x90|0x91|0x92|0x93|0x94|0x95|0x96|0x97)")
    fprintf(out_file, ") - (0x90|0x48 0x90))");
  PrintInstructionsEndActions(instruction, false);
}

// PrintOneSizeDefinitionModRMRegister prints full definition of one signle
// form which does include “ModR/M byte” but which is not used to access
// memory.
//
// This function should handle two corner cases: when field “reg” in
// “ModR/M byte” is used to extend opcode and when “imm” field is used to
// extend opcode.  These two cases are never combined.
void PrintOneSizeDefinitionModRMRegister(const MarkedInstruction& instruction) {
  PrintOperatorDelimiter();
  MarkedInstruction adjusted_instruction = instruction;
  const std::vector<MarkedInstruction::Operand>& operands =
    adjusted_instruction.operands();
  if (IsModRMRegUsed(adjusted_instruction))
    for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
           operands.begin(); operand != operands.end(); ++operand)
      if (operand->type == 'C' || operand->type == 'D' ||
          operand->type == 'G' || operand->type == 'V') {
        adjusted_instruction = adjusted_instruction.set_rex_r();
        break;
      }
  if (IsModRMRMIsUsed(adjusted_instruction))
    for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
           operands.begin(); operand != operands.end(); ++operand)
      if (operand->type == 'E' ||
          operand->type == 'M' || operand->type == 'R' ||
          operand->type == 'U' || operand->type == 'W') {
        adjusted_instruction = adjusted_instruction.set_rex_b();
        break;
      }
  PrintLegacyPrefixes(adjusted_instruction);
  PrintREXPrefix(adjusted_instruction);
  PrintMainOpcodePart(adjusted_instruction);
  if (!adjusted_instruction.signature_after_imm())
    PrintSignature(adjusted_instruction, false);
  fprintf(out_file, " modrm_registers");
  if (enabled(kParseOperands)) {
    size_t operand_index = 0;
    for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
           operands.begin(); operand != operands.end(); ++operand) {
      const char* operand_type;
      switch (operand->type) {
        case 'C':
        case 'D':
        case 'G':
        case 'V':
          operand_type = "reg";
          break;
        case 'P':
        case 'S':
          operand_type = "reg_norex";
          break;
        case 'E':
        case 'M':
        case 'R':
        case 'U':
        case 'W':
           operand_type = "rm";
           break;
        case 'N':
        case 'Q':
           operand_type = "rm_norex";
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
  const std::vector<std::string>& opcodes = adjusted_instruction.opcodes();
  if (!adjusted_instruction.signature_after_imm() &&
      (adjusted_instruction.signature_after_modrm() ||
       (enabled(kInstructionName) &&
        !ia32_mode &&
        (opcodes.size() < 3 ||
         (opcodes[0] != "0xc4" &&
          (opcodes[0] != "0x8f" || opcodes[1] == "/0"))))))
    fprintf(out_file, ")");
  if (adjusted_instruction.signature_after_imm()) {
    PrintOpcodeInImmediate(adjusted_instruction);
    PrintSignature(adjusted_instruction, false);
  } else {
    PrintImmediateArguments(adjusted_instruction);
  }
  PrintInstructionsEndActions(adjusted_instruction, false);
}

// PrintOneSizeDefinitionModRMMemory prints full definition of one single
// form which does include “ModR/M byte” which is used to access memory.
//
// This is the most complicated and expensive variant to parse because there
// are so many variants: with and without base, without and without index and,
// accordingly, with zero, one, or two bits used from REX/VEX byte.
//
// Additionally function should handle two corner cases: when field “reg” in
// “ModR/M byte” is used to extend opcode and when “imm” field is used to
// extend opcode.  These two cases are never combined.
void PrintOneSizeDefinitionModRMMemory(const MarkedInstruction& instruction) {
  static const struct { const char* mode; bool rex_x; bool rex_b; } modes[] = {
    { " operand_disp",           false, true  },
    { " operand_rip",            false, false },
    { " single_register_memory", false, true  },
    { " operand_sib_pure_index", true,  false },
    { " operand_sib_base_index", true,  true  }
  };
  for (size_t mode = 0; mode < sizeof modes/sizeof modes[0]; ++mode) {
    PrintOperatorDelimiter();
    MarkedInstruction adjusted_instruction = instruction;
    const std::vector<MarkedInstruction::Operand>& operands =
      adjusted_instruction.operands();
    if (IsModRMRegUsed(adjusted_instruction))
      for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
             operands.begin(); operand != operands.end(); ++operand)
        if (operand->type == 'C' || operand->type == 'D' ||
            operand->type == 'G' || operand->type == 'V') {
          adjusted_instruction = adjusted_instruction.set_rex_r();
          break;
        }
    if (IsModRMRMIsUsed(adjusted_instruction)) {
      if (modes[mode].rex_x)
        adjusted_instruction = adjusted_instruction.set_rex_x();
      if (modes[mode].rex_b)
        adjusted_instruction = adjusted_instruction.set_rex_b();
    }
    PrintLegacyPrefixes(adjusted_instruction);
    PrintREXPrefix(adjusted_instruction);
    PrintMainOpcodePart(adjusted_instruction);
    if (!adjusted_instruction.signature_after_imm())
      PrintSignature(adjusted_instruction, true);
    const std::vector<std::string>& opcodes = adjusted_instruction.opcodes();
    if (!adjusted_instruction.signature_after_imm() &&
        (adjusted_instruction.signature_after_modrm() ||
         (enabled(kInstructionName) &&
          !ia32_mode &&
          (opcodes.size() < 3 ||
           (opcodes[0] != "0xc4" &&
            (opcodes[0] != "0x8f" || opcodes[1] == "/0"))))))
      fprintf(out_file, " any");
    else
      fprintf(out_file, " (any");
    if (enabled(kParseOperands)) {
      size_t operand_index = 0;
      for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
             operands.begin(); operand != operands.end(); ++operand) {
        const char* operand_type;
        switch (operand->type) {
          case 'C':
          case 'D':
          case 'G':
          case 'V':
            operand_type = "from_modrm_reg";
            break;
          case 'P':
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
    fprintf(out_file, " any* &%s", modes[mode].mode);
    if (enabled(kCheckAccess) &&
        !adjusted_instruction.has_flag("no_memory_access"))
      fprintf(out_file, " @check_access");
    fprintf(out_file, ")");
    if (adjusted_instruction.signature_after_imm()) {
      PrintOpcodeInImmediate(adjusted_instruction);
      PrintSignature(adjusted_instruction, true);
    } else {
      PrintImmediateArguments(adjusted_instruction);
    }
    PrintInstructionsEndActions(adjusted_instruction, true);
  }
}

// ExpandOperandSizeStrings converts sizes from AMD/Intel manual format to
// concrete and human-readable format.
//
// That is: instead of, e.g., “pqwx” we get either “ymm” (if operand in question
// is register) or “256bit” (if operand in question is memory).  All functions
// “downstream” (that is: above this line) use this description.
MarkedInstruction ExpandOperandSizeStrings(const MarkedInstruction& instruction,
                                           bool memory_access) {
  bool depends_on_mod = false;
  bool memory_operands = false;
  std::vector<MarkedInstruction::Operand> operands =
    instruction.operands();
  for (std::vector<MarkedInstruction::Operand>::iterator operand =
         operands.begin(); operand != operands.end(); ++operand) {
    struct { const char type, *size, *register_size, *memory_size; }
                                                             operand_sizes[] = {
      // BEGIN: 8/16/32/64bits - mostly GP registers.
      { ' ', "b",    "8bit",    "8bit"       },
      // “Sw”, “as”, “bs”, “cs”, “ds”, “fs”, and “gs” are segment registers.
      { 'a', "s",    "segreg",  NULL         },
      { 'b', "s",    "segreg",  NULL         },
      { 'c', "s",    "segreg",  NULL         },
      { 'd', "s",    "segreg",  NULL         },
      { 'f', "s",    "segreg",  NULL         },
      { 'g', "s",    "segreg",  NULL         },
      { 'S', "w",    "segreg",  NULL         },
      { ' ', "w",    "16bit",   "16bit"      },
      { ' ', "d",    "32bit",   "32bit"      },
      // q may be %rXX, %mmX or %xmmX.
      { 'N', "q",    "mmx",     "64bit"      },
      { 'P', "q",    "mmx",     "64bit"      },
      { 'Q', "q",    "mmx",     "64bit"      },
      { 'U', "q",    "xmm",     "64bit"      },
      { 'V', "q",    "xmm",     "64bit"      },
      { 'W', "q",    "xmm",     "64bit"      },
      { ' ', "q",    "64bit",   "64bit"      },
      // END: 8/16/32/64bits.

      // BEGIN: GP registers & memory/control registers/debug registers.
      // Control registers.
      { 'C', "r",    "creg",    NULL         },
      // Debug registers.
      { 'D', "r",    "dreg",    NULL         },
      // Test registers.
      { 'T', "r",    "treg",    NULL         },
      // 32bit register in ia32 mode, 64bit register in amd64 mode.
      { ' ', "r",    "regsize", "regsize"    },
      // END: GP registers & memory/control registers/debug registers.

      // BEGIN: XMM registers/128bit memory.
      { ' ', "",     "xmm",     "128bit"     },
      { ' ', "o",    "xmm",     "128bit"     },
      { ' ', "dq",   "xmm",     "128bit"     },
      // END: XMM registers/128bit memory.

      // BEGIN: YMM registers/256bit memory.
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
      // END: YMM registers/256bit memory.

      // BEGIN: MMX/XXM registers, 64bit/128bit in memory.
#define MMX_XMM_registers(size)                                                \
      { 'a', size,   "xmm",     NULL         }, /* %xmm0 */                    \
      { 'H', size,   "xmm",     NULL         },                                \
      { 'L', size,   "xmm",     NULL         },                                \
      { 'M', size,   NULL,      "128bit"     },                                \
      { 'N', size,   "mmx",     NULL         },                                \
      { 'P', size,   "mmx",     NULL         },                                \
      { 'Q', size,   "mmx",     "64bit"      },                                \
      { 'U', size,   "xmm",     NULL         },                                \
      { 'V', size,   "xmm",     NULL         },                                \
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
      // END: MMX/XXM registers, 64bit/128bit in memory.

      // BEGIN: XMM registers, floating point operands in memory.
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
      // END: XMM registers, floating point operands in memory.

      // 2bit immediate.
      { ' ', "2",    "2bit",    NULL         },

      // x87 register.
      { ' ', "7",    "x87",     NULL         },

      // BEGIN: Various structures in memory.
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
      // END: Various structures in memory.
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
          depends_on_mod |= memory_ne_register;
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
  if (enabled(kInstructionName)) {
    const std::set<std::string>& flags = instruction.flags();
    for (std::set<std::string>::const_iterator flag = flags.begin();
         flag != flags.end(); ++flag)
      if (memory_access &&
          strncmp(flag->c_str(), "att-show-memory-suffix-", 23) == 0)
        depends_on_mod = true;
  }
  if (depends_on_mod &&
      !instruction.signature_after_modrm() &&
      !instruction.signature_after_imm()) {
    std::vector<std::string> opcodes = instruction.opcodes();
    opcodes.push_back(memory_operands ? "/m" : "/r");
    return instruction.set_opcode_in_modrm().with_operands(operands).
           with_opcodes(opcodes);
  } else {
    return instruction.with_operands(operands);
  }
}

// PrintOneSizeDefinition prints definition for one single operands size.
//
// Note that 64bit operands are illegal in ia32 mode (duh).  This function
// does not print anything in this case.
void PrintOneSizeDefinition(const MarkedInstruction& instruction) {
  // 64bit commands are not supported in ia32 mode.
  if (ia32_mode && instruction.rex_w_required()) return;

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
        // Fallthrough.
      case 'M':
        // Two different operands can not use “r/m” field in “ModR/M” byte.
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
        // Two different operands can not use “reg” field in “ModR/M” byte.
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
        ExpandOperandSizeStrings(instruction, true);
      PrintOneSizeDefinitionModRMMemory(instruction_with_expanded_sizes);
      if (instruction.has_flag("lock"))
        PrintOneSizeDefinitionModRMMemory(
          instruction_with_expanded_sizes.add_required_prefix("lock"));
    }
    if (modrm_register)
      PrintOneSizeDefinitionModRMRegister(
        ExpandOperandSizeStrings(instruction, false));
  } else {
    PrintOneSizeDefinitionNoModRM(ExpandOperandSizeStrings(instruction, false));
  }
}

// PrintInstructionPXSplit prints “xmm” and “ymm” versions of the instructions.
//
// If there are no “L” bit then it just calls PrintOneSizeDefinition.
void PrintInstructionPXSplit(const MarkedInstruction& instruction) {
  std::vector<MarkedInstruction::Operand> operands =
    instruction.operands();
  bool found_splittable_operand = false;
  bool found_memory_splittable_operand = false;
  bool found_nonmemory_splittable_operand = false;
  for (std::vector<MarkedInstruction::Operand>::const_iterator operand =
         operands.begin(); operand != operands.end(); ++operand)
    if ((*operand->size.rbegin() == 'x') &&
        ((operand->size.length() == 1) || (*operand->size.begin() == 'p'))) {
      found_splittable_operand = true;
      if (IsMemoryCapableOperand(operand->type) ||
          operand->type == 'o' || operand->type == 'X' || operand->type == 'Y')
        found_memory_splittable_operand = true;
      else if (operand->type != 'I')
        found_nonmemory_splittable_operand = true;
    }
  if (found_splittable_operand) {
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
        if (found_memory_splittable_operand &&
            !found_nonmemory_splittable_operand) {
          opcode->at(++Lbit) = '0';
          PrintOneSizeDefinition(instruction.
                                 with_flag("att-show-memory-suffix-x").
                                 with_opcodes(opcodes).
                                 with_operands(operands));
          opcode->at(Lbit) = '1';
          PrintOneSizeDefinition(instruction.
                                 with_flag("att-show-memory-suffix-y").
                                 with_opcodes(opcodes));
        } else {
          opcode->at(++Lbit) = '0';
          PrintOneSizeDefinition(instruction.
                                 with_opcodes(opcodes).
                                 with_operands(operands));
          opcode->at(Lbit) = '1';
          PrintOneSizeDefinition(instruction.with_opcodes(opcodes));
        }
        return;
      }
    }
    fprintf(stderr, "%s: error - can not set 'L' bit in instruction '%s'",
            short_program_name, instruction.name().c_str());
    exit(1);
  }
  PrintOneSizeDefinition(instruction);
}

// PrintInstructionVYZSplit splits 16bit/32bit/64bit versions of instruction.
//
// If there are no “z” (16bit/32bit) operands, “y” (32bit/64bit) operands, or
// “v” (16bit/32bit/64bit) operands then it calls PrintInstructionPXSplit twice:
// once with W cleared and once with W set.  If flag is “norexw” is used or if
// “rexw” pseudo-prefix is used then this second call does not happen.
void PrintInstructionVYZSplit(const MarkedInstruction& instruction) {
  const std::vector<MarkedInstruction::Operand>& operands =
    instruction.operands();
  bool found_splittable_operand = false;
  bool found_memory_splittable_operand = false;
  bool found_nonmemory_splittable_operand = false;
  for (std::vector<MarkedInstruction::Operand>::const_iterator
         operand = operands.begin(); operand != operands.end(); ++operand)
    if (operand->size == "v" || operand->size == "z") {
      found_splittable_operand = true;
      if (IsMemoryCapableOperand(operand->type) ||
          operand->type == 'o' || operand->type == 'X' || operand->type == 'Y')
        found_memory_splittable_operand = true;
      else if (operand->type != 'I')
        found_nonmemory_splittable_operand = true;
    }
  if (found_splittable_operand) {
    if (found_memory_splittable_operand &&
        !found_nonmemory_splittable_operand) {
      PrintInstructionPXSplit(instruction.with_flag("att-show-memory-suffix-w").
                                          add_required_prefix("data16"));
      PrintInstructionPXSplit(instruction.with_flag("att-show-memory-suffix-l").
                                          clear_rex_w_required());
      MarkedInstruction instruction_w =
        instruction.with_flag("att-show-memory-suffix-q").set_rex_w_required();
      PrintInstructionPXSplit(instruction_w);
    } else {
      PrintInstructionPXSplit(instruction.add_required_prefix("data16"));
      PrintInstructionPXSplit(instruction.clear_rex_w_required());
      MarkedInstruction instruction_w = instruction.set_rex_w_required();
      PrintInstructionPXSplit(instruction_w);
    }
    return;
  }

  for (std::vector<MarkedInstruction::Operand>::const_iterator
         operand = operands.begin(); operand != operands.end(); ++operand)
    if (operand->size == "y") {
      found_splittable_operand = true;
      if (IsMemoryCapableOperand(operand->type) ||
          operand->type == 'o' || operand->type == 'X' || operand->type == 'Y')
        found_memory_splittable_operand = true;
      else if (operand->type != 'I')
        found_nonmemory_splittable_operand = true;
    }
  if (found_splittable_operand) {
    if (found_memory_splittable_operand &&
        !found_nonmemory_splittable_operand) {
      PrintInstructionPXSplit(instruction.with_flag("att-show-memory-suffix-l").
                                          clear_rex_w_required());
      MarkedInstruction instruction_w = instruction.set_rex_w_required();
      PrintInstructionPXSplit(instruction.with_flag("att-show-memory-suffix-q").
                                          set_rex_w_required());
    } else {
      PrintInstructionPXSplit(instruction.clear_rex_w_required());
      MarkedInstruction instruction_w = instruction.set_rex_w_required();
      PrintInstructionPXSplit(instruction.set_rex_w_required());
    }
    return;
  }

  const std::multiset<std::string>& required_prefixes =
    instruction.required_prefixes();
  if (!instruction.has_flag("norex") && !instruction.has_flag("norexw") &&
      !instruction.rex_w_required() &&
      required_prefixes.find("data16") == required_prefixes.end()) {
    PrintInstructionPXSplit(instruction.clear_rex_w());
    PrintInstructionPXSplit(instruction.clear_rex_w().set_rex_w_required());
  } else if (instruction.has_flag("spurious-rex.w")) {
    PrintInstructionPXSplit(instruction.clear_rex_w());
  } else {
    PrintInstructionPXSplit(instruction);
  }
}

// PrintOneInstruction prints definition of one instruction.
//
// It processes instructions one-by-one and does first preliminary split:
// non-marked operands (which means they are 8bit/16bit/32bit/64bit operands)
// are processed as two separate instructions—once as 8bit operand and once
// as 16bit/32bit/64bit operand (16bit/32bit for immediates).
void PrintOneInstruction(const MarkedInstruction& instruction) {
  // Find if there “generic-sized operands” and if they are only memory-capable
  // operands or if some of them reference register-only operands, too.
  std::vector<MarkedInstruction::Operand> operands = instruction.operands();
  bool found_splittable_operand = false;
  bool found_memory_splittable_operand = false;
  bool found_nonmemory_splittable_operand = false;
  for (std::vector<MarkedInstruction::Operand>::const_iterator
         operand = operands.begin(); operand != operands.end(); ++operand) {
    if (operand->size == "") {
      found_splittable_operand = true;
      if (IsMemoryCapableOperand(operand->type) ||
          operand->type == 'o' || operand->type == 'X' || operand->type == 'Y')
        found_memory_splittable_operand = true;
      else if (operand->type != 'I')
        found_nonmemory_splittable_operand = true;
    }
  }

  // If there are “generic-sized operands” then we generate specialized versions
  // with better refined sizes and print them here.
  if (found_splittable_operand) {
    // First we print instructrion with “byte-sized operands” (this does not
    // require modifying the opcode).
    for (std::vector<MarkedInstruction::Operand>::iterator operand =
         operands.begin(); operand != operands.end(); ++operand)
      if (operand->size == "") operand->size = "b";
    if (found_memory_splittable_operand && !found_nonmemory_splittable_operand)
      PrintInstructionVYZSplit(instruction.
                               with_flag("att-show-memory-suffix-b").
                               with_operands(operands));
    else
      PrintInstructionVYZSplit(instruction.with_operands(operands));

    // Now we print instruction with “word/dword/quadword-sized operands”
    // (this requires setting the least significant bit of the opcode).
    std::vector<std::string> opcodes = instruction.opcodes();
    // TODO(khim): Create a helper function which finds the last “regular”
    // opcode byte.
    // Find the opcode byte to modify.  This is the last byte which does not
    // have a letter “/” i.e. it's the last “regular” byte.
    for (std::vector<std::string>::reverse_iterator opcode = opcodes.rbegin();
         opcode != opcodes.rend(); ++opcode)
      if (opcode->find('/') == opcode->npos) {
        // Last bit of the opcode ('w' bit) which determines the size of the
        // operand must be zero.  In textual hex form it's “0”, “2”, “4”, “6”,
        // “8”, "a"/“A”, “c”/“C”, or “e”/“E”.
        if (strchr("02468aceACE", *(opcode->rbegin()))) {
          // Toggle the 'w' bit.
          *(opcode->rbegin()) += 0x1;
        } else {
          fprintf(stderr, "%s: error - can not change the opcode size: %s",
                  short_program_name, opcode->c_str());
          exit(1);
        }
        break;
      }
    // Find the operands with an empty “size” value.  “I” (immediates) need to
    // become “z” (“word/dword” size) while all other operands are becoming “v”
    // (“word/dword/quadword” size).
    operands = instruction.operands();
    for (std::vector<MarkedInstruction::Operand>::iterator
         operand = operands.begin(); operand != operands.end(); ++operand)
      if (operand->size == "") {
        if (operand->type == 'I')
          operand->size = "z";
        else
          operand->size = "v";
      }
    PrintInstructionVYZSplit(instruction.
                             with_opcodes(opcodes).
                             with_operands(operands));
  } else {
    // No “generic-sized operands”, just print one definition here.
    PrintInstructionVYZSplit(instruction);
  }
}

// PrintInstructionComment prints comment with “cannonical” representaion of
// the instruction in a ragel comment.
//
// Canonical here means: all the operands are prefixed with read/write prefix
// (“=”, “&”, “!”, and “'”), all the flags are listed in a sorted order, etc.
void PrintInstructionComment(const Instruction& instruction) {
  // If this is not the first instruction the we need to finish printing the
  // previous one.
  if (instruction_definition_started) {
    fprintf(out_file, ") |");
    instruction_definition_started = false;
  }
  // Print the comment.
  const std::vector<Instruction::Operand>& operands = instruction.operands();
  const std::vector<std::string>& opcodes = instruction.opcodes();
  const std::set<std::string>& flags = instruction.flags();
  fprintf(out_file, "\n");
  // Instruction name.
  fprintf(out_file, "    # %s", instruction.name().c_str());
  // Operands.
  for (std::vector<MarkedInstruction::Operand>::const_reverse_iterator
         operand = operands.rbegin(); operand != operands.rend(); ++operand) {
    fprintf(out_file, " %c%c%s%s", operand->ReadWriteTextPrefix(),
                                   operand->type,
                                   operand->size.c_str(),
                                   operand->implicit ? "*" : "");
  }
  // Opcodes.
  fprintf(out_file, ",");
  for (std::vector<std::string>::const_iterator opcode = opcodes.begin();
       opcode != opcodes.end(); ++opcode)
    fprintf(out_file, " %s", opcode->c_str());
  // And flags (if they exist).
  if (flags.begin() != flags.end()) {
    fprintf(out_file, ",");
    for (std::set<std::string>::const_iterator flag = flags.begin();
         flag != flags.end(); ++flag)
      fprintf(out_file, " %s", flag->c_str());
  }
}

// PrintOneInstructionDefinition prints definition for the “one_instruction”.
void PrintOneInstructionDefinition(void) {
  for (std::vector<Instruction>::const_iterator
         instruction = instructions.begin(); instruction != instructions.end();
         ++instruction) {
    PrintInstructionComment(*instruction);
    PrintOneInstruction(MarkedInstruction(*instruction));
  }
}

#ifndef NDEBUG
// We only run tests if we build gen_dfa in debug mode.  It may be good idea to
// disable optimized builds altogether: it's fast enough even in debug mode and
// speed is of no issue here.

// Compare “file” contents with “expected_output” contents.
void CompareFileContentsToString(FILE* file, const char* expected_output) {
  size_t pos = 0;
  size_t count;
  char buf[1024];
  fflush(file);
  fseek(file, 0, SEEK_SET);
  while ((count = fread(buf, 1, sizeof buf, file)) > 0) {
    assert(strncmp(expected_output + pos, buf, count) == 0);
    pos += count;
  }
  assert(expected_output[pos] == '\0');
  fclose(file);
}

// Run “text_func”, compare output to “out_file” and “const_file” with etalons.
void CompareFileOutput(const char* expected_output_file,
                       void (*test_func)(void)) {
  out_file = tmpfile();

  // Clear this bit: this means we can not really test PrintOperatorDelimiter
  // function in isolation but it's so simple it's pretty hard to break it
  // anyway and it's excercised enough by other tests.
  instruction_definition_started = false;
  test_func();

  CompareFileContentsToString(out_file, expected_output_file);
  out_file = stdout;
}


// Run full generation stack. We want to test some cases end-to-end but we
// don't go overboard with this testing so we are testing couple of exceptional
// cases and one “typical” instruction:
//  • “xchg %reg, %ax/%eax/%rax” vs “nop”: there are special rules WRT this one
//    as described in “NOP in 64-Bit Mode” appendix.
//  • “maskmovq/maskmovdqu/vmaskmovdqu”: these are non-trival and they are
//    encoded with some small changes in validator_x86_64.rl and we want to keep
//    these different versions synchronized
//  * “div !I =R": to check handling of differently-sized operands.

// nop
void test_fullstack_mode_nop(void) {
  std::vector<MarkedInstruction::Operand> operands;
  std::vector<std::string> opcodes;
  std::set<std::string> flags;

  opcodes.push_back("0x90");
  instructions.push_back(Instruction("nop", operands, opcodes, flags));

  CompareFileOutput(
    "\n    # nop, 0x90\n"
    "    (0x90 >begin_opcode @end_opcode @instruction_nop @operands_count_is_0",
    PrintOneInstructionDefinition);

  ia32_mode = false;
  CompareFileOutput(
    "\n    # nop, 0x90\n"
    "    (REX_RXB? "
    "0x90 >begin_opcode @end_opcode @instruction_nop "
    "@operands_count_is_0 @set_spurious_rex_b @set_spurious_rex_x "
    "@set_spurious_rex_r @set_spurious_rex_w) |\n"
    "    (REXW_RXB "
    "0x90 >begin_opcode @end_opcode @instruction_nop "
    "@operands_count_is_0 @set_spurious_rex_b @set_spurious_rex_x "
    "@set_spurious_rex_r @set_spurious_rex_w",
    PrintOneInstructionDefinition);
}

// xchg &av rv
void test_fullstack_mode_xchg(void) {
  std::vector<MarkedInstruction::Operand> operands;
  std::vector<std::string> opcodes;
  std::set<std::string> flags;

  operands.push_back(Instruction::Operand('r', "v", true, true, true, false));
  operands.push_back(Instruction::Operand('a', "v", true, true, true, false));
  opcodes.push_back("0x90");
  instructions.push_back(Instruction("xchg", operands, opcodes, flags));

  CompareFileOutput(
    "\n    # xchg &av &rv, 0x90\n"
    "    (((data16 (0x90|0x91|0x92|0x93|0x94|0x95|0x96|0x97) "
    ">begin_opcode @operand0_from_opcode @end_opcode "
    "@instruction_xchg @operands_count_is_2 "
    "@operand0_16bit @operand1_16bit @operand1_rax "
    "@operand0_readwrite @operand1_readwrite) - (0x90|0x48 0x90))) |\n"
    "    ((((0x90|0x91|0x92|0x93|0x94|0x95|0x96|0x97) "
    ">begin_opcode @operand0_from_opcode @end_opcode "
    "@instruction_xchg @operands_count_is_2 "
    "@operand0_32bit @operand1_32bit @operand1_rax "
    "@operand0_readwrite @operand1_readwrite) - (0x90|0x48 0x90))",
    PrintOneInstructionDefinition);

  ia32_mode = false;
  CompareFileOutput(
    "\n    # xchg &av &rv, 0x90\n"
    "    (((data16 REX_RXB? (0x90|0x91|0x92|0x93|0x94|0x95|0x96|0x97) "
    ">begin_opcode @operand0_from_opcode @end_opcode "
    "@instruction_xchg @operands_count_is_2 @operand0_16bit "
    "@operand1_16bit @operand1_rax @operand0_readwrite @operand1_readwrite "
    "@set_spurious_rex_x @set_spurious_rex_r) - (0x90|0x48 0x90))) |\n"
    "    (((REX_RXB? (0x90|0x91|0x92|0x93|0x94|0x95|0x96|0x97) "
    ">begin_opcode @operand0_from_opcode @end_opcode "
    "@instruction_xchg @operands_count_is_2 @operand0_32bit "
    "@operand1_32bit @operand1_rax @operand0_readwrite @operand1_readwrite "
    "@set_spurious_rex_x @set_spurious_rex_r) - (0x90|0x48 0x90))) |\n"
    "    (((REXW_RXB (0x90|0x91|0x92|0x93|0x94|0x95|0x96|0x97) "
    ">begin_opcode @operand0_from_opcode @end_opcode "
    "@instruction_xchg @operands_count_is_2 @operand0_64bit "
    "@operand1_64bit @operand1_rax @operand0_readwrite @operand1_readwrite "
    "@set_spurious_rex_x @set_spurious_rex_r) - (0x90|0x48 0x90))) |\n"
    "    (((data16 REXW_RXB (0x90|0x91|0x92|0x93|0x94|0x95|0x96|0x97) "
    ">begin_opcode @operand0_from_opcode @end_opcode "
    "@instruction_xchg @operands_count_is_2 "
    "@operand0_64bit @operand1_64bit @operand1_rax @operand0_readwrite "
    "@operand1_readwrite @set_spurious_rex_x @set_spurious_rex_r) - "
    "(0x90|0x48 0x90))",
    PrintOneInstructionDefinition);
}

// maskmovq Nq Pq, 0x0f 0xf7
void test_fullstack_mode_maskmovq(void) {
  std::vector<MarkedInstruction::Operand> operands;
  std::vector<std::string> opcodes;
  std::set<std::string> flags;

  operands.push_back(Instruction::Operand('P', "q", true, true, true, false));
  operands.push_back(Instruction::Operand('N', "q", true, true, false, false));
  opcodes.push_back("0x0f");
  opcodes.push_back("0xf7");
  instructions.push_back(Instruction("maskmovq", operands, opcodes, flags));

  CompareFileOutput(
    "\n    # maskmovq =Nq &Pq, 0x0f 0xf7\n"
    "    ((0x0f 0xf7) >begin_opcode @end_opcode @instruction_maskmovq "
    "@operands_count_is_2 @operand0_mmx @operand1_mmx "
    "@operand0_readwrite @operand1_read modrm_registers "
    "@operand0_from_modrm_reg_norex @operand1_from_modrm_rm_norex",
    PrintOneInstructionDefinition);

  ia32_mode = false;
  CompareFileOutput(
    "\n    # maskmovq =Nq &Pq, 0x0f 0xf7\n"
    "    (REX_RXB? (0x0f 0xf7) >begin_opcode @end_opcode @instruction_maskmovq "
    "@operands_count_is_2 @operand0_mmx @operand1_mmx @operand0_readwrite "
    "@operand1_read (((any - b_00_xxx_100) | (b_00_xxx_100 any)) "
    "@set_spurious_rex_b @set_spurious_rex_x @set_spurious_rex_r "
    "@set_spurious_rex_w any* & modrm_registers @operand0_from_modrm_reg_norex "
    "@operand1_from_modrm_rm_norex)) |\n"
    "    (REXW_RXB (0x0f 0xf7) >begin_opcode @end_opcode @instruction_maskmovq "
    "@operands_count_is_2 @operand0_mmx @operand1_mmx @operand0_readwrite "
    "@operand1_read (((any - b_00_xxx_100) | (b_00_xxx_100 any)) "
    "@set_spurious_rex_b @set_spurious_rex_x @set_spurious_rex_r "
    "@set_spurious_rex_w any* & modrm_registers @operand0_from_modrm_reg_norex "
    "@operand1_from_modrm_rm_norex)",
    PrintOneInstructionDefinition);
}

// maskmovdqu Upb Vpb, 0x66 0x0f 0xf7
void test_fullstack_mode_maskmovdqu(void) {
  std::vector<MarkedInstruction::Operand> operands;
  std::vector<std::string> opcodes;
  std::set<std::string> flags;

  operands.push_back(Instruction::Operand('V', "pb", true, true, true, false));
  operands.push_back(Instruction::Operand('U', "pb", true, true, false, false));
  opcodes.push_back("0x66");
  opcodes.push_back("0x0f");
  opcodes.push_back("0xf7");
  instructions.push_back(Instruction("maskmovdqu", operands, opcodes, flags));

  CompareFileOutput(
    "\n    # maskmovdqu =Upb &Vpb, 0x66 0x0f 0xf7\n"
    "    (0x66 (0x0f 0xf7) >begin_opcode @end_opcode @not_data16_prefix "
    "@instruction_maskmovdqu @operands_count_is_2 "
    "@operand0_xmm @operand1_xmm @operand0_readwrite @operand1_read "
    "modrm_registers @operand0_from_modrm_reg @operand1_from_modrm_rm",
    PrintOneInstructionDefinition);

  ia32_mode = false;
  CompareFileOutput(
    "\n    # maskmovdqu =Upb &Vpb, 0x66 0x0f 0xf7\n  "
    "  (0x66 REX_RXB? (0x0f 0xf7) >begin_opcode @end_opcode @not_data16_prefix "
    "@instruction_maskmovdqu @operands_count_is_2 @operand0_xmm @operand1_xmm "
    "@operand0_readwrite @operand1_read (((any - b_00_xxx_100) | (b_00_xxx_100 "
    "any)) @set_spurious_rex_x @set_spurious_rex_w any* & modrm_registers "
    "@operand0_from_modrm_reg @operand1_from_modrm_rm)) |\n  "
    "  (0x66 REXW_RXB (0x0f 0xf7) >begin_opcode @end_opcode @not_data16_prefix "
    "@instruction_maskmovdqu @operands_count_is_2 @operand0_xmm @operand1_xmm "
    "@operand0_readwrite @operand1_read (((any - b_00_xxx_100) | (b_00_xxx_100 "
    "any)) @set_spurious_rex_x @set_spurious_rex_w any* & modrm_registers "
    "@operand0_from_modrm_reg @operand1_from_modrm_rm)",
    PrintOneInstructionDefinition);
}

// vmaskmovdqu Upb Vpb, 0xc4 RXB.00001 x.1111.0.01 0xf7
void test_fullstack_mode_vmaskmovdqu(void) {
  std::vector<MarkedInstruction::Operand> operands;
  std::vector<std::string> opcodes;
  std::set<std::string> flags;

  operands.push_back(Instruction::Operand('V', "pb", true, true, true, false));
  operands.push_back(Instruction::Operand('U', "pb", true, true, false, false));
  opcodes.push_back("0xc4");
  opcodes.push_back("RXB.00001");
  opcodes.push_back("x.1111.0.01");
  opcodes.push_back("0xf7");
  instructions.push_back(Instruction("vmaskmovdqu", operands, opcodes, flags));

  CompareFileOutput(
    "\n    # vmaskmovdqu =Upb &Vpb, 0xc4 RXB.00001 x.1111.0.01 0xf7\n"
    "    ((((0xc4 (VEX_NONE & VEX_map00001)  b_0_1111_0_01 @vex_prefix3) | "
    "(0xc5  b_1_1111_0_01 @vex_prefix_short)) 0xf7) >begin_opcode @end_opcode "
    "@instruction_vmaskmovdqu @operands_count_is_2 @operand0_xmm @operand1_xmm "
    "@operand0_readwrite @operand1_read "
    "modrm_registers @operand0_from_modrm_reg @operand1_from_modrm_rm",
    PrintOneInstructionDefinition);

  ia32_mode = false;
  CompareFileOutput(
    "\n    # vmaskmovdqu =Upb &Vpb, 0xc4 RXB.00001 x.1111.0.01 0xf7\n"
    "    ((((0xc4 (VEX_RB & VEX_map00001)  b_0_1111_0_01 @vex_prefix3) | "
    "(0xc5  b_X_1111_0_01 @vex_prefix_short)) 0xf7) >begin_opcode @end_opcode "
    "@instruction_vmaskmovdqu @operands_count_is_2 @operand0_xmm @operand1_xmm "
    "@operand0_readwrite @operand1_read "
    "modrm_registers @operand0_from_modrm_reg @operand1_from_modrm_rm) |\n"
    "    ((((0xc4 (VEX_RB & VEX_map00001)  b_0_1111_0_01 @vex_prefix3) | "
    "(0xc5  b_X_1111_0_01 @vex_prefix_short)) 0xf7) >begin_opcode @end_opcode "
    "@instruction_vmaskmovdqu @operands_count_is_2 @operand0_xmm @operand1_xmm "
    "@operand0_readwrite @operand1_read "
    "modrm_registers @operand0_from_modrm_reg @operand1_from_modrm_rm",
    PrintOneInstructionDefinition);
}

// div !I =R, 0xf6 /6
void test_fullstack_mode_div(void) {
  std::vector<MarkedInstruction::Operand> operands;
  std::vector<std::string> opcodes;
  std::set<std::string> flags;

  operands.push_back(Instruction::Operand('R', "", true, true, false, false));
  operands.push_back(Instruction::Operand('I', "", true, false, true, false));
  opcodes.push_back("0xf6");
  opcodes.push_back("/6");
  instructions.push_back(Instruction("div", operands, opcodes, flags));

  CompareFileOutput(
    "\n    # div !I =R, 0xf6 /6\n"
    "    (0xf6 >begin_opcode (opcode_6 @end_opcode @instruction_div "
    "@operands_count_is_2 @operand0_8bit @operand1_8bit @operand1_immediate "
    "@operand0_read @operand1_write any* & "
    "modrm_registers @operand0_from_modrm_rm) imm8) |\n"
    "    (data16 0xf7 >begin_opcode (opcode_6 @end_opcode @instruction_div "
    "@operands_count_is_2 @operand0_16bit @operand1_16bit @operand1_immediate "
    "@operand0_read @operand1_write any* & "
    "modrm_registers @operand0_from_modrm_rm) imm16) |\n"
    "    (0xf7 >begin_opcode (opcode_6 @end_opcode @instruction_div "
    "@operands_count_is_2 @operand0_32bit @operand1_32bit @operand1_immediate "
    "@operand0_read @operand1_write any* & "
    "modrm_registers @operand0_from_modrm_rm) imm32",
    PrintOneInstructionDefinition);

  ia32_mode = false;
  CompareFileOutput(
    "\n    # div !I =R, 0xf6 /6\n"
    "    (REX_RXB? 0xf6 >begin_opcode (opcode_6 @end_opcode @instruction_div "
    "@operands_count_is_2 @operand0_8bit @operand1_8bit @operand1_immediate "
    "@operand0_read @operand1_write any* & ((any - b_00_xxx_100) | "
    "(b_00_xxx_100 any)) @set_spurious_rex_x @set_spurious_rex_r "
    "@set_spurious_rex_w any* & modrm_registers @operand0_from_modrm_rm) imm8) "
    "|\n"
    "    (REXW_RXB 0xf6 >begin_opcode (opcode_6 @end_opcode @instruction_div "
    "@operands_count_is_2 @operand0_8bit @operand1_8bit @operand1_immediate "
    "@operand0_read @operand1_write any* & ((any - b_00_xxx_100) | "
    "(b_00_xxx_100 any)) @set_spurious_rex_x @set_spurious_rex_r "
    "@set_spurious_rex_w any* & modrm_registers @operand0_from_modrm_rm) imm8) "
    "|\n    ("
    "data16 REX_RXB? 0xf7 >begin_opcode (opcode_6 @end_opcode @instruction_div "
    "@operands_count_is_2 @operand0_16bit @operand1_16bit @operand1_immediate "
    "@operand0_read @operand1_write any* & ((any - b_00_xxx_100) | "
    "(b_00_xxx_100 any)) @set_spurious_rex_x @set_spurious_rex_r any* & "
    "modrm_registers @operand0_from_modrm_rm) imm16) |\n"
    "    (REX_RXB? 0xf7 >begin_opcode (opcode_6 @end_opcode @instruction_div "
    "@operands_count_is_2 @operand0_32bit @operand1_32bit @operand1_immediate "
    "@operand0_read @operand1_write any* & ((any - b_00_xxx_100) | "
    "(b_00_xxx_100 any)) @set_spurious_rex_x @set_spurious_rex_r any* & "
    "modrm_registers @operand0_from_modrm_rm) imm32) |\n"
    "    (REXW_RXB 0xf7 >begin_opcode (opcode_6 @end_opcode @instruction_div "
    "@operands_count_is_2 @operand0_64bit @operand1_32bit @operand1_immediate "
    "@operand0_read @operand1_write any* & ((any - b_00_xxx_100) | "
    "(b_00_xxx_100 any)) @set_spurious_rex_x @set_spurious_rex_r any* & "
    "modrm_registers @operand0_from_modrm_rm) imm32) |\n    ("
    "data16 REXW_RXB 0xf7 >begin_opcode (opcode_6 @end_opcode @instruction_div "
    "@operands_count_is_2 @operand0_64bit "
    "@operand1_32bit @operand1_immediate @operand0_read @operand1_write any* & "
    "((any - b_00_xxx_100) | (b_00_xxx_100 any)) @set_spurious_rex_x "
    "@set_spurious_rex_r any* & modrm_registers @operand0_from_modrm_rm) imm32",
    PrintOneInstructionDefinition);
}

void RunTest(const char *test_name, void (*test_func)(void)) {
  printf("Running %s...\n", test_name);
  test_func();

  // Return all the global variables back to pristine state.  Note: we are
  // returning *everything* back even if some variables are not touched by
  // tests.
  instruction_names.clear();
  instructions.clear();
  ia32_mode = true;
  memset(disabled_actions, 0, sizeof(disabled_actions));
  instruction_definition_started = false;
  out_file = stdout;
  out_file_name = NULL;
}

#define RUN_TEST(test_func) (RunTest(#test_func, test_func))

void TestMain() {
  RUN_TEST(test_fullstack_mode_nop);
  RUN_TEST(test_fullstack_mode_xchg);
  RUN_TEST(test_fullstack_mode_maskmovq);
  RUN_TEST(test_fullstack_mode_maskmovdqu);
  RUN_TEST(test_fullstack_mode_vmaskmovdqu);
  RUN_TEST(test_fullstack_mode_div);
}
#endif /* NDEBUG */

} // namespace

int main(int argc, char* argv[]) {
  // basename(3) may change the passed argument thus we are using copy
  // of argv[0].  This creates tiny memory leak but since we only do that
  // once per program invocation it's contained.
  short_program_name = basename(strdup(argv[0]));

  current_dir_name = get_current_dir_name();
  size_t current_dir_name_len = strlen(current_dir_name);

#ifndef NDEBUG
  // Run self-test before doing anything else.
  TestMain();
#endif

  for (;;) {
    int option_index;

    int option = getopt_long(argc, argv, "c:d:hm:o:v",
                             kProgramOptions, &option_index);

    if (option == -1) {
      break;
    }

    switch (option) {
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
        if (strcmp(optarg, "ia32") == 0) {
          ia32_mode = true;
        } else if (strcmp(optarg, "amd64") == 0) {
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
        // getopt_long already printed an error message.
        return 1;
      default:
        assert(false);
    }
  }

  for (int i = optind; i < argc; ++i) {
    ParseDefFile(argv[i]);
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
    PrintNameActions();
  }

  fprintf(out_file, "\n  one_instruction =");

  PrintOneInstructionDefinition();

  fprintf(out_file, ");\n"
"}%%%%\n"
"");
  return 0;
}
