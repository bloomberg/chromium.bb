/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#ifdef __GNUC__
/* We use operand number formats here.  They are defined in SUSv2 and supported
   on all POSIX systems (including all versions of Linux and MacOS), yet
   -pedantic warns about them.  */
#pragma GCC diagnostic ignored "-Wformat"
/* GCC complains even when we explicitly use empty initializer list.  Can be
   easily fixed with data member initializers, but this requires GCC 4.7+.  */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
/* Default is perfectly valid way to handle missing cases.  Especially if we
   only care about very few non-default ones as often here.  */
#pragma GCC diagnostic ignored "-Wswitch-enum"
#pragma GCC diagnostic error "-Wswitch"
#endif

template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

namespace {
  const char* short_program_name;

  const struct option kProgramOptions[] = {
    {"mode",       required_argument,      NULL,   'm'},
    {"disable",    required_argument,      NULL,   'd'},
    {"output",     required_argument,      NULL,   'o'},
    {"const_file", required_argument,      NULL,   'c'},
    {"help",       no_argument,            NULL,   'h'},
    {"version",    no_argument,            NULL,   'v'},
    {NULL,         0,                      NULL,   0}
  };

  const char kVersion[] = "0.0";

  const char* const kProgramHelp = "Usage: %1$s [OPTION]... [FILE]...\n"
"\n"
"Creates ragel machine which recognizes instructions listed in given files.\n"
"\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"\n"
"Options list:\n"
"  -m, --mode=mode            CPU mode: ia32 for IA32, amd64 for x86-64\n"
"  -d, --disable=action_list  disable actions from the comma-separated list\n"
"  -o, --output=FILE          write result to FILE instead of standard output\n"
"  -c, --const_file=FILE      wrire result of FILE instead of standard output\n"
"  -h, --help                 display this help and exit\n"
"  -v, --version              output version information and exit\n"
"\n"
"Here is the full list of possible action types with short descrion of places\n"
"where they are insered:\n"
"  rex_prefix          triggered when legacy REX prefix is parsed\n"
"    @rex_prefix         inserted after possible REX prefix\n"
"  vex_prefix          triggered when VEX-encoded action is detected\n"
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
"  mark_data_fields    this will make 'data fields' (dispXX, immXX, relXX)\n"
"                        with xxxXX_operand_begin and xxxXX_operand_end\n"
"  check_access        this will check memory access (action is not generated\n"
"                        by %1$s, you need to define it in your program)\n"
"\n"
"  imm_operand_action  generate imm_operand action references, but not\n"
"                        actions themselves\n"
"\n"
"  rel_operand_action  generate rel_operand action references, but not\n"
"                        actions themselves\n"
"\n"
"  nacl-forbidden      generate instructions forbidden for nacl\n"
"\n"
"  parse_nonwrite_registers  parse register operands which are only read\n"
"                              or not touched at all\n"
"  parse_immediate_operands  parse immediate operands\n"
"  parse_relative_operands   parse immediate operands (jumps, calls, etc)\n"
"  parse_x87_operands        parse x87 operands\n"
"  parse_mmx_operands        parse MMX operands\n"
"  parse_xmm_operands        parse XMM operands\n"
"  parse_ymm_operands        parse YMM operands\n"
"  parse_operand_positions   produce corrent numbers of operands (required\n"
"                              for decoding, not important for validation)\n";

  const char* const kVersionHelp = "%1$s %2$s\n"
"Copyright (c) 2012 The Native Client Authors. All rights reserved.\n"
"Use of this source code is governed by a BSD-style license that can be\n"
"found in the LICENSE file.\n";

  enum class Actions {
    kRexPrefix,
    kVexPrefix,
    kInstructionName,
    kOpcode,
    kParseOperands,
    kParseOperandsStates,
    kParseNonWriteRegisters,
    kParseImmediateOperands,
    kParseRelativeOperands,
    kParseX87Operands,
    kParseMMXOperands,
    kParseXMMOperands,
    kParseYMMOperands,
    kParseOperandPositions,
    kMarkDataFields,
    kCheckAccess,
    kImmOperandAction,
    kRelOperandAction,
    kNaClForbidden
  };
  const char* kDisablableActionsList[] = {
    "rex_prefix",
    "vex_prefix",
    "instruction_name",
    "opcode",
    "parse_operands",
    "parse_operands_states",
    "parse_nonwrite_registers",
    "parse_immediate_operands",
    "parse_relative_operands",
    "parse_x87_operands",
    "parse_mmx_operands",
    "parse_xmm_operands",
    "parse_ymm_operands",
    "parse_operand_positions",
    "mark_data_fields",
    "check_access",
    "imm_operand_action",
    "rel_operand_action",
    "nacl-forbidden"
  };
  bool disabled_actions[arraysize(kDisablableActionsList)];

  bool enabled(Actions action) {
    return !disabled_actions[static_cast<int>(action)];
  }

  std::map<std::string, size_t> instruction_names;

  static const std::set<std::string> all_instruction_flags = {
    /* Parsing flags. */
    "branch_hint",
    "condrep",
    "lock",
    "no_memory_access",
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
    "CPUFeature_x87",
    "CPUFeature_XOP",

     /* Flags for enabling/disabling based on architecture and validity.  */
     "ia32",
     "amd64",
     "nacl-ia32-forbidden",
     "nacl-amd64-forbidden",
     "nacl-forbidden"
  };

  class Instruction {
   public:
    static void check_flag_valid(const std::string &flag) {
      if (all_instruction_flags.find(flag) == all_instruction_flags.end()) {
        fprintf(stderr, "%s: unknown flag: '%s'\n",
                short_program_name, flag.c_str());
        exit(1);
      }
    }

    void add_flag(const std::string &flag) {
      check_flag_valid(flag);
      flags.insert(flag);
    }

    bool has_flag(const std::string &flag) {
      check_flag_valid(flag);
      return flags.find(flag) != flags.end();
    }

    std::string name;
    struct Operand {
      char source;
      std::string size;
      bool enabled;
      bool read;
      bool write;
      bool implicit;
    };
    std::vector<Operand> operands;
    std::vector<std::string> opcodes;
    std::set<std::string> flags;
  };
  std::vector<Instruction> instructions;

  FILE *out_file = stdout;
  FILE *const_file = stdout;
  char *out_file_name = NULL;
  char *const_file_name = NULL;

  auto ia32_mode = true;

  std::string read_file(const char *filename) {
    std::string file_content;
    auto file = open(filename, O_RDONLY);
    char buf[1024];
    ssize_t count;

    if (file == -1) {
      fprintf(stderr, "%s: can not open '%s' file (%s)\n",
              short_program_name, filename, strerror(errno));
      exit(1);
    }
    while ((count = read(file, buf, sizeof(buf))) > 0) {
      for (auto it = buf; it < buf + count ; ++it) {
        if (*it != '\r') {
          file_content.push_back(*it);
        } else {
          file_content.push_back('\n');
        }
      }
    }
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

  bool is_eol(char c) {
    return c == '\n';
  }

  bool is_whitespace(char c) {
    return c == ' ' || c == '\t';
  }

  /* Advances the iterator till the next comma or line end, splits the traversed
   * text by whitespace and returns it.  Respects quoted text.
   */
  std::vector<std::string> split_till_comma(
      std::string::const_iterator *it_out,
      const std::string::const_iterator &line_end) {
    std::vector<std::string> ret;
    std::string::const_iterator &it = *it_out;
    std::string str;

    for (; it != line_end; ++it) {
      if (is_whitespace(*it)) {
        if (!str.empty()) {
          ret.push_back(str);
          str.clear();
        }
        it = std::find_if_not(it, line_end, is_whitespace);
      }
      if (*it == ',') {
        break;
      }
      if (*it != '"') {
        str.push_back(*it);
        continue;
      }

      for (++it; *it != '"'; ++it) {
        if (it == line_end) {
          fprintf(stderr, "%s: quoted text reaches end of line: %s\n",
                  short_program_name, str.c_str());
          exit(1);
        }
        str.push_back(*it);
      }
    }
    ret.push_back(str);
    return ret;
  }

  struct extract_operand {
    Instruction* instruction;
    const std::vector<std::string>& operation;

    extract_operand(Instruction *i, const std::vector<std::string> &o)
      : instruction(i), operation(o) {
    }

    void operator()(const std::string &str) {
      Instruction::Operand operand;
      switch (str[0]) {
        case '\'':
          operand = {str[1], str.substr(2), true, false, false, false};
          break;
        case '=':
          operand = {str[1], str.substr(2), true, true, false, false};
          break;
        case '!':
          operand = {str[1], str.substr(2), true, false, true, false};
          break;
        case '&':
          operand = {str[1], str.substr(2), true, true, true, false};
          break;
        default:
          if (&str == &*operation.rbegin()) {
            if (operation.size() <= 3) {
              operand = {str[0], str.substr(1), true, true, true, false};
            } else {
              operand = {str[0], str.substr(1), true, false, true, false};
            }
          } else {
            operand = {str[0], str.substr(1), true, true, false, false};
          }
      }
      if (*(operand.size.rbegin()) == '*') {
        operand.size.resize(operand.size.length() - 1);
      }
      instruction->operands.push_back(operand);
    }
  };

  void load_instructions(const char *filename) {
    const std::string file_content = read_file(filename);
    auto it = file_content.begin();
    while (it != file_content.end()) {
      it = std::find_if_not(it, file_content.end(), is_eol);
      if (it == file_content.end()) {
        return;
      }
      /* If line starts with '#' then it's a comment. */
      if (*it == '#') {
        it = std::find_if(it, file_content.end(), is_eol);
      } else {
        auto line_end = std::find_if(it, file_content.end(), is_eol);
        /* Note: initialization list makes sure flags are toggled to zero.  */
        Instruction instruction { };
        auto operation = split_till_comma(&it, line_end);
        /* Line with just whitespaces is ignored.  */
        if (operation.size() != 0) {
          for_each(operation.rbegin(),
                   operation.rend() - 1,
                   extract_operand(&instruction, operation));
          auto enabled_instruction = true;
          if (*it == ',') {
            ++it;
            instruction.opcodes = split_till_comma(&it, line_end);
            if (*it == ',') {
              ++it;
              auto flags = split_till_comma(&it, line_end);
              for (auto flag_it = flags.begin();
                   flag_it != flags.end(); ++flag_it) {
                instruction.add_flag(*flag_it);
              }
              if (instruction.has_flag("ia32")) {
                if (!ia32_mode) {
                  enabled_instruction = false;
                }
              }
              if (instruction.has_flag("amd64")) {
                if (ia32_mode) {
                  enabled_instruction = false;
                }
              }
              if (instruction.has_flag("nacl-ia32-forbidden")) {
                if (ia32_mode && !enabled(Actions::kNaClForbidden)) {
                  enabled_instruction = false;
                }
              }
              if (instruction.has_flag("nacl-amd64-forbidden")) {
                if (!ia32_mode && !enabled(Actions::kNaClForbidden)) {
                  enabled_instruction = false;
                }
              }
              if (instruction.has_flag("nacl-forbidden")) {
                if (!enabled(Actions::kNaClForbidden)) {
                  enabled_instruction = false;
                }
              }
            }
          }
          if (enabled_instruction) {
            instruction_names[instruction.name = operation[0]] = 0;
            instructions.push_back(instruction);
          }
        }
        it = std::find_if(it, file_content.end(), is_eol);
      }
    }
  }

  std::string chartest(std::vector<bool> v) {
    std::string result;
    auto delimiter = "( ";
    for (int c = 0x00; c <= 0xff; ++c) {
      if (v[c]) {
        char buf[10];
        snprintf(buf, sizeof(buf), "%s0x%02x", delimiter, c);
        result += buf;
        delimiter = " | ";
      }
    }
    return result + " )";
  }
  struct comparator {
    const std::vector<bool> &v;
    const int n;
    comparator(const std::vector<bool> &v_, const int n_) : v(v_), n(n_) { }
    #define DECLARE_OPERATOR(x) \
      const std::vector<bool> operator x (int m) { \
        std::vector<bool> r(256, false); \
        for (int c = 0x00; c <= 0xff; ++c) { \
          if (v[c]) { \
            r[c] = (c & n) x m; \
          } \
        } \
        return r; \
      }
    DECLARE_OPERATOR(==)
    DECLARE_OPERATOR(!=)
    DECLARE_OPERATOR(<=)
    DECLARE_OPERATOR(>=)
    DECLARE_OPERATOR(<)
    DECLARE_OPERATOR(>)
    #undef DECLARE_OPERATOR
  };
  comparator operator& (const std::vector<bool> &v, int n) {
    return comparator(v, n);
  }
  #define DECLARE_OPERATOR(x) \
    const std::vector<bool> operator x (const std::vector<bool> &v1, \
                                        const std::vector<bool> &v2) { \
      std::vector<bool> r(256); \
      for (int c = 0x00; c <= 0xff; ++c) { \
        r[c] = (v1[c] x v2[c]); \
      } \
      return r; \
    }
  DECLARE_OPERATOR(&&)
  DECLARE_OPERATOR(||)
  #undef DECLARE_OPERATOR
  #define chartest(x) (chartest(x).c_str())
  std::vector<bool> c(256, true);

  const std::string& select_name(
      const std::map<std::string, size_t>::value_type& p) {
    return p.first;
  }

  bool compare_names(const std::string& x, const std::string& y) {
    return (x.size() > y.size()) || ((x.size() == y.size()) && x < y);
  }

  void print_consts(void)  {
    if (enabled(Actions::kInstructionName)) {
      std::vector<std::string> names;
      std::transform(instruction_names.begin(), instruction_names.end(),
        std::back_inserter(names),
        select_name);
      std::sort(names.begin(), names.end(), compare_names);
      for (auto name_it = names.begin(); name_it != names.end(); ++name_it) {
        auto &name = *name_it;
        if (instruction_names[name] == 0) {
          for (decltype(name.length()) p = 1; p < name.length(); ++p) {
            auto it = instruction_names.find(std::string(name, p));
            if (it != instruction_names.end()) {
              it->second = 1;
            }
          }
        }
      }
      size_t offset = 0;
      for (auto pair_it = instruction_names.begin();
           pair_it != instruction_names.end(); ++pair_it) {
        auto &pair = *pair_it;
        if (pair.second != 1) {
          pair.second = offset;
          offset += pair.first.length() + 1;
        }
      }
      for (auto name_it = names.begin(); name_it != names.end(); ++name_it) {
        auto &name = *name_it;
        auto offset = instruction_names[name];
        if (offset != 1) {
          for (decltype(name.length()) p = 1; p < name.length(); ++p) {
            auto it = instruction_names.find(std::string(name, p));
            if ((it != instruction_names.end()) && (it->second == 1)) {
              it->second = offset + p;
            }
          }
        }
      }
      offset = 0;
      auto delimiter = "static const char instruction_names[] = {\n  ";
      for (auto pair_it = instruction_names.begin();
           pair_it != instruction_names.end(); ++pair_it) {
        auto &pair = *pair_it;
        if (pair.second == offset) {
          fprintf(const_file, "%s", delimiter);
          for (auto c_it = pair.first.begin();
               c_it != pair.first.end(); ++c_it) {
            auto &c = *c_it;
            fprintf(const_file, "0x%02x, ", static_cast<int>(c));
          }
          fprintf(const_file, "\'\\0\',  /* ");
          fprintf(const_file, "%s", pair.first.c_str());
          offset += pair.first.length() + 1;
          delimiter = " */\n  ";
        }
      }
      fprintf(const_file, " */\n};\n");
    }
    if (enabled(Actions::kParseOperands)) {
      fprintf(const_file, "static const uint8_t index_registers[] = {\n"
"  REG_RAX, REG_RCX, REG_RDX, REG_RBX,\n"
"  REG_RIZ, REG_RBP, REG_RSI, REG_RDI,\n"
"  REG_R8,  REG_R9,  REG_R10, REG_R11,\n"
"  REG_R12, REG_R13, REG_R14, REG_R15\n"
"};\n"
"");
    }
  }

  std::string c_identifier(std::string text) {
    std::string name;
    for (auto c_it = text.begin(); c_it != text.end(); ++c_it) {
      auto c = *c_it;
      if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') ||
          ('0' <= c && c <= '9')) {
        name.push_back(c);
      } else {
        name.push_back('_');
      }
    }
    return name;
  }

  void print_common_decoding(void) {
    if (enabled(Actions::kRelOperandAction)) {
      fprintf(out_file, "  action rel8_operand {\n"
"    SET_OPERAND_NAME(0, JMP_TO);\n"
"    SET_MODRM_BASE(REG_RIP);\n"
"    SET_MODRM_INDEX(NO_REG);\n"
"    SET_MODRM_SCALE(0);\n"
"    SET_DISP_TYPE(DISP8);\n"
"    SET_DISP_PTR(p);\n"
"  }\n"
"  action rel16_operand {\n"
"    SET_OPERAND_NAME(0, JMP_TO);\n"
"    SET_MODRM_BASE(REG_RIP);\n"
"    SET_MODRM_INDEX(NO_REG);\n"
"    SET_MODRM_SCALE(0);\n"
"    SET_DISP_TYPE(DISP16);\n"
"    SET_DISP_PTR(p - 1);\n"
"  }\n"
"  action rel32_operand {\n"
"    SET_OPERAND_NAME(0, JMP_TO);\n"
"    SET_MODRM_BASE(REG_RIP);\n"
"    SET_MODRM_INDEX(NO_REG);\n"
"    SET_MODRM_SCALE(0);\n"
"    SET_DISP_TYPE(DISP32);\n"
"    SET_DISP_PTR(p - 3);\n"
"  }\n"
"");
    }
    fprintf(out_file,
"  action branch_not_taken {\n"
"    SET_BRANCH_NOT_TAKEN(TRUE);\n"
"  }\n"
"  action branch_taken {\n"
"    SET_BRANCH_TAKEN(TRUE);\n"
"  }\n"
"  action data16_prefix {\n"
"    SET_DATA16_PREFIX(TRUE);\n"
"  }\n"
"  action lock_prefix {\n"
"    SET_LOCK_PREFIX(TRUE);\n"
"  }\n"
"  action rep_prefix {\n"
"    SET_REPZ_PREFIX(TRUE);\n"
"  }\n"
"  action repz_prefix {\n"
"    SET_REPZ_PREFIX(TRUE);\n"
"  }\n"
"  action repnz_prefix {\n"
"    SET_REPNZ_PREFIX(TRUE);\n"
"  }\n"
"  action not_data16_prefix {\n"
"    SET_DATA16_PREFIX(FALSE);\n"
"  }\n"
"  action not_lock_prefix0 {\n"
"    SET_OPERAND_NAME(0, GET_OPERAND_NAME(0) | 0x08);\n"
"    SET_LOCK_PREFIX(FALSE);\n"
"  }\n"
"  action not_lock_prefix1 {\n"
"    SET_OPERAND_NAME(1, GET_OPERAND_NAME(1) | 0x08);\n"
"    SET_LOCK_PREFIX(FALSE);\n"
"  }\n"
"  action not_repnz_prefix {\n"
"    SET_REPNZ_PREFIX(FALSE);\n"
"  }\n"
"  action not_repz_prefix {\n"
"    SET_REPZ_PREFIX(FALSE);\n"
"  }\n"
"  action disp8_operand {\n"
"    SET_DISP_TYPE(DISP8);\n"
"    SET_DISP_PTR(p);\n"
"  }\n"
"  action disp32_operand {\n"
"    SET_DISP_TYPE(DISP32);\n"
"    SET_DISP_PTR(p - 3);\n"
"  }\n"
"  action disp64_operand {\n"
"    SET_DISP_TYPE(DISP64);\n"
"    SET_DISP_PTR(p - 7);\n"
"  }\n");
     if (enabled(Actions::kImmOperandAction)) {
       fprintf(out_file, "  action imm2_operand {\n"
"    SET_IMM_TYPE(IMM2);\n"
"    SET_IMM_PTR(p);\n"
"  }\n"
"  action imm8_operand {\n"
"    SET_IMM_TYPE(IMM8);\n"
"    SET_IMM_PTR(p);\n"
"  }\n"
"  action imm8_second_operand {\n"
"    SET_IMM2_TYPE(IMM8);\n"
"    SET_IMM2_PTR(p);\n"
"  }\n"
"  action imm16_operand {\n"
"    SET_IMM_TYPE(IMM16);\n"
"    SET_IMM_PTR(p - 1);\n"
"  }\n"
"  action imm16_second_operand {\n"
"    SET_IMM2_TYPE(IMM16);\n"
"    SET_IMM2_PTR(p - 1);\n"
"  }\n"
"  action imm32_operand {\n"
"    SET_IMM_TYPE(IMM32);\n"
"    SET_IMM_PTR(p - 3);\n"
"  }\n"
"  action imm32_second_operand {\n"
"    SET_IMM2_TYPE(IMM32);\n"
"    SET_IMM2_PTR(p - 3);\n"
"  }\n"
"  action imm64_operand {\n"
"    SET_IMM_TYPE(IMM64);\n"
"    SET_IMM_PTR(p - 7);\n"
"  }\n"
"  action imm64_second_operand {\n"
"    SET_IMM2_TYPE(IMM64);\n"
"    SET_IMM2_PTR(p - 7);\n"
"  }\n"
"");
    }
    if (enabled(Actions::kParseOperands)) {
      if (ia32_mode) {
        fprintf(out_file, "  action modrm_only_base {\n"
"    SET_DISP_TYPE(DISPNONE);\n"
"    SET_MODRM_BASE((*p) & 0x07);\n"
"    SET_MODRM_INDEX(NO_REG);\n"
"    SET_MODRM_SCALE(0);\n"
"  }\n"
"  action modrm_base_disp {\n"
"    SET_MODRM_BASE((*p) & 0x07);\n"
"    SET_MODRM_INDEX(NO_REG);\n"
"    SET_MODRM_SCALE(0);\n"
"  }\n"
"  action modrm_pure_disp {\n"
"    SET_MODRM_BASE(NO_REG);\n"
"    SET_MODRM_INDEX(NO_REG);\n"
"    SET_MODRM_SCALE(0);\n"
"  }\n"
"  action modrm_pure_index {\n"
"    SET_DISP_TYPE(DISPNONE);\n"
"    SET_MODRM_BASE(NO_REG);\n"
"    SET_MODRM_INDEX(index_registers[((*p) & 0x38) >> 3]);\n"
"    SET_MODRM_SCALE(((*p) & 0xc0) >> 6);\n"
"  }\n"
"  action modrm_parse_sib {\n"
"    SET_DISP_TYPE(DISPNONE);\n"
"    SET_MODRM_BASE((*p) & 0x7);\n"
"    SET_MODRM_INDEX(index_registers[((*p) & 0x38) >> 3]);\n"
"    SET_MODRM_SCALE(((*p) & 0xc0) >> 6);\n"
"  }\n");
      } else {
        fprintf(out_file, "  action modrm_only_base {\n"
"    SET_DISP_TYPE(DISPNONE);\n"
"    SET_MODRM_BASE(((*p) & 0x07) |\n"
"                   ((GET_REX_PREFIX() & 0x01) << 3) |\n"
"                   (((~GET_VEX_PREFIX2()) & 0x20) >> 2));\n"
"    SET_MODRM_INDEX(NO_REG);\n"
"    SET_MODRM_SCALE(0);\n"
"  }\n"
"  action modrm_base_disp {\n"
"    SET_MODRM_BASE(((*p) & 0x07) |\n"
"                   ((GET_REX_PREFIX() & 0x01) << 3) |\n"
"                   (((~GET_VEX_PREFIX2()) & 0x20) >> 2));\n"
"    SET_MODRM_INDEX(NO_REG);\n"
"    SET_MODRM_SCALE(0);\n"
"  }\n"
"  action modrm_rip {\n"
"    SET_MODRM_BASE(REG_RIP);\n"
"    SET_MODRM_INDEX(NO_REG);\n"
"    SET_MODRM_SCALE(0);\n"
"  }\n"
"  action modrm_pure_index {\n"
"    SET_DISP_TYPE(DISPNONE);\n"
"    SET_MODRM_BASE(NO_REG);\n"
"    SET_MODRM_INDEX(index_registers[(((*p) & 0x38) >> 3) |\n"
"                                    ((GET_REX_PREFIX() & 0x02) << 2) |\n"
"                                    (((~GET_VEX_PREFIX2()) & 0x40) >> 3)]);\n"
"    SET_MODRM_SCALE(((*p) & 0xc0) >> 6);\n"
"  }\n"
"  action modrm_parse_sib {\n"
"    SET_DISP_TYPE(DISPNONE);\n"
"    SET_MODRM_BASE(((*p) & 0x7) |\n"
"                   ((GET_REX_PREFIX() & 0x01) << 3) |\n"
"                   (((~GET_VEX_PREFIX2()) & 0x20) >> 2));\n"
"    SET_MODRM_INDEX(index_registers[(((*p) & 0x38) >> 3) |\n"
"                                    ((GET_REX_PREFIX() & 0x02) << 2) |\n"
"                                    (((~GET_VEX_PREFIX2()) & 0x40) >> 3)]);\n"
"    SET_MODRM_SCALE(((*p) & 0xc0) >> 6);\n"
"  }\n"
"");
      }
    }
    fprintf(out_file, "\n"
"  # Relative jumps and calls.\n"
"  rel8 = any %s;\n"
"  rel16 = any{2} %s;\n"
"  rel32 = any{4} %s;\n"
"", enabled(Actions::kMarkDataFields) ?
         ">rel8_operand_begin @rel8_operand_end" : "@rel8_operand",
       enabled(Actions::kMarkDataFields) ?
         ">rel16_operand_begin @rel16_operand_end" : "@rel16_operand",
       enabled(Actions::kMarkDataFields) ?
         ">rel32_operand_begin @rel32_operand_end" : "@rel32_operand");
    fprintf(out_file, "\n"
"  # Displacements.\n"
"  disp8                = any %s;\n"
"  disp32       = any{4} %s;\n"
"  disp64       = any{8} %s;\n"
"", enabled(Actions::kMarkDataFields) ?
         ">disp8_operand_begin @disp8_operand_end" : "@disp8_operand",
       enabled(Actions::kMarkDataFields) ?
         ">disp32_operand_begin @disp32_operand_end" : "@disp32_operand",
       enabled(Actions::kMarkDataFields) ?
         ">disp64_operand_begin @disp64_operand_end" : "@disp64_operand");
    fprintf(out_file, "\n"
"  # Immediates.\n"
"  imm2 = %s @imm2_operand;\n"
"", ia32_mode ? chartest((c & 0x8c) == 0x00) : chartest((c & 0x0c) == 0x00));
    fprintf(out_file, "  imm8 = any %s;\n"
"  imm16 = any{2} %s;\n"
"  imm32 = any{4} %s;\n"
"  imm64 = any{8} %s;\n"
"  imm8n2 = any %s;\n"
"  imm16n2 = any{2} %s;\n"
"  imm32n2 = any{4} %s;\n"
"  imm64n2 = any{8} %s;\n"
"", enabled(Actions::kMarkDataFields) ?
         ">imm8_operand_begin @imm8_operand_end" : "@imm8_operand",
       enabled(Actions::kMarkDataFields) ?
         ">imm8_operand_begin @imm16_operand_end" : "@imm16_operand",
       enabled(Actions::kMarkDataFields) ?
         ">imm8_operand_begin @imm32_operand_end" : "@imm32_operand",
       enabled(Actions::kMarkDataFields) ?
         ">imm8_operand_begin @imm64_operand_end" : "@imm64_operand",
       enabled(Actions::kMarkDataFields) ?
         ">imm8_operand_begin @imm8_operand_end" : "@imm8_second_operand",
       enabled(Actions::kMarkDataFields) ?
         ">imm16_operand_begin @imm16_operand_end" : "@imm16_second_operand",
       enabled(Actions::kMarkDataFields) ?
         ">imm32_operand_begin @imm32_operand_end" : "@imm32_second_operand",
       enabled(Actions::kMarkDataFields) ?
         ">imm64_operand_begin @imm64_operand_end" : "@imm64_second_operand");
    fprintf(out_file, "\n"
"  # Different types of operands.\n"
"  operand_sib_base_index = (%2$s . %3$s%1$s) |\n"
"                          (%4$s . any%1$s . disp8) |\n"
"                          (%5$s . any%1$s . disp32);\n"
"", enabled(Actions::kParseOperands) ? " @modrm_parse_sib" : "",
       chartest((c & 0xC0) == 0    && (c & 0x07) == 0x04),
       chartest((c & 0x07) != 0x05),
       chartest((c & 0xC0) == 0x40 && (c & 0x07) == 0x04),
       chartest((c & 0xC0) == 0x80 && (c & 0x07) == 0x04));
    fprintf(out_file, "  operand_sib_pure_index = %2$s . %3$s%1$s . disp32;\n"
"", enabled(Actions::kParseOperands) ? " @modrm_pure_index" : "",
       chartest((c & 0xC0) == 0    && (c & 0x07) == 0x04),
       chartest((c & 0x07) == 0x05));
    fprintf(out_file, "  operand_disp  = (%2$s%1$s . disp8) |\n"
"                 (%3$s%1$s . disp32);\n"
"", enabled(Actions::kParseOperands) ? " @modrm_base_disp" : "",
       chartest((c & 0xC0) == 0x40 && (c & 0x07) != 0x04),
       chartest((c & 0xC0) == 0x80 && (c & 0x07) != 0x04));
    fprintf(out_file, "  # It's pure disp32 in IA32 case, "
            "but offset(%%rip) in x86-64 case.\n"
"  operand_rip = %2$s%1$s . disp32;\n"
"", enabled(Actions::kParseOperands) ?
                           ia32_mode ? " @modrm_pure_disp" : " @modrm_rip" : "",
       chartest((c & 0xC0) == 0   && (c & 0x07) == 0x05));
    fprintf(out_file, "  single_register_memory = %2$s%1$s;\n"
"", enabled(Actions::kParseOperands) ? " @modrm_only_base" : "",
       chartest((c & 0xC0) == 0   && (c & 0x07) != 0x04 &&
                                     (c & 0x07) != 0x05));
    fprintf(out_file, "  modrm_memory = (operand_disp | operand_rip |\n"
"                 operand_sib_base_index | operand_sib_pure_index |\n"
"                 single_register_memory)%1$s;\n"
"  modrm_registers = %2$s;\n"
"", enabled(Actions::kCheckAccess) ? " @check_access" : "",
       chartest((c & 0xC0) == 0xC0));
    fprintf(out_file, "\n"
"  # Operations selected using opcode in ModR/M.\n"
"  opcode_0 = %s;\n"
"  opcode_1 = %s;\n"
"  opcode_2 = %s;\n"
"  opcode_3 = %s;\n"
"  opcode_4 = %s;\n"
"  opcode_5 = %s;\n"
"  opcode_6 = %s;\n"
"  opcode_7 = %s;\n"
"  # Used for segment operations: there only 6 segment registers.\n"
"  opcode_s = %s;\n"
"  # This is used to move operand name detection after first byte of ModRM.\n"
"  opcode_m = any;\n"
"  opcode_r = any;\n"
"", chartest((c & 0x38) == 0x00),
       chartest((c & 0x38) == 0x08),
       chartest((c & 0x38) == 0x10),
       chartest((c & 0x38) == 0x18),
       chartest((c & 0x38) == 0x20),
       chartest((c & 0x38) == 0x28),
       chartest((c & 0x38) == 0x30),
       chartest((c & 0x38) == 0x38),
       chartest((c & 0x38) < 0x30));
    fprintf(out_file, "\n"
"  # Prefixes.\n"
"  data16 = 0x66 @data16_prefix;\n"
"  branch_hint = 0x2e @branch_not_taken | 0x3e @branch_taken;\n"
"  condrep = 0xf2 @repnz_prefix | 0xf3 @repz_prefix;\n"
"  lock = 0xf0 @lock_prefix;\n"
"  rep = 0xf3 @rep_prefix;\n"
"  repnz = 0xf2 @repnz_prefix;\n"
"  repz = 0xf3 @repz_prefix;\n"
"");
    if (enabled(Actions::kRexPrefix)) {
      fprintf(out_file, "\n"
"  # REX prefixes.\n"
"  action rex_prefix {\n"
"    SET_REX_PREFIX(*p);\n"
"  }\n"
"");
    }
    fprintf(out_file, "  REX_NONE = 0x40%1$s;\n"
"  REX_W    = %2$s%1$s;\n"
"  REX_R    = %3$s%1$s;\n"
"  REX_X    = %4$s%1$s;\n"
"  REX_B    = %5$s%1$s;\n"
"  REX_WR   = %6$s%1$s;\n"
"  REX_WX   = %7$s%1$s;\n"
"  REX_WB   = %8$s%1$s;\n"
"  REX_RX   = %9$s%1$s;\n"
"  REX_RB   = %10$s%1$s;\n"
"  REX_XB   = %11$s%1$s;\n"
"  REX_WRX  = %12$s%1$s;\n"
"  REX_WRB  = %13$s%1$s;\n"
"  REX_WXB  = %14$s%1$s;\n"
"  REX_RXB  = %15$s%1$s;\n"
"  REX_WRXB = %16$s%1$s;\n"
"", enabled(Actions::kRexPrefix) ? " @rex_prefix" : "",
       chartest((c & 0xf7) == 0x40),
       chartest((c & 0xfb) == 0x40),
       chartest((c & 0xfd) == 0x40),
       chartest((c & 0xfe) == 0x40),
       chartest((c & 0xf3) == 0x40),
       chartest((c & 0xf5) == 0x40),
       chartest((c & 0xf6) == 0x40),
       chartest((c & 0xf9) == 0x40),
       chartest((c & 0xfa) == 0x40),
       chartest((c & 0xfc) == 0x40),
       chartest((c & 0xf1) == 0x40),
       chartest((c & 0xf2) == 0x40),
       chartest((c & 0xf4) == 0x40),
       chartest((c & 0xf8) == 0x40),
       chartest((c & 0xf0) == 0x40));
    fprintf(out_file, "\n"
"  rex_w    = REX_W    - REX_NONE;\n"
"  rex_r    = REX_R    - REX_NONE;\n"
"  rex_x    = REX_X    - REX_NONE;\n"
"  rex_b    = REX_B    - REX_NONE;\n"
"  rex_wr   = REX_WR   - REX_NONE;\n"
"  rex_wx   = REX_WX   - REX_NONE;\n"
"  rex_wb   = REX_WB   - REX_NONE;\n"
"  rex_rx   = REX_RX   - REX_NONE;\n"
"  rex_rb   = REX_RB   - REX_NONE;\n"
"  rex_xb   = REX_XB   - REX_NONE;\n"
"  rex_wrx  = REX_WRX  - REX_NONE;\n"
"  rex_wrb  = REX_WRB  - REX_NONE;\n"
"  rex_wxb  = REX_WXB  - REX_NONE;\n"
"  rex_rxb  = REX_RXB  - REX_NONE;\n"
"  rex_wrxb = REX_WRXB - REX_NONE;\n"
"  REXW_NONE= 0x48%1$s;\n"
"  REXW_R   = %2$s%1$s;\n"
"  REXW_X   = %3$s%1$s;\n"
"  REXW_B   = %4$s%1$s;\n"
"  REXW_RX  = %5$s%1$s;\n"
"  REXW_RB  = %6$s%1$s;\n"
"  REXW_XB  = %7$s%1$s;\n"
"  REXW_RXB = %8$s%1$s;\n"
"", enabled(Actions::kRexPrefix) ? " @rex_prefix" : "",
       chartest((c & 0xfb) == 0x48),
       chartest((c & 0xfd) == 0x48),
       chartest((c & 0xfe) == 0x48),
       chartest((c & 0xf9) == 0x48),
       chartest((c & 0xfa) == 0x48),
       chartest((c & 0xfc) == 0x48),
       chartest((c & 0xf8) == 0x48));
    if (enabled(Actions::kVexPrefix)) {
      if (ia32_mode) {
        fprintf(out_file, "\n"
"  # VEX/XOP prefix - byte 3.\n"
"  action vex_prefix3 {\n"
"    SET_VEX_PREFIX3(*p);\n"
"  }\n"
"  # VEX/XOP short prefix\n"
"  action vex_prefix_short {\n"
"    /* VEX.R is not used in ia32 mode.  */\n"
"    SET_VEX_PREFIX3(p[0] & 0x7f);\n"
"  }\n"
"");
      } else {
        fprintf(out_file, "\n"
"  # VEX/XOP prefix - byte 2.\n"
"  action vex_prefix2 {\n"
"    SET_VEX_PREFIX2(*p);\n"
"  }\n"
"  # VEX/XOP prefix - byte 3.\n"
"  action vex_prefix3 {\n"
"    SET_VEX_PREFIX3(*p);\n"
"  }\n"
"  # VEX/XOP short prefix\n"
"  action vex_prefix_short {\n"
"    /* This emulates two prefixes case. */\n"
"    SET_VEX_PREFIX2((p[0] & 0x80) | 0x61);\n"
"    SET_VEX_PREFIX3(p[0] & 0x7f);\n"
"  }\n"
"");
      }
    }
    typedef std::pair<const char *, int> T;
    static const T vex_fields[] = {
      T { "NONE",       0xe0 },
      T { "R",          0x60 },
      T { "X",          0xa0 },
      T { "B",          0xc0 },
      T { "RX",         0x20 },
      T { "RB",         0x40 },
      T { "XB",         0x80 },
      T { "RXB",        0x00 }
    };
    for (size_t vex_it = 0; vex_it < arraysize(vex_fields); ++vex_it) {
      auto vex = vex_fields[vex_it];
      fprintf(out_file, "  VEX_%2$s = %3$s%1$s;\n"
"", enabled(Actions::kVexPrefix) && !ia32_mode ? " @vex_prefix2" : "",
       vex.first, chartest((c & vex.second) == vex.second));
    }
    static const T vex_map[] = {
      T { "01",         1       },
      T { "02",         2       },
      T { "03",         3       },
      T { "08",         8       },
      T { "09",         9       },
      T { "0A",         10      },
      T { "00001",      1       },
      T { "00010",      2       },
      T { "00011",      3       },
      T { "01000",      8       },
      T { "01001",      9       },
      T { "01010",      10      },
    };
    for (size_t vex_it = 0; vex_it < arraysize(vex_map); ++vex_it) {
      auto vex = vex_map[vex_it];
      fprintf(out_file, "  VEX_map%1$s = %2$s;\n"
"", vex.first, chartest((c & 0x1f) == vex.second));
    }
    if (enabled(Actions::kOpcode)) {
      fprintf(out_file, "\n"
"  action begin_opcode {\n"
"    begin_opcode = p;\n"
"  }\n"
"  action end_opcode {\n"
"    end_opcode = p;\n"
"  }\n"
"");
    }
    if (enabled(Actions::kParseOperands)) {
      for (auto i = 0 ; i <= 5; ++i) {
        fprintf(out_file, "  action operands_count_is_%1$d {\n"
"    SET_OPERANDS_COUNT(%1$d);\n"
"  }\n"
"", i);
      }
      for (auto i = 0 ; i < 5; ++i) {
        typedef std::pair<const char *, const char *> T;
        static const T sizes[] = {
          T { "2bit",                   "Size2bit"                      },
          T { "8bit",                   "Size8bit"                      },
          T { "16bit",                  "Size16bit"                     },
          T { "32bit",                  "Size32bit"                     },
          T { "64bit",                  "Size64bit"                     },
          T { "128bit",                 "Size128bit"                    },
          T { "256bit",                 "Size256bit"                    },
          T { "float16bit",             "FloatSize16bit"                },
          T { "float32bit",             "FloatSize32bit"                },
          T { "float64bit",             "FloatSize64bit"                },
          T { "float80bit",             "FloatSize80bit"                },
          T { "regsize",                ia32_mode ? "Size32bit" :
                                                    "Size64bit"         },
          T { "x87_16bit",              "X87Size16bit"                  },
          T { "x87_32bit",              "X87Size32bit"                  },
          T { "x87_64bit",              "X87Size64bit"                  },
          T { "x87_bcd",                "X87BCD"                        },
          T { "x87_env",                "X87ENV"                        },
          T { "x87_state",              "X87STATE"                      },
          T { "x87_mmx_xmm_state",      "X87MMXXMMSTATE"                },
          T { "x87",                    "ST"                            },
          T { "mmx",                    "MMX"                           },
          T { "xmm",                    "XMM"                           },
          T { "ymm",                    "YMM"                           },
          T { "farptr",                 "FarPtr"                        },
          T { "segreg",                 "SegmentRegister"               },
          T { "creg",                   "ControlRegister"               },
          T { "dreg",                   "DebugRegister"                 },
          T { "selector",               "Selector"                      }
        };
        for (size_t size_it = 0; size_it < arraysize(sizes); ++size_it) {
          auto size = sizes[size_it];
          fprintf(out_file, "  action operand%1$d_%2$s {\n"
"    SET_OPERAND_TYPE(%1$d, Operand%3$s);\n"
"  }\n"
"", i, size.first, size.second);
        }
        if (ia32_mode) {
          fprintf(out_file, "  action operand%1$d_absolute_disp {\n"
"    SET_OPERAND_NAME(%1$d, REG_RM);\n"
"    SET_MODRM_BASE(NO_REG);\n"
"    SET_MODRM_INDEX(NO_REG);\n"
"    SET_MODRM_SCALE(0);\n"
"  }\n"
"  action operand%1$d_from_opcode {\n"
"    SET_OPERAND_NAME(%1$d, (*p) & 0x7);\n"
"  }\n"
"  action operand%1$d_from_is4 {\n"
"    SET_OPERAND_NAME(%1$d, p[0] >> 4);\n"
"  }\n"
"  action operand%1$d_from_modrm_rm {\n"
"    SET_OPERAND_NAME(%1$d, (*p) & 0x07);\n"
"  }\n"
"  action operand%1$d_from_modrm_reg {\n"
"    SET_OPERAND_NAME(%1$d, ((*p) & 0x38) >> 3);\n"
"  }\n"
"  action operand%1$d_from_modrm_reg_norex {\n"
"    SET_OPERAND_NAME(%1$d, ((*p) & 0x38) >> 3);\n"
"  }\n"
"  action operand%1$d_from_vex {\n"
"    SET_OPERAND_NAME(%1$d, ((~GET_VEX_PREFIX3()) & 0x38) >> 3);\n"
"  }\n"
"", i);
        } else {
          fprintf(out_file, "  action operand%1$d_absolute_disp {\n"
"    SET_OPERAND_NAME(%1$d, REG_RM);\n"
"    SET_MODRM_BASE(NO_REG);\n"
"    SET_MODRM_INDEX(REG_RIZ);\n"
"    SET_MODRM_SCALE(0);\n"
"  }\n"
"  action operand%1$d_from_opcode {\n"
"    SET_OPERAND_NAME(%1$d, ((*p) & 0x7) |\n"
"                            ((GET_REX_PREFIX() & 0x01) << 3) |\n"
"                            (((~GET_VEX_PREFIX2()) & 0x20) >> 2));\n"
"  }\n"
"  action operand%1$d_from_is4 {\n"
"    SET_OPERAND_NAME(%1$d, p[0] >> 4);\n"
"  }\n"
"  action operand%1$d_from_modrm_rm {\n"
"    SET_OPERAND_NAME(%1$d, ((*p) & 0x07) |\n"
"                           ((GET_REX_PREFIX() & 0x01) << 3) |\n"
"                           (((~GET_VEX_PREFIX2()) & 0x20) >> 2));\n"
"  }\n"
"  action operand%1$d_from_modrm_reg {\n"
"    SET_OPERAND_NAME(%1$d, (((*p) & 0x38) >> 3) |\n"
"                           ((GET_REX_PREFIX() & 0x04) << 1) |\n"
"                           (((~GET_VEX_PREFIX2()) & 0x80) >> 4));\n"
"  }\n"
"  action operand%1$d_from_modrm_reg_norex {\n"
"    SET_OPERAND_NAME(%1$d, ((*p) & 0x38) >> 3);\n"
"  }\n"
"  action operand%1$d_from_vex {\n"
"    SET_OPERAND_NAME(%1$d, ((~GET_VEX_PREFIX3()) & 0x78) >> 3);\n"
"  }\n"
"", i);
        }
        static const T types[] = {
          T { "ds_rbx",                 "REG_DS_RBX"    },
          T { "ds_rsi",                 "REG_DS_RSI"    },
          T { "es_rdi",                 "REG_ES_RDI"    },
          T { "immediate",              "REG_IMM"       },
          T { "second_immediate",       "REG_IMM2"      },
          T { "port_dx",                "REG_PORT_DX"   },
          T { "rax",                    "REG_RAX"       },
          T { "rcx",                    "REG_RCX"       },
          T { "rdx",                    "REG_RDX"       },
          T { "rm",                     "REG_RM"        },
          T { "st",                     "REG_ST"        }
        };
        for (size_t type_it = 0; type_it < arraysize(types); ++type_it) {
          auto type = types[type_it];
          fprintf(out_file, "  action operand%1$d_%2$s {\n"
"    SET_OPERAND_NAME(%1$d, %3$s);\n"
"  }\n"
"", i, type.first, type.second);
        }
      }
    }
    if (enabled(Actions::kParseOperandsStates)) {
      for (auto i = 0 ; i < 5; ++i) {
        fprintf(out_file, "  action operand%1$d_unused {\n"
"    SET_OPERAND_READ(%1$d, FALSE);\n"
"    SET_OPERAND_WRITE(%1$d, FALSE);\n"
"  }\n"
"  action operand%1$d_read {\n"
"    SET_OPERAND_READ(%1$d, TRUE);\n"
"    SET_OPERAND_WRITE(%1$d, FALSE);\n"
"  }\n"
"  action operand%1$d_write {\n"
"    SET_OPERAND_READ(%1$d, FALSE);\n"
"    SET_OPERAND_WRITE(%1$d, TRUE);\n"
"  }\n"
"  action operand%1$d_readwrite {\n"
"    SET_OPERAND_READ(%1$d, TRUE);\n"
"    SET_OPERAND_WRITE(%1$d, TRUE);\n"
"  }\n"
"", i);
      }
    }
    for (auto flag_it = all_instruction_flags.begin();
         flag_it != all_instruction_flags.end(); ++flag_it) {
      auto &flag = *flag_it;
      if (!strncmp(flag.c_str(), "CPUFeature_", 11)) {
        fprintf(out_file, "  action %1$s {\n"
"    SET_CPU_FEATURE(%1$s);\n"
"  }\n"
"", flag.c_str());
      }
    }
  }

  void print_name_actions(void) {
    for (auto pair_it = instruction_names.begin();
         pair_it != instruction_names.end(); ++pair_it) {
      auto &pair = *pair_it;
      fprintf(out_file, "  action instruction_%s"
        " { SET_INSTRUCTION_NAME(instruction_names + %zd); }\n",
                                 c_identifier(pair.first).c_str(), pair.second);
    }
  }

  class MarkedInstruction : Instruction {
   public:
    /* Additional marks are created in the process of parsing. */
    explicit MarkedInstruction(Instruction instruction_) :
        Instruction(instruction_),
        instruction_class(get_instruction_class(instruction_)),
        rex { }, opcode_in_modrm(false), opcode_in_imm(false) {
      if (has_flag("branch_hint")) {
        optional_prefixes.insert("branch_hint");
      }
      if (has_flag("condrep")) {
        optional_prefixes.insert("condrep");
      }
      if (has_flag("rep")) {
        optional_prefixes.insert("rep");
      }
      for (auto opcode_it = opcodes.begin();
           opcode_it != opcodes.end(); ++opcode_it) {
        auto opcode = *opcode_it;
        if (opcode == "/") {
          opcode_in_imm = true;
          break;
        } else if (opcode.find('/') != opcode.npos) {
          opcode_in_modrm = true;
          break;
        }
      }
      /* If register is stored in opcode we need to expand opcode now.  */
      for (auto operand_it = operands.begin();
           operand_it != operands.end(); ++operand_it) {
        auto &operand = *operand_it;
        if (operand.source == 'r') {
          auto opcode = opcodes.rbegin();
          for (; opcode != opcodes.rend(); ++opcode) {
            if (opcode->find('/') == opcode->npos) {
              auto saved_opcode = *opcode;
              switch (*(opcode->rbegin())) {
                case '0':
                  (*opcode) = "(";
                  opcode->append(saved_opcode);
                  for (auto c = '1'; c <= '7'; ++c) {
                    opcode->push_back('|');
                    (*(saved_opcode.rbegin())) = c;
                    opcode->append(saved_opcode);
                  }
                  opcode->push_back(')');
                  if (saved_opcode == "0x97") {
                    opcode->erase(1, 5);
                  }
                  break;
                case '8':
                  (*opcode) = "(";
                  opcode->append(saved_opcode);
                  static const char cc[] = {'9', 'a', 'b', 'c', 'd', 'e', 'f'};
                  for (size_t c_it = 0; c_it < arraysize(cc); ++c_it) {
                    auto c = cc[c_it];
                    opcode->push_back('|');
                    (*(saved_opcode.rbegin())) = c;
                    opcode->append(saved_opcode);
                  }
                  opcode->push_back(')');
                  break;
                default:
                  fprintf(stderr, "%s: error - can not use 'r' operand in "
                    "instruction '%s'", short_program_name, name.c_str());
                  exit(1);
              }
              if (operand.size != "7") rex.b = true;
              break;
            }
          }
          if (opcode == opcodes.rend()) {
            fprintf(stderr, "%s: error - can not use 'r' operand in "
              "instruction '%s'", short_program_name, name.c_str());
            exit(1);
          }
          break;
        }
      }
      /* Some 'opcodes' include prefixes, move them there.  */
      static std::set<std::string> opcode_prefixes = {
        "0x66", "data16",
        "0xf0", "lock",
        "0xf2", "repnz",
        "0xf3", "repz",
        "rexw"
      };
      while (opcode_prefixes.find(*opcodes.begin()) != opcode_prefixes.end()) {
        if ((*opcodes.begin()) == "rexw") {
          if (!rex.w && instruction_class == InstructionClass::kDefault) {
            instruction_class = InstructionClass::kRexW;
            rex.w = true;
          } else if (!rex.w && instruction_class == InstructionClass::kSize8) {
            instruction_class = InstructionClass::kSize8RexW;
            rex.w = true;
          } else {
            fprintf(stderr,
                    "%s: error - can not enforce '%drexw' prefix in "
                      "instruction '%s'",
                    short_program_name,
                    instruction_class,
                    name.c_str());
            exit(1);
          }
        } else {
          required_prefixes.insert(*opcodes.begin());
        }
        opcodes.erase(opcodes.begin());
      }
    }

    enum InstructionClass {
      kDefault,
      /* The same as kDefault, but to make 'dil'/'sil'/'bpl'/'spl' accesible
       * pure REX (hex 0x40) is allowed.  */
      kSize8,
      /* One operand is kSize8, another is RexW.  */
      kSize8RexW,
      kData16,
      kRexW,
      /* Combinations: must generate few different lines.  */
      kDefaultRexW,
      kData16DefaultRexW,
      kSize8Data16DefaultRexW,
      kSize8Data16DefaultRexWxDefaultRexW,
      kLSetUnset,
      kLSetUnsetDefaultRexW,
      /* Unknown: if all arguments are unknown the result is kDefault.  */
      kUnknown
    } instruction_class;
    std::multiset<std::string> required_prefixes;
    std::multiset<std::string> optional_prefixes;
    struct {
      bool b : 1;
      bool x : 1;
      bool r : 1;
      bool w : 1;
    } rex;
    bool opcode_in_modrm : 1;
    bool opcode_in_imm : 1;

    static InstructionClass get_instruction_class(
                                               const Instruction &instruction) {
      InstructionClass instruction_class = InstructionClass::kUnknown;
      for (auto operand_it = instruction.operands.begin();
           operand_it != instruction.operands.end(); ++operand_it) {
        auto &operand = *operand_it;
        static const std::map<std::string, InstructionClass> classes_map {
          /* 'size8' is special 'prefix' not included in AMD manual:  w bit in
             opcode switches between 8bit and 16/32/64 bit versions.  M is just
             an address in memory: it means register-only encodings are invalid,
             but other operands decide everything else.  */
          { "",         InstructionClass::kSize8Data16DefaultRexW       },
          { "2",        InstructionClass::kUnknown                      },
          { "7",        InstructionClass::kUnknown                      },
          { "d",        InstructionClass::kUnknown                      },
          { "do",       InstructionClass::kUnknown                      },
          { "dq",       InstructionClass::kUnknown                      },
          { "fq",       InstructionClass::kUnknown                      },
          { "o",        InstructionClass::kUnknown                      },
          { "p",        InstructionClass::kUnknown                      },
          { "pb",       InstructionClass::kUnknown                      },
          { "pd",       InstructionClass::kUnknown                      },
          { "pdw",      InstructionClass::kUnknown                      },
          { "pdwx",     InstructionClass::kLSetUnset                    },
          { "pdx",      InstructionClass::kLSetUnset                    },
          { "ph",       InstructionClass::kUnknown                      },
          { "pi",       InstructionClass::kUnknown                      },
          { "pj",       InstructionClass::kUnknown                      },
          { "pjx",      InstructionClass::kLSetUnset                    },
          { "pk",       InstructionClass::kUnknown                      },
          { "pq",       InstructionClass::kUnknown                      },
          { "pqw",      InstructionClass::kUnknown                      },
          { "pqwx",     InstructionClass::kLSetUnset                    },
          { "ps",       InstructionClass::kUnknown                      },
          { "psx",      InstructionClass::kLSetUnset                    },
          { "pw",       InstructionClass::kUnknown                      },
          { "q",        InstructionClass::kUnknown                      },
          { "r",        InstructionClass::kUnknown                      },
          { "s",        InstructionClass::kUnknown                      },
          { "sb",       InstructionClass::kUnknown                      },
          { "sd",       InstructionClass::kUnknown                      },
          { "se",       InstructionClass::kUnknown                      },
          { "si",       InstructionClass::kUnknown                      },
          { "sq",       InstructionClass::kUnknown                      },
          { "sr",       InstructionClass::kUnknown                      },
          { "ss",       InstructionClass::kUnknown                      },
          { "st",       InstructionClass::kUnknown                      },
          { "sw",       InstructionClass::kUnknown                      },
          { "sx",       InstructionClass::kUnknown                      },
          { "v",        InstructionClass::kData16DefaultRexW            },
          { "w",        InstructionClass::kUnknown                      },
          { "x",        InstructionClass::kLSetUnset                    },
          { "y",        InstructionClass::kDefaultRexW                  },
          { "z",        InstructionClass::kData16DefaultRexW            }
        };
        InstructionClass operand_class;
        auto it = classes_map.find(operand.size);
        if (it != classes_map.end()) {
          operand_class = it->second;
        /* If it's 8bit register specified using ModRM then we mark it as size8
           to make it possible to use REX_NONE for 'dil'/'sil'/'bpl'/'spl'.  */
        } else if (operand.size == "b") {
          if ((operand.source == 'E') ||
              (operand.source == 'G') ||
              (operand.source == 'M') ||
              (operand.source == 'R') ||
              (operand.source == 'r')) {
            operand_class = InstructionClass::kSize8;
          } else {
            operand_class = InstructionClass::kUnknown;
          }
        } else {
          fprintf(stderr, "%s: unknown operand: '%c%s'\n",
                  short_program_name, operand.source, operand.size.c_str());
          exit(1);
        }
        if ((operand_class == InstructionClass::kUnknown) ||
            (instruction_class == operand_class)) {
          {}  /* Do nothing: operand_class does not change anything.  */
        } else if (instruction_class == InstructionClass::kUnknown) {
          instruction_class = operand_class;
        } else if (((instruction_class == InstructionClass::kDefaultRexW) &&
                    (operand_class ==
                                  InstructionClass::kSize8Data16DefaultRexW)) ||
                   ((instruction_class ==
                                   InstructionClass::kSize8Data16DefaultRexW) &&
                    (operand_class == InstructionClass::kDefaultRexW))) {
          instruction_class =
                          InstructionClass::kSize8Data16DefaultRexWxDefaultRexW;
        } else {
          fprintf(stderr, "%s: error - incompatible modes %d & %d\n",
                  short_program_name, instruction_class, operand_class);
          exit(1);
        }
      }
      switch (instruction_class) {
        case InstructionClass::kUnknown:
          return InstructionClass::kDefault;
        default:
          return instruction_class;
      }
    }

    void print_one_size_definition_data16(void) {
      auto saved_prefixes = required_prefixes;
      required_prefixes.insert("data16");
      print_one_size_definition();
      required_prefixes = saved_prefixes;
    }

    void print_one_size_definition_rexw(void) {
      auto saved_rex = rex;
      rex.w = true;
      print_one_size_definition();
      rex = saved_rex;
    }

    void print_definition(void) {
      switch (auto saved_class = instruction_class) {
        case InstructionClass::kDefault:
        case InstructionClass::kSize8:
          print_one_size_definition();
          break;
        case InstructionClass::kSize8RexW:
        case InstructionClass::kRexW:
          print_one_size_definition_rexw();
          break;
        case InstructionClass::kData16:
          print_one_size_definition_data16();
          break;
        case InstructionClass::kDefaultRexW:
          instruction_class = InstructionClass::kDefault;
          print_one_size_definition();
          instruction_class = InstructionClass::kRexW;
          print_one_size_definition_rexw();
          instruction_class = saved_class;
        break;
        case InstructionClass::kData16DefaultRexW:
          instruction_class = InstructionClass::kData16;
          print_one_size_definition_data16();
          instruction_class = InstructionClass::kDefault;
          print_one_size_definition();
          instruction_class = InstructionClass::kRexW;
          print_one_size_definition_rexw();
          instruction_class = saved_class;
          break;
        case InstructionClass::kSize8Data16DefaultRexWxDefaultRexW: {
          /* Here we need to set instruction_class simultaneously to kSize8
             and to kRexW.  We can not do that thus we are cheating: we convert
             all Size8/Size16/Size32/Size64 operands to pure Size8 operands
             right here and leave all other operands to be handled by the
             print_one_size_definition() call.  */
          instruction_class = InstructionClass::kRexW;
          auto saved_operands = operands;
          for (auto operand_it = operands.begin();
               operand_it != operands.end(); ++operand_it) {
            auto &operand = *operand_it;
            if (operand.size == "") {
              operand.size = "b";
            }
          }
          print_one_size_definition_rexw();
          operands = saved_operands;
        } /* Falltrought: the rest is as in kSize8Data16DefaultRexW.  */
        case InstructionClass::kSize8Data16DefaultRexW:
          instruction_class = InstructionClass::kSize8;
          print_one_size_definition();
          for (auto opcode = opcodes.rbegin();
               opcode != opcodes.rend(); ++opcode) {
            if (opcode->find('/') == opcode->npos) {
              /* 'w' bit is last bit both in binary and textual form.  */
              *(opcode->rbegin()) += 0x1;
              instruction_class = InstructionClass::kData16;
              print_one_size_definition_data16();
              instruction_class = InstructionClass::kDefault;
              print_one_size_definition();
              instruction_class = InstructionClass::kRexW;
              print_one_size_definition_rexw();
              instruction_class = saved_class;
              return;
            }
          }
          fprintf(stderr, "%s: error - can not set 'w' bit in "
            "instruction '%s'", short_program_name, name.c_str());
          exit(1);
        case InstructionClass::kLSetUnset:
        case InstructionClass::kLSetUnsetDefaultRexW:
          for (auto opcode_it = opcodes.begin();
               opcode_it != opcodes.end(); ++opcode_it) {
            auto &opcode = *opcode_it;
            auto Lbit = opcode.find(".L.");
            if (Lbit != opcode.npos) {
              opcode[++Lbit] = '1';
              instruction_class = InstructionClass::kDefault;
              print_one_size_definition();
              if (saved_class == InstructionClass::kLSetUnsetDefaultRexW) {
                instruction_class = InstructionClass::kRexW;
                print_one_size_definition_rexw();
              }
              opcode[Lbit] = '0';
              auto saved_operands = operands;
              for (auto operand_it = operands.begin();
                   operand_it != operands.end(); ++operand_it) {
                auto &operand = *operand_it;
                static const char cc[] = {'H', 'L', 'U', 'V', 'W'};
                for (size_t c_it = 0; c_it < arraysize(cc); ++c_it) {
                  auto c = cc[c_it];
                  if ((operand.source == c) &&
                      (*(operand.size.rbegin()) == 'x') &&
                      (((operand.size.length() > 1) &&
                        (operand.size[0] == 'p')) ||
                       (operand.size.length() == 1))) {
                    operand.size.resize(operand.size.length() - 1);
                  }
                }
              }
              instruction_class = InstructionClass::kDefault;
              print_one_size_definition();
              if (saved_class == InstructionClass::kLSetUnsetDefaultRexW) {
                instruction_class = InstructionClass::kRexW;
                print_one_size_definition_rexw();
              }
              operands = saved_operands;
              return;
            }
          }
          fprintf(stderr, "%s: error - can not set 'L' bit in instruction '%s'",
                  short_program_name, name.c_str());
          exit(1);
          break;
        case InstructionClass::kUnknown:
          fprintf(stderr, "%s: error - incorrect operand mode: '%d'",
                  short_program_name, instruction_class);
          exit(1);
      }
    }

    void print_one_size_definition(void) {
      /* 64bit commands are not supported in ia32 mode.  */
      if (ia32_mode && rex.w) {
        return;
      }
      bool modrm_memory = false;
      bool modrm_register = false;
      char operand_source = ' ';
      for (auto operand_it = operands.begin();
           operand_it != operands.end(); ++operand_it) {
        auto &operand = *operand_it;
        static std::map<char, std::pair<bool, bool> > operand_map {
          { 'E', std::make_pair(true,  true) },
          { 'M', std::make_pair(true,  false) },
          { 'N', std::make_pair(false, true) },
          { 'Q', std::make_pair(true,  true) },
          { 'R', std::make_pair(false, true) },
          { 'U', std::make_pair(false, true) },
          { 'W', std::make_pair(true,  true) }
        };
        auto it = operand_map.find(operand.source);
        if (it != operand_map.end()) {
          if ((modrm_memory || modrm_register) &&
              ((modrm_memory != it->second.first) ||
               (modrm_register != it->second.second))) {
            fprintf(stderr,
                    "%s: error - conflicting operand sources: '%c' and '%c'",
                    short_program_name, operand_source, operand.source);
            exit(1);
          }
          modrm_memory = it->second.first;
          modrm_register = it->second.second;
          operand_source = operand.source;
        }
      }
      if (modrm_memory || modrm_register) {
        if (modrm_memory) {
          auto saved_prefixes = optional_prefixes;
          print_one_size_definition_modrm_memory();
          if (has_flag("lock")) {
            auto saved_prefixes = required_prefixes;
            required_prefixes.insert("lock");
            print_one_size_definition_modrm_memory();
            required_prefixes = saved_prefixes;
          }
          optional_prefixes = saved_prefixes;
        }
        if (modrm_register) {
          print_one_size_definition_modrm_register();
        }
      } else {
        print_one_size_definition_nomodrm();
      }
    }

    void print_one_size_definition_nomodrm(void) {
      print_operator_delimiter();
      print_legacy_prefixes();
      print_rex_prefix();
      print_opcode_nomodrm();
      if (opcode_in_imm) {
        print_immediate_opcode();
        print_opcode_recognition(false);
      } else {
        print_opcode_recognition(false);
        print_immediate_arguments();
      }
    }

    void print_one_size_definition_modrm_register(void) {
      print_operator_delimiter();
      if (mod_reg_is_used()) {
        rex.r = true;
      }
      if (mod_rm_is_used()) {
        rex.b = true;
      }
      print_legacy_prefixes();
      print_rex_prefix();
      print_opcode_nomodrm();
      if (!opcode_in_imm) {
        print_opcode_recognition(false);
      }
      fprintf(out_file, " modrm_registers");
      if (enabled(Actions::kParseOperands)) {
        auto operand_index = 0;
        for (auto operand_it = operands.begin();
             operand_it != operands.end(); ++operand_it) {
          auto &operand = *operand_it;
          static const std::map<char, const char*> operand_type {
            { 'C', "reg"       },
            { 'D', "reg"       },
            { 'E', "rm"        },
            { 'G', "reg"       },
            { 'M', "rm"        },
            { 'N', "rm"        },
            { 'P', "reg"       },
            { 'Q', "rm"        },
            { 'R', "rm"        },
            { 'S', "reg_norex" },
            { 'U', "rm"        },
            { 'V', "reg"       },
            { 'W', "rm"        }
          };
          if (operand.enabled) {
            auto it = operand_type.find(operand.source);
            if (it != operand_type.end()) {
              fprintf(out_file, " @operand%zd_from_modrm_%s", operand_index,
                      it->second);
            }
          }
          if (operand.enabled || enabled(Actions::kParseOperandPositions)) {
            ++operand_index;
          }
        }
      }
      if (opcode_in_modrm) {
        fprintf(out_file, ")");
      }
      if (opcode_in_imm) {
        print_immediate_opcode();
        print_opcode_recognition(false);
      } else {
        print_immediate_arguments();
      }
    }

    void print_one_size_definition_modrm_memory(void) {
      typedef std::tuple<const char *, bool, bool> T;
      static const T modes[] = {
        T { " operand_disp",            false,  true    },
        T { " operand_rip",             false,  false   },
        T { " single_register_memory",  false,  true    },
        T { " operand_sib_pure_index",  true,   false   },
        T { " operand_sib_base_index",  true,   true    }
      };
      for (size_t mode_it = 0; mode_it < arraysize(modes); ++mode_it) {
        auto mode = modes[mode_it];
        print_operator_delimiter();
        if (mod_reg_is_used()) {
          rex.r = true;
        }
        if (mod_rm_is_used()) {
          rex.x = std::get<1>(mode);
          rex.b = std::get<2>(mode);
        }
        print_legacy_prefixes();
        print_rex_prefix();
        print_opcode_nomodrm();
        if (!opcode_in_imm) {
          print_opcode_recognition(true);
        }
        if (opcode_in_modrm) {
          fprintf(out_file, " any");
        } else {
          fprintf(out_file, " (any");
        }
        if (enabled(Actions::kParseOperands)) {
          auto operand_index = 0;
          for (auto operand_it = operands.begin();
               operand_it != operands.end(); ++operand_it) {
            auto &operand = *operand_it;
            static const std::map<char, const char*> operand_type {
              { 'C', "from_modrm_reg"         },
              { 'D', "from_modrm_reg"         },
              { 'E', "rm"                     },
              { 'G', "from_modrm_reg"         },
              { 'M', "rm"                     },
              { 'N', "rm"                     },
              { 'P', "from_modrm_reg"         },
              { 'Q', "rm"                     },
              { 'R', "rm"                     },
              { 'S', "from_modrm_reg_norex"   },
              { 'U', "rm"                     },
              { 'V', "from_modrm_reg"         },
              { 'W', "rm"                     }
            };
            if (operand.enabled) {
              auto it = operand_type.find(operand.source);
              if (it != operand_type.end() &&
                  (strcmp(it->second, "rm") ||
                   enabled(Actions::kParseOperandPositions))) {
                fprintf(out_file, " @operand%zd_%s", operand_index, it->second);
              }
            }
            if (operand.enabled || enabled(Actions::kParseOperandPositions)) {
              ++operand_index;
            }
          }
        }
        fprintf(out_file, " . any* &%s", std::get<0>(mode));
        if (enabled(Actions::kCheckAccess) && !has_flag("no_memory_access")) {
          fprintf(out_file, " @check_access");
        }
        fprintf(out_file, ")");
        if (opcode_in_imm) {
          print_immediate_opcode();
          print_opcode_recognition(true);
        } else {
          print_immediate_arguments();
        }
      }
    }

    static bool first_delimiter;
    void print_operator_delimiter(void) {
      if (first_delimiter) {
        fprintf(out_file, "\n    (");
      } else {
        fprintf(out_file, ") |\n    (");
      }
      first_delimiter = false;
    }

    bool mod_reg_is_used() {
      for (auto operand_it = operands.begin();
           operand_it != operands.end(); ++operand_it) {
        auto &operand = *operand_it;
        if (operand.source == 'C' &&
            required_prefixes.find("0xf0") == required_prefixes.end()) {
          return true;
        }
        static const char cc[] = { 'G', 'P', 'V' };
        for (size_t c_it = 0; c_it < arraysize(cc); ++c_it) {
          auto c = cc[c_it];
          if (operand.source == c) {
            return true;
          }
        }
      }
      return false;
    }

    bool mod_rm_is_used() {
      for (auto operand_it = operands.begin();
           operand_it != operands.end(); ++operand_it) {
        auto &operand = *operand_it;
        static const char cc[] = { 'E', 'M', 'N', 'Q', 'R', 'U', 'W' };
        for (size_t c_it = 0; c_it < arraysize(cc); ++c_it) {
          auto c = cc[c_it];
          if (operand.source == c) {
            return true;
          }
        }
      }
      return false;
    }

    void print_legacy_prefixes(void) {
      if ((required_prefixes.size() == 1) &&
          (optional_prefixes.size() == 0)) {
        fprintf(out_file, "%s ", required_prefixes.begin()->c_str());
      } else if ((required_prefixes.size() == 0) &&
                 (optional_prefixes.size() == 1)) {
        fprintf(out_file, "%s? ", optional_prefixes.begin()->c_str());
      } else if ((optional_prefixes.size() > 0) ||
                 (required_prefixes.size() > 0)) {
        auto delimiter = "(";
        auto opt_start = required_prefixes.size() ? 0 : 1;
        auto opt_end = 1 << optional_prefixes.size();
        for (auto opt = opt_start; opt < opt_end; ++opt) {
          auto prefixes = required_prefixes;
          auto it = optional_prefixes.begin();
          for (auto id = 1; id < opt_end; id <<= 1, ++it) {
            if (opt & id) {
              prefixes.insert(*it);
            }
          }
          if (prefixes.size() == 1) {
            fprintf(out_file, "%s%s", delimiter, prefixes.begin()->c_str());
            delimiter = " | ";
          } else {
            std::vector<std::string> permutations(prefixes.begin(),
                                                                prefixes.end());
            do {
              fprintf(out_file, "%s", delimiter);
              delimiter = " | ";
              auto delimiter = '(';
              for (auto prefix_it = permutations.begin();
                   prefix_it != permutations.end(); ++prefix_it) {
                auto &prefix = *prefix_it;
                fprintf(out_file, "%c%s", delimiter, prefix.c_str());
                delimiter = ' ';
              }
              fprintf(out_file, ")");
            } while (next_permutation(permutations.begin(),
                                                           permutations.end()));
          }
        }
        if (opt_start) {
          fprintf(out_file, ")? ");
        } else {
          fprintf(out_file, ") ");
        }
      }
    }

    void print_rex_prefix(void) {
      /* Prefix REX is not used in ia32 mode.  */
      if (ia32_mode) {
        return;
      }
      /* VEX/XOP instructions integrate REX bits and opcode bits.  They will
         be printed in print_opcode_nomodrm.  */
      if ((opcodes.size() >= 3) &&
          ((opcodes[0] == "0xc4") ||
           ((opcodes[0] == "0x8f") && (opcodes[1] != "/0")))) {
        return;
      }
      /* Allow any bits in rex prefix for the compatibility. See
         http://code.google.com/p/nativeclient/issues/detail?id=2517 */
      if (rex.w || rex.r || rex.x || rex.b) {
        if (!rex.r && !rex.x && !rex.b) {
          fprintf(out_file, "REXW_NONE ");
        } else {
          if (rex.w) {
            fprintf(out_file, "REXW_RXB ");
          } else {
            fprintf(out_file, "REX_RXB? ");
          }
        }
      }
    }

    void print_third_byte(const std::string& third_byte) const {
      auto byte = 0;
      for (auto i = 7, p = 1; i >= 0; --i, p <<= 1) {
        if (third_byte[i] == '1' || third_byte[i] == 'X' ||
            ((third_byte[i] == 'W') && rex.w)) {
          byte |= p;
        }
      }
      if (third_byte.find('X') == third_byte.npos) {
        fprintf(out_file, "0x%02x", byte);
      } else {
        std::set<decltype(byte)> bytes { byte };
        for (auto i = 7, p = 1; i >= 0; --i, p <<= 1) {
          if (third_byte[i] == 'X') {
            for (auto byte_it = bytes.begin();
                 byte_it != bytes.end(); ++byte_it) {
              auto &byte = *byte_it;
              bytes.insert(byte & ~p);
            }
          }
        }
        auto delimiter = "(";
        for (auto byte_it = bytes.begin(); byte_it != bytes.end(); ++byte_it) {
          auto &byte = *byte_it;
          fprintf(out_file, "%s0x%02x", delimiter, byte);
          delimiter = " | ";
        }
        fprintf(out_file, ")");
      }
    }

    void print_opcode_nomodrm(void) {
      if ((opcodes.size() == 1) ||
          ((opcodes.size() == 2) &&
           (opcodes[1].find('/') != opcodes[1].npos))) {
        if (opcodes[0].find('/') == opcodes[0].npos) {
          if ((instruction_class == InstructionClass::kData16) &&
              (opcodes[0] == "(0x91|0x92|0x93|0x94|0x95|0x96|0x97)")) {
            fprintf(out_file, "(0x90|0x91|0x92|0x93|0x94|0x95|0x96|0x97)");
          } else {
            fprintf(out_file, "%s", opcodes[0].c_str());
          }
        }
      } else if ((opcodes.size() >= 3) &&
                 ((opcodes[0] == "0xc4") ||
                  ((opcodes[0] == "0x8f") && (opcodes[1] != "/0"))) &&
                 (opcodes[1].substr(0, 4) == "RXB.")) {
        auto c5_ok = (opcodes[0] == "0xc4") &&
                     ((opcodes[1] == "RXB.01") ||
                      (opcodes[1] == "RXB.00001")) &&
                     ((*opcodes[2].begin() == '0') ||
                      (*opcodes[2].begin() == 'x') ||
                      (*opcodes[2].begin() == 'X'));
        if (c5_ok) fprintf(out_file, "((");
        fprintf(out_file, "(%s (VEX_", opcodes[0].c_str());
        if (ia32_mode || (!rex.r && !rex.x & !rex.b)) {
          fprintf(out_file, "NONE");
        } else {
          if (rex.r) fprintf(out_file, "R");
          if (rex.x) fprintf(out_file, "X");
          if (rex.b) fprintf(out_file, "B");
        }
        fprintf(out_file, " & VEX_map%s) ", opcodes[1].c_str() + 4);
        auto third_byte = opcodes[2];
        static const char* symbolic_names[] = { "cntl", "dest", "src1", "src" };
        for (size_t symbolic_it = 0;
             symbolic_it < arraysize(symbolic_names); ++symbolic_it) {
          auto symbolic = symbolic_names[symbolic_it];
          for (auto it = third_byte.begin(); it != third_byte.end(); ++it) {
            auto symbolic_len = strlen(symbolic);
            if (static_cast<size_t>(third_byte.end() - it) >= symbolic_len &&
                !strncmp(&*it, symbolic, strlen(symbolic))) {
              third_byte.replace(it, it + strlen(symbolic), "XXXX");
              break;
            }
          }
        }
        static const std::set<char> third_byte_check[] {
          { '0', '1', 'x', 'X', 'W' },
          { '.' },
          { '0', '1', 'x', 'X' },
          { '0', '1', 'x', 'X' },
          { '0', '1', 'x', 'X' },
          { '0', '1', 'x', 'X' },
          { '.' },
          { '0', '1', 'x', 'X' },
          { '.' },
          { '0', '1' },
          { '0', '1' }
        };
        auto third_byte_ok = (arraysize(third_byte_check) ==
            third_byte.length());
        if (third_byte_ok) {
          for (size_t set_it = 0; set_it < arraysize(third_byte_check);
              ++set_it) {
            auto &set = third_byte_check[set_it];
            if (set.find(third_byte[&set - third_byte_check]) == set.end()) {
              third_byte_ok = false;
              break;
            }
          }
        }
        if (third_byte_ok) {
          if (ia32_mode && third_byte[2] == 'X') {
            third_byte[2] = '1';
          }
          third_byte.erase(1, 1);
          third_byte.erase(5, 1);
          third_byte.erase(6, 1);
          print_third_byte(third_byte);
          if (enabled(Actions::kVexPrefix)) {
            fprintf(out_file, " @vex_prefix3");
          }
          if (c5_ok) {
            fprintf(out_file, ") | (0xc5 ");
            if (!ia32_mode && rex.r) {
              third_byte[0] = 'X';
            } else {
              third_byte[0] = '1';
            }
            print_third_byte(third_byte);
            if (enabled(Actions::kVexPrefix)) {
              fprintf(out_file, " @vex_prefix_short");
            }
            fprintf(out_file, "))");
          }
          for (auto opcode = opcodes.begin() + 3;
               opcode != opcodes.end(); ++opcode) {
            if (opcode->find('/') == opcode->npos) {
              fprintf(out_file, " %s", opcode->c_str());
            } else {
              break;
            }
          }
          fprintf(out_file, ")");
        } else {
          fprintf(stderr, "%s: error - third byte of VEX/XOP command "
            "in unparseable (%s) in instruction '%s'",
                  short_program_name, third_byte.c_str(), name.c_str());
          exit(1);
        }
      } else {
        auto delimiter = '(';
        for (auto opcode_it = opcodes.begin();
             opcode_it != opcodes.end(); ++opcode_it) {
          auto &opcode = *opcode_it;
          if (opcode.find('/') == opcode.npos) {
            fprintf(out_file, "%c%s", delimiter, opcode.c_str());
            delimiter = ' ';
          } else {
            break;
          }
        }
        fprintf(out_file, ")");
      }
      if (enabled(Actions::kOpcode)) {
        fprintf(out_file, " >begin_opcode");
      }
      if (enabled(Actions::kParseOperands)) {
        auto operand_index = 0;
        for (auto operand_it = operands.begin();
             operand_it != operands.end(); ++operand_it) {
          auto &operand = *operand_it;
          if (operand.enabled && operand.source == 'r') {
            if (enabled(Actions::kParseX87Operands) || (operand.size != "7")) {
              fprintf(out_file, " @operand%zd_from_opcode", operand_index);
            }
          }
          if (operand.enabled || enabled(Actions::kParseOperandPositions)) {
            ++operand_index;
          }
        }
      }
    }

    void print_opcode_recognition(bool memory_access) {
      if (opcode_in_modrm) {
        fprintf(out_file, " (");
        for (auto opcode = opcodes.rbegin(); opcode != opcodes.rend();
                                                                     ++opcode) {
          if ((*opcode) != "/" && opcode->find('/') != opcode->npos) {
            fprintf(out_file, "opcode_%s", opcode->c_str() + 1);
          }
        }
      }
      if (enabled(Actions::kOpcode)) {
        fprintf(out_file, " @end_opcode");
      }
      for (auto prefix_it = required_prefixes.begin();
           prefix_it != required_prefixes.end(); ++prefix_it) {
        auto &prefix = *prefix_it;
        if (prefix == "0x66") {
          fprintf(out_file, " @not_data16_prefix");
          break;
        }
      }
      for (auto prefix_it = required_prefixes.begin();
           prefix_it != required_prefixes.end(); ++prefix_it) {
        auto &prefix = *prefix_it;
        if (prefix == "0xf2") {
          fprintf(out_file, " @not_repnz_prefix");
          break;
        }
      }
      for (auto prefix_it = required_prefixes.begin();
           prefix_it != required_prefixes.end(); ++prefix_it) {
        auto &prefix = *prefix_it;
        if (prefix == "0xf3") {
          fprintf(out_file, " @not_repz_prefix");
          break;
        }
      }
      if (enabled(Actions::kInstructionName)) {
        fprintf(out_file, " @instruction_%s", c_identifier(name).c_str());
      }
      for (auto flag_it = flags.begin(); flag_it != flags.end(); ++flag_it) {
        auto &flag = *flag_it;
        if (!strncmp(flag.c_str(), "CPUFeature_", 11)) {
          fprintf(out_file, " @%s", flag.c_str());
        }
      }
      if (enabled(Actions::kParseOperands)) {
        if (enabled(Actions::kParseOperandPositions)) {
          fprintf(out_file, " @operands_count_is_%zd", operands.size());
        }
        int operand_index = 0;
        for (auto operand_it = operands.begin();
             operand_it != operands.end(); ++operand_it) {
          auto &operand = *operand_it;
          typedef std::tuple<InstructionClass, char, std::string> T;
          static const std::map<T, const char*> operand_sizes {
            { T { InstructionClass::kDefault, ' ', ""     },    "32bit"       },
            { T { InstructionClass::kSize8,   ' ', ""     },    "8bit"        },
            { T { InstructionClass::kData16,  ' ', ""     },    "16bit"       },
            { T { InstructionClass::kRexW,    ' ', ""     },    "64bit"       },
            { T { InstructionClass::kUnknown, 'H', ""     },    "128bit"      },
            { T { InstructionClass::kRexW,    'I', ""     },    "32bit"       },
            { T { InstructionClass::kUnknown, 'L', ""     },    "128bit"      },
            { T { InstructionClass::kUnknown, 'V', ""     },    "128bit"      },
            { T { InstructionClass::kUnknown, 'W', ""     },    "128bit"      },
            { T { InstructionClass::kUnknown, ' ', "2"    },    "2bit"        },
            { T { InstructionClass::kUnknown, ' ', "7"    },    "x87"         },
            { T { InstructionClass::kUnknown, ' ', "b"    },    "8bit"        },
            { T { InstructionClass::kUnknown, ' ', "d"    },    "32bit"       },
            { T { InstructionClass::kUnknown, ' ', "do"   },    "256bit"      },
            { T { InstructionClass::kUnknown, ' ', "dq"   },    "128bit"      },
            { T { InstructionClass::kUnknown, ' ', "fq"   },    "256bit"      },
            { T { InstructionClass::kUnknown, ' ', "o"    },    "128bit"      },
            { T { InstructionClass::kUnknown, ' ', "p"    },    "farptr"      },
            { T { InstructionClass::kUnknown, ' ', "pb"   },    "xmm"         },
            { T { InstructionClass::kUnknown, ' ', "pd"   },    "xmm"         },
            { T { InstructionClass::kUnknown, ' ', "pdw"  },    "xmm"         },
            { T { InstructionClass::kUnknown, ' ', "pdwx" },    "ymm"         },
            { T { InstructionClass::kUnknown, ' ', "pdx"  },    "ymm"         },
            { T { InstructionClass::kUnknown, ' ', "ph"   },    "xmm"         },
            { T { InstructionClass::kUnknown, ' ', "pi"   },    "xmm"         },
            { T { InstructionClass::kUnknown, ' ', "pj"   },    "xmm"         },
            { T { InstructionClass::kUnknown, ' ', "pjx"  },    "ymm"         },
            { T { InstructionClass::kUnknown, ' ', "pk"   },    "xmm"         },
            { T { InstructionClass::kUnknown, ' ', "pq"   },    "xmm"         },
            { T { InstructionClass::kUnknown, ' ', "pqw"  },    "xmm"         },
            { T { InstructionClass::kUnknown, ' ', "pqwx" },    "ymm"         },
            { T { InstructionClass::kUnknown, ' ', "ps"   },    "xmm"         },
            { T { InstructionClass::kUnknown, ' ', "psx"  },    "ymm"         },
            { T { InstructionClass::kUnknown, ' ', "pw"   },    "xmm"         },
            { T { InstructionClass::kUnknown, ' ', "q"    },    "64bit"       },
            { T { InstructionClass::kUnknown, 'N', "q"    },    "mmx"         },
            { T { InstructionClass::kUnknown, 'P', "q"    },    "mmx"         },
            { T { InstructionClass::kUnknown, 'Q', "q"    },    "mmx"         },
            { T { InstructionClass::kUnknown, 'U', "q"    },    "xmm"         },
            { T { InstructionClass::kUnknown, 'V', "q"    },    "xmm"         },
            { T { InstructionClass::kUnknown, 'W', "q"    },    "xmm"         },
            { T { InstructionClass::kUnknown, ' ', "r"    },    "regsize"     },
            { T { InstructionClass::kUnknown, 'C', "r"    },    "creg"        },
            { T { InstructionClass::kUnknown, 'D', "r"    },    "dreg"        },
            { T { InstructionClass::kUnknown, ' ', "s"    },    "selector"    },
            { T { InstructionClass::kUnknown, ' ', "sb"   },    "x87_bcd"     },
            { T { InstructionClass::kUnknown, ' ', "sd"   },    "float64bit"  },
            { T { InstructionClass::kUnknown, ' ', "se"   },    "x87_env"     },
            { T { InstructionClass::kUnknown, ' ', "si"   },    "x87_32bit"   },
            { T { InstructionClass::kUnknown, ' ', "sq"   },    "x87_64bit"   },
            { T { InstructionClass::kUnknown, ' ', "sr"   },    "x87_state"   },
            { T { InstructionClass::kUnknown, ' ', "ss"   },    "float32bit"  },
            { T { InstructionClass::kUnknown, ' ', "st"   },    "float80bit"  },
            { T { InstructionClass::kUnknown, ' ', "sw"   },    "x87_16bit"   },
            { T { InstructionClass::kUnknown, ' ', "sx"   },
                                                          "x87_mmx_xmm_state" },
            { T { InstructionClass::kData16,  ' ', "v"    },    "16bit"       },
            { T { InstructionClass::kDefault, ' ', "v"    },    "32bit"       },
            { T { InstructionClass::kRexW,    ' ', "v"    },    "64bit"       },
            { T { InstructionClass::kUnknown, ' ', "w"    },    "16bit"       },
            { T { InstructionClass::kUnknown, 'S', "w"    },    "segreg"      },
            { T { InstructionClass::kUnknown, 'H', "x"    },    "256bit"      },
            { T { InstructionClass::kUnknown, 'L', "x"    },    "256bit"      },
            { T { InstructionClass::kUnknown, 'V', "x"    },    "256bit"      },
            { T { InstructionClass::kUnknown, 'W', "x"    },    "256bit"      },
            { T { InstructionClass::kDefault, ' ', "y"    },    "32bit"       },
            { T { InstructionClass::kSize8,   ' ', "y"    },    "32bit"       },
            { T { InstructionClass::kData16,  ' ', "y"    },    "32bit"       },
            { T { InstructionClass::kDefault, ' ', "y"    },    "32bit"       },
            { T { InstructionClass::kRexW,    ' ', "y"    },    "64bit"       },
            { T { InstructionClass::kData16,  ' ', "z"    },    "16bit"       },
            { T { InstructionClass::kDefault, ' ', "z"    },    "32bit"       },
            { T { InstructionClass::kRexW,    ' ', "z"    },    "32bit"       }
          };
          auto it = operand_sizes.find(T {instruction_class,
                                          operand.source, operand.size});
          if (it == operand_sizes.end()) {
            it = operand_sizes.find(T {InstructionClass::kUnknown,
                                       operand.source, operand.size});
          }
          if (it == operand_sizes.end()) {
            it = operand_sizes.find(T {instruction_class,
                                       ' ', operand.size});
          }
          if (it == operand_sizes.end()) {
            it = operand_sizes.find(T {instruction_class,
                                       operand.source, ""});
          }
          if (it == operand_sizes.end()) {
            it = operand_sizes.find(T {InstructionClass::kUnknown,
                                       ' ', operand.size});
          }
          if (it == operand_sizes.end()) {
            fprintf(stderr,
                    "%s: error - can not determine operand size: %c%s",
                    short_program_name,
                    operand.source,
                    operand.size.c_str());
            exit(1);
          } else {
            if (!enabled(Actions::kParseImmediateOperands)) {
              if (operand.source == 'I' || operand.source == 'i') {
                operand.enabled = false;
              }
            }
            if (!enabled(Actions::kParseRelativeOperands)) {
              if (operand.source == 'J' || operand.source == 'O') {
                operand.enabled = false;
              }
            }
            if (!enabled(Actions::kParseX87Operands) &&
                (!strncmp(it->second, "x87", 3) ||
                 !strncmp(it->second, "float", 5))) {
              operand.enabled = false;
            }
            if (!enabled(Actions::kParseMMXOperands) &&
                !strcmp(it->second, "mmx")) {
              operand.enabled = false;
            }
            if (!enabled(Actions::kParseXMMOperands) &&
                (!strcmp(it->second, "xmm") ||
                 !strcmp(it->second, "128bit"))) {
              operand.enabled = false;
            }
            if (!enabled(Actions::kParseYMMOperands) &&
                (!strcmp(it->second, "ymm") ||
                 !strcmp(it->second, "256bit"))) {
              operand.enabled = false;
            }
            if (!operand.write && !enabled(Actions::kParseNonWriteRegisters)) {
              operand.enabled = false;
            }
            if (operand.enabled) {
              if (enabled(Actions::kParseOperandPositions) ||
                  (((operand.source != 'E') && (operand.source != 'M') &&
                    (operand.source != 'N') && (operand.source != 'Q') &&
                    (operand.source != 'R') && (operand.source != 'U') &&
                    (operand.source != 'W')) || !memory_access)) {
                fprintf(out_file, " @operand%zd_%s", operand_index, it->second);
              }
            }
          }
          static std::map<char, const char*> operand_type {
            { '1', "one"                },
            { 'a', "rax"                },
            { 'b', "ds_rbx"             },
            { 'c', "rcx"                },
            { 'd', "rdx"                },
            { 'i', "second_immediate"   },
            { 'o', "port_dx"            },
            { 't', "st"                 },
            { 'B', "from_vex"           },
            { 'H', "from_vex"           },
            { 'I', "immediate"          },
            { 'O', "absolute_disp"      },
            { 'X', "ds_rsi"             },
            { 'Y', "es_rdi"             }
          };
          auto it2 = operand_type.find(operand.source);
          if (it2 != operand_type.end()) {
            if (operand.enabled) {
              fprintf(out_file, " @operand%zd_%s", operand_index, it2->second);
            }
          }
          if (operand.enabled || enabled(Actions::kParseOperandPositions)) {
            ++operand_index;
          }
        }
      }
      if (enabled(Actions::kParseOperandsStates)) {
        auto operand_index = 0;
        for (auto operand_it = operands.begin();
             operand_it != operands.end(); ++operand_it) {
          auto &operand = *operand_it;
          if (operand.enabled) {
            if (operand.write) {
              if (operand.read) {
                fprintf(out_file, " @operand%zd_readwrite", operand_index);
              } else {
                fprintf(out_file, " @operand%zd_write", operand_index);
              }
            } else {
              if (operand.read) {
                fprintf(out_file, " @operand%zd_read", operand_index);
              } else {
                fprintf(out_file, " @operand%zd_unused", operand_index);
              }
            }
          }
          if (operand.enabled || enabled(Actions::kParseOperandPositions)) {
            ++operand_index;
          }
        }
      }
      if (opcode_in_modrm) {
        fprintf(out_file, " any* &");
      }
    }

    void print_immediate_arguments(void) {
      auto operand_index = 0;
      for (auto it = operands.begin(); it != operands.end(); ++it) {
        if (it->enabled || enabled(Actions::kParseOperandPositions)) {
          ++operand_index;
        }
      }
      for (auto operand = operands.rbegin(); operand != operands.rend();
                                                                    ++operand) {
        static const std::map<std::pair<InstructionClass, std::string>,
            const char*> immediate_sizes {
          { { InstructionClass::kDefault,       ""      },      "imm32" },
          { { InstructionClass::kSize8,         ""      },      "imm8"  },
          { { InstructionClass::kData16,        ""      },      "imm16" },
          { { InstructionClass::kRexW,          ""      },      "imm32" },
          { { InstructionClass::kDefault,       "2"     },      "imm2"  },
          { { InstructionClass::kDefault,       "b"     },      "imm8"  },
          { { InstructionClass::kSize8,         "b"     },      "imm8"  },
          { { InstructionClass::kData16,        "b"     },      "imm8"  },
          { { InstructionClass::kRexW,          "b"     },      "imm8"  },
          { { InstructionClass::kDefault,       "d"     },      "imm32" },
          { { InstructionClass::kRexW,          "d"     },      "imm32" },
          { { InstructionClass::kDefault,       "v"     },      "imm32" },
          { { InstructionClass::kData16,        "v"     },      "imm16" },
          { { InstructionClass::kRexW,          "v"     },      "imm64" },
          { { InstructionClass::kDefault,       "w"     },      "imm16" },
          { { InstructionClass::kDefault,       "z"     },      "imm32" },
          { { InstructionClass::kData16,        "z"     },      "imm16" },
          { { InstructionClass::kRexW,          "z"     },      "imm32" }
        };
        if (operand->source == 'i') {
          auto it = immediate_sizes.find({instruction_class,
                                          operand->size});
          if (it == immediate_sizes.end()) {
            fprintf(stderr,
                    "%s: error - can not determine immediate size: %c%s",
                    short_program_name, operand->source, operand->size.c_str());
            exit(1);
          }
          fprintf(out_file, " %sn2", it->second);
        }
        if (operand->source == 'I') {
          auto it = immediate_sizes.find({instruction_class,
                                          operand->size});
          if (it == immediate_sizes.end()) {
            fprintf(stderr,
                    "%s: error - can not determine immediate size: %c%s",
                    short_program_name, operand->source, operand->size.c_str());
            exit(1);
          }
          fprintf(out_file, " %s", it->second);
        }
        static const std::map<std::pair<InstructionClass, std::string>,
             const char *> jump_sizes {
          { { InstructionClass::kDefault,       "b"     },      "rel8"  },
          { { InstructionClass::kDefault,       "d"     },      "rel32" },
          { { InstructionClass::kDefault,       "w"     },      "rel16" },
          { { InstructionClass::kDefault,       "z"     },      "rel32" },
          { { InstructionClass::kData16,        "z"     },      "rel16" },
          { { InstructionClass::kRexW,          "z"     },      "rel32" },
        };
        if (operand->source == 'J') {
          auto it = jump_sizes.find({instruction_class, operand->size});
          if (it == jump_sizes.end()) {
            fprintf(stderr, "%s: error - can not determine jump size: %c%s",
                    short_program_name, operand->source, operand->size.c_str());
            exit(1);
          }
          fprintf(out_file, " %s", it->second);
        }
        if (operand->source == 'L') {
          if (operands.size() == 4) {
            if (ia32_mode) {
              fprintf(out_file, " %s", chartest((c & 0x8f) == 0x00));
            } else {
              fprintf(out_file, " %s", chartest((c & 0x0f) == 0x00));
            }
          }
          if (operand->enabled && enabled(Actions::kParseOperands)) {
            fprintf(out_file, " @operand%zd_from_is4", operand_index - 1);
          }
        }
        if (operand->source == 'O') {
          if (ia32_mode) {
            fprintf(out_file, " disp32");
          } else {
            fprintf(out_file, " disp64");
          }
        }
        if (operand->enabled || enabled(Actions::kParseOperandPositions)) {
          --operand_index;
        }
      }
      for (auto prefix_it = required_prefixes.begin();
           prefix_it != required_prefixes.end(); ++prefix_it) {
        auto &prefix = *prefix_it;
        if (prefix == "0xf0") {
          for (auto operand_it = operands.begin();
               operand_it != operands.end(); ++operand_it) {
            auto &operand = *operand_it;
            if (operand.source == 'C') {
              fprintf(out_file, " @not_lock_prefix%zd",
                      &operand - &*operands.begin());
              break;
            }
          }
          break;
        }
      }
    }

    void print_immediate_opcode(void) {
      auto print_opcode = false;
      for (auto opcode_it = opcodes.begin();
           opcode_it != opcodes.end(); ++opcode_it) {
        auto &opcode = *opcode_it;
        if (opcode == "/") {
          print_opcode = true;
        } else if (print_opcode) {
          fprintf(out_file, " %s", opcode.c_str());
        }
      }
    }
  };

  void print_one_instruction_definition(void) {
    for (auto instruction_it = instructions.begin();
         instruction_it != instructions.end(); ++instruction_it) {
      auto &instruction = *instruction_it;
      MarkedInstruction(instruction).print_definition();
    }
  }
  bool MarkedInstruction::first_delimiter = true;
}

struct compare_action {
  const char* action;

  explicit compare_action(const char* y) : action(y) {
  }

  bool operator()(const char* x) const {
    return !strcmp(x, action);
  }
};

int main(int argc, char *argv[]) {
  /* basename(3) may change the passed argument thus we are using copy
     of argv[0].  This creates tiny memory leak but since we only do that
     once per program invocation it's contained.  */
  short_program_name = basename(strdup(argv[0]));

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
        for (auto action_to_disable = strtok(optarg, ",");
             action_to_disable;
             action_to_disable = strtok(NULL, ",")) {
          compare_action compare_with_action_to_disable(action_to_disable);
          auto action_number = std::find_if(
            kDisablableActionsList,
            kDisablableActionsList + arraysize(kDisablableActionsList),
            compare_with_action_to_disable);
          if (action_number !=
              kDisablableActionsList + arraysize(kDisablableActionsList)) {
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
        abort();
    }
  }

  for (auto i = optind; i < argc; ++i) {
    load_instructions(argv[i]);
  }

  if (out_file_name && !(out_file = fopen(out_file_name, "w"))) {
    fprintf(stderr,
            "%s: can not open '%s' file (%s)\n",
            short_program_name, out_file_name, strerror(errno));
    return 1;
  } else if ((out_file_name || const_file_name) &&
             (enabled(Actions::kInstructionName) ||
              enabled(Actions::kParseOperands))) {
    size_t const_name_len = 0;
    if (out_file_name && !const_file_name) {
      const_name_len = strlen(out_file_name) + 10;
      const_file_name = static_cast<char *>(malloc(const_name_len));
      strcpy(const_file_name, out_file_name);
      char* dot_position = strrchr(const_file_name, '.');
      if (!dot_position) {
        dot_position = strrchr(const_file_name, '\0');
      }
      strcpy(dot_position, "-consts.c");
    }
    if (!(const_file = fopen(const_file_name, "w"))) {
      fprintf(stderr, "%s: can not open '%s' file (%s)\n",
              short_program_name, const_file_name, strerror(errno));
       return 1;
    }
    if (const_name_len) {
      free(const_file_name);
      const_file_name = NULL;
    }
  }

  if (enabled(Actions::kInstructionName) ||
      enabled(Actions::kParseOperands)) {
    print_consts();

    if (out_file == const_file) {
      for (auto i = 0; i < 80; ++i) {
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

  print_common_decoding();

  if (enabled(Actions::kInstructionName)) {
    print_name_actions();
  }

  fprintf(out_file, "\n  one_instruction =");

  print_one_instruction_definition();

  fprintf(out_file, ");\n"
"}%%%%\n"
"");
  return 0;
}
