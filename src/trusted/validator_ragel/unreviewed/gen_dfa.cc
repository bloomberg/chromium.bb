/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
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
#include <tuple>
#include <vector>

#if defined __GNUC__ && defined __GNUC_MINOR__
  #if __GNUC__ == 4 && __GNUC_MINOR__ < 6
    namespace std {
      template<class Container>
      inline auto begin(Container& container) -> decltype(container.begin()) {
        return container.begin();
      }

      template<class Container>
      inline auto end(Container& container) -> decltype(container.end()) {
        return container.end();
      }
    }
    #define nullptr ((const char*)0)
  #endif
#endif

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
  const char* current_dir_name;

  const struct option kProgramOptions[] = {
    {"mode",            required_argument,      NULL,           'm'},
    {"disable",         required_argument,      NULL,           'd'},
    {"output",          required_argument,      NULL,           'o'},
    {"const_file",      required_argument,      NULL,           'c'},
    {"help",            no_argument,            NULL,           'h'},
    {"version",         no_argument,            NULL,           'v'},
    {NULL,              0,                      NULL,           0}
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
"  check_access        this will check memory access (action is not generated\n"
"                        by %1$s, you need to define it in your program)\n"
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
    kCheckAccess,
    kNaClForbidden
  };
  const char* kDisablableActionsList[] = {
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
    "check_access",
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

  class extract_operand;
  class MarkedInstruction;
  class Instruction {
   protected:
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

   private:
    static void check_flag_valid(const std::string &flag) {
      if (all_instruction_flags.find(flag) == end(all_instruction_flags)) {
        fprintf(stderr, "%s: unknown flag: '%s'\n",
                short_program_name, flag.c_str());
        exit(1);
      }
    }

    void add_flag(const std::string &flag) {
      check_flag_valid(flag);
      flags.insert(flag);
    }

    friend class extract_operand;
    friend class MarkedInstruction;
    friend void load_instructions(const char*);

   public:
    const decltype(name)& get_name(void) const {
      return name;
    }
    Instruction replace_name(const decltype(name)& name) const {
      auto result = *this;
      result.name = name;
      return result;
    }
    const decltype(operands)& get_operands(void) const {
      return operands;
    }
    Instruction replace_operands(const decltype(operands)& operands) const {
      auto result = *this;
      result.operands = operands;
      return result;
    }
    const decltype(opcodes)& get_opcodes(void) const {
      return opcodes;
    }
    Instruction replace_opcodes(const decltype(opcodes)& opcodes) const {
      auto result = *this;
      result.opcodes = opcodes;
      return result;
    }
    const decltype(flags)& get_flags(void) const {
      return flags;
    }
    Instruction replace_flags(const decltype(flags)& flags) const {
      auto result = *this;
      result.flags = flags;
      return result;
    }
    bool has_flag(const std::string &flag) const {
      check_flag_valid(flag);
      return flags.find(flag) != end(flags);
    }
  };
  std::vector<Instruction> instructions;

  FILE *out_file = stdout;
  FILE *const_file = stdout;
  const char *out_file_name = nullptr;
  const char *const_file_name = nullptr;

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
    auto it = begin(file_content);
    while (it != end(file_content)) {
      it = std::find_if_not(it, end(file_content), is_eol);
      if (it == end(file_content)) {
        return;
      }
      /* If line starts with '#' then it's a comment. */
      if (*it == '#') {
        it = std::find_if(it, end(file_content), is_eol);
      } else {
        auto line_end = std::find_if(it, end(file_content), is_eol);
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
            if (it == line_end)
                      line_end = std::find_if(++it, end(file_content), is_eol);
            instruction.opcodes = split_till_comma(&it, line_end);
            if (*it == ',') {
              ++it;
              if (it == line_end)
                      line_end = std::find_if(++it, end(file_content), is_eol);
              auto flags = split_till_comma(&it, line_end);
              for (auto flag_it = begin(flags);
                   flag_it != end(flags); ++flag_it) {
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
        it = std::find_if(it, end(file_content), is_eol);
      }
    }
  }

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
      std::transform(begin(instruction_names), end(instruction_names),
        std::back_inserter(names),
        select_name);
      std::sort(begin(names), end(names), compare_names);
      for (auto name_it = begin(names); name_it != end(names); ++name_it) {
        auto &name = *name_it;
        if (instruction_names[name] == 0) {
          for (decltype(name.length()) p = 1; p < name.length(); ++p) {
            auto it = instruction_names.find(std::string(name, p));
            if (it != end(instruction_names)) {
              it->second = 1;
            }
          }
        }
      }
      size_t offset = 0;
      for (auto pair_it = begin(instruction_names);
           pair_it != end(instruction_names); ++pair_it) {
        auto &pair = *pair_it;
        if (pair.second != 1) {
          pair.second = offset;
          offset += pair.first.length() + 1;
        }
      }
      for (auto name_it = begin(names); name_it != end(names); ++name_it) {
        auto &name = *name_it;
        auto offset = instruction_names[name];
        if (offset != 1) {
          for (decltype(name.length()) p = 1; p < name.length(); ++p) {
            auto it = instruction_names.find(std::string(name, p));
            if ((it != end(instruction_names)) && (it->second == 1)) {
              it->second = offset + p;
            }
          }
        }
      }
      offset = 0;
      auto delimiter = "static const char instruction_names[] = {\n  ";
      for (auto pair_it = begin(instruction_names);
           pair_it != end(instruction_names); ++pair_it) {
        auto &pair = *pair_it;
        if (pair.second == offset) {
          fprintf(const_file, "%s", delimiter);
          for (auto c_it = begin(pair.first);
               c_it != end(pair.first); ++c_it) {
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
    for (auto c_it = begin(text); c_it != end(text); ++c_it) {
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

  void print_name_actions(void) {
    for (auto pair_it = begin(instruction_names);
         pair_it != end(instruction_names); ++pair_it) {
      auto &pair = *pair_it;
      fprintf(out_file, "  action instruction_%s"
        " { SET_INSTRUCTION_NAME(instruction_names + %zd); }\n",
                                 c_identifier(pair.first).c_str(), pair.second);
    }
  }

  class MarkedInstruction : public Instruction {
    std::multiset<std::string> required_prefixes;
    std::multiset<std::string> optional_prefixes;
    struct RexType {
      bool b : 1;
      bool x : 1;
      bool r : 1;
      bool w : 1;
    } rex;
    bool opcode_in_modrm : 1;
    bool opcode_in_imm : 1;
    bool fwait : 1;
   public:
    explicit MarkedInstruction(const Instruction& instruction_) :
        Instruction(instruction_),
        required_prefixes(),
        optional_prefixes(),
        rex { false, false, false, false },
        opcode_in_modrm(false),
        opcode_in_imm(false),
        fwait(false) {
      if (has_flag("branch_hint")) optional_prefixes.insert("branch_hint");
      if (has_flag("condrep")) optional_prefixes.insert("condrep");
      if (has_flag("rep")) optional_prefixes.insert("rep");
      for (auto opcode_it = begin(opcodes);
           opcode_it != end(opcodes); ++opcode_it) {
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
      for (auto operand_it = begin(operands);
           operand_it != end(operands); ++operand_it) {
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
        "rexw", "fwait"
      };
      while (opcode_prefixes.find(*begin(opcodes)) != end(opcode_prefixes)) {
        if ((*begin(opcodes)) == "rexw")
          rex.w = true;
        else if ((*begin(opcodes)) == "fwait")
          fwait = true;
        else if ((*begin(opcodes)) != "rexw")
          required_prefixes.insert(*begin(opcodes));
        opcodes.erase(begin(opcodes));
      }
    }
    MarkedInstruction(const Instruction& instruction_,
                      const decltype(required_prefixes)& required_prefixes_,
                      const decltype(optional_prefixes)& optional_prefixes_,
                      const decltype(rex)& rex_,
                      const decltype(opcode_in_modrm)& opcode_in_modrm_,
                      const decltype(opcode_in_imm)& opcode_in_imm_,
                      const decltype(fwait)& fwait_) :
      Instruction(instruction_),
      required_prefixes(required_prefixes_),
      optional_prefixes(optional_prefixes_),
      rex(rex_),
      opcode_in_modrm(opcode_in_modrm_),
      opcode_in_imm(opcode_in_imm_),
      fwait(fwait_) {
    }
    MarkedInstruction replace_name(const decltype(name)& name) const {
      return MarkedInstruction(Instruction::replace_name(name),
                               required_prefixes,
                               optional_prefixes,
                               rex,
                               opcode_in_modrm,
                               opcode_in_imm,
                               fwait);
    }
    MarkedInstruction replace_operands(
                                     const decltype(operands)& operands) const {
      return MarkedInstruction(Instruction::replace_operands(operands),
                               required_prefixes,
                               optional_prefixes,
                               rex,
                               opcode_in_modrm,
                               opcode_in_imm,
                               fwait);
    }
    MarkedInstruction replace_opcodes(
                                const std::vector<std::string>& opcodes) const {
      return MarkedInstruction(Instruction::replace_opcodes(opcodes),
                               required_prefixes,
                               optional_prefixes,
                               rex,
                               opcode_in_modrm,
                               opcode_in_imm,
                               fwait);
    }
    MarkedInstruction replace_flags(const std::set<std::string>& flags) const {
      return MarkedInstruction(Instruction::replace_flags(flags),
                               required_prefixes,
                               optional_prefixes,
                               rex,
                               opcode_in_modrm,
                               opcode_in_imm,
                               fwait);
    }
    const decltype(required_prefixes)& get_required_prefixes(void) const {
      return required_prefixes;
    }
    MarkedInstruction add_requred_prefix(const std::string& string) const {
      auto result = *this;
      result.required_prefixes.insert(string);
      if (string == "data16")
        for (auto operand_it = begin(result.operands);
             operand_it != end(result.operands); ++operand_it) {
          auto& operand = *operand_it;
          if (operand.size == "v" || operand.size == "z")
            operand.size = "w";
          if (operand.size == "y")
            operand.size = "d";
        }
      return result;
    }
    const decltype(optional_prefixes)& get_optional_prefixes(void) const {
      return optional_prefixes;
    }
    MarkedInstruction add_optional_prefix(const std::string& string) const {
      auto result = *this;
      result.optional_prefixes.insert(string);
      return result;
    }
    const decltype(rex)& get_rex(void) const {
      return rex;
    }
    MarkedInstruction set_rex_b(void) const {
      auto result = *this;
      result.rex.b = true;
      return result;
    }
    MarkedInstruction set_rex_x(void) const {
      auto result = *this;
      result.rex.x = true;
      return result;
    }
    MarkedInstruction set_rex_r(void) const {
      auto result = *this;
      result.rex.r = true;
      return result;
    }
    MarkedInstruction set_rex_w(void) const {
      auto result = *this;
      result.rex.w = true;
      for (auto operand_it = begin(result.operands);
           operand_it != end(result.operands); ++operand_it) {
        auto& operand = *operand_it;
        if (operand.size == "v" || operand.size == "y")
          operand.size = "q";
        if (operand.size == "z")
          operand.size = "d";
      }
      return result;
    }
    MarkedInstruction clear_rex_w(void) const {
      auto result = *this;
      result.rex.w = false;
      for (auto operand_it = begin(result.operands);
           operand_it != end(result.operands); ++operand_it) {
        auto& operand = *operand_it;
        if (operand.size == "v" || operand.size == "y" || operand.size == "z")
          operand.size = "d";
      }
      return result;
    }
    decltype(opcode_in_modrm) get_opcode_in_modrm(void) const {
      return opcode_in_modrm;
    }
    decltype(opcode_in_modrm) get_opcode_in_imm(void) const {
      return opcode_in_imm;
    }
    decltype(opcode_in_modrm) get_fwait(void) const {
      return fwait;
    }
  };

  bool mod_reg_is_used(const MarkedInstruction& instruction) {
    const auto& operands = instruction.get_operands();
    for (auto operand_it = begin(operands);
         operand_it != end(operands); ++operand_it) {
      auto& operand = *operand_it;
      const auto source = operand.source;
      const auto& prefixes = instruction.get_required_prefixes();
      /* Control registers can use 0xf0 prefix to select %cr8…%cr15.  */
      if ((source == 'C' && prefixes.find("0xf0") == end(prefixes)) ||
          source == 'G' || source == 'P' || source == 'V')
        return true;
    }
    return false;
  }

  bool mod_rm_is_used(const MarkedInstruction& instruction) {
    const auto& operands = instruction.get_operands();
    for (auto operand_it = begin(operands);
         operand_it != end(operands); ++operand_it) {
      auto& operand = *operand_it;
      const auto source = operand.source;
      if (source == 'E' || source == 'M' || source == 'N' ||
          source == 'Q' || source == 'R' || source == 'U' || source == 'W')
        return true;
    }
    return false;
  }

  bool first_delimiter = true;
  void print_operator_delimiter(void) {
    if (first_delimiter)
      fprintf(out_file, "\n    (");
    else
      fprintf(out_file, ") |\n    (");
    first_delimiter = false;
  }

  void print_legacy_prefixes(const MarkedInstruction& instruction) {
    const auto& required_prefixes = instruction.get_required_prefixes();
    if (instruction.get_fwait()) {
      if (ia32_mode)
        fprintf(out_file, "0x9b ");
      else if ((required_prefixes.size() == 1) &&
               (*begin(required_prefixes)) == "data16")
        fprintf(out_file, "(REX_WRXB? 0x9b data16 | 0x9b ");
      else
        fprintf(out_file, "(REX_WRXB? 0x9b | 0x9b ");
    }
    const auto& optional_prefixes = instruction.get_optional_prefixes();
    if ((required_prefixes.size() == 1) &&
        (optional_prefixes.size() == 0)) {
      fprintf(out_file, "%s ", begin(required_prefixes)->c_str());
    } else if ((required_prefixes.size() == 0) &&
               (optional_prefixes.size() == 1)) {
      fprintf(out_file, "%s? ", begin(optional_prefixes)->c_str());
    } else if ((optional_prefixes.size() > 0) ||
               (required_prefixes.size() > 0)) {
      auto delimiter = "(";
      auto opt_start = required_prefixes.size() ? 0 : 1;
      auto opt_end = 1 << optional_prefixes.size();
      for (auto opt = opt_start; opt < opt_end; ++opt) {
        auto prefixes = required_prefixes;
        auto it = begin(optional_prefixes);
        for (auto id = 1; id < opt_end; id <<= 1, ++it) {
          if (opt & id) {
            prefixes.insert(*it);
          }
        }
        if (prefixes.size() == 1) {
          fprintf(out_file, "%s%s", delimiter, begin(prefixes)->c_str());
          delimiter = " | ";
        } else {
          std::vector<std::string> permutations(begin(prefixes), end(prefixes));
          do {
            fprintf(out_file, "%s", delimiter);
            delimiter = " | ";
            auto delimiter = '(';
            for (auto prefix_it = begin(permutations);
                 prefix_it != end(permutations); ++prefix_it) {
              auto &prefix = *prefix_it;
              fprintf(out_file, "%c%s", delimiter, prefix.c_str());
              delimiter = ' ';
            }
            fprintf(out_file, ")");
          } while (next_permutation(begin(permutations), end(permutations)));
        }
      }
      if (opt_start)
        fprintf(out_file, ")? ");
      else
        fprintf(out_file, ") ");
    }
  }

  void print_rex_prefix(const MarkedInstruction& instruction) {
    /* Prefix REX is not used in ia32 mode.  */
    if (ia32_mode || instruction.has_flag("norex"))
      return;
    const auto& opcodes = instruction.get_opcodes();
    /* VEX/XOP instructions integrate REX bits and opcode bits.  They will
       be printed in print_opcode_nomodrm.  */
    if ((opcodes.size() >= 3) &&
        ((opcodes[0] == "0xc4") ||
         ((opcodes[0] == "0x8f") && (opcodes[1] != "/0"))))
      return;
    /* Allow any bits in  rex prefix for the compatibility. See
       http://code.google.com/p/nativeclient/issues/detail?id=2517 */
    if (instruction.get_rex().w)
      fprintf(out_file, "REXW_RXB");
    else
      fprintf(out_file, "REX_RXB?");
    if (instruction.get_fwait())
      fprintf(out_file, ")");
    fprintf(out_file, " ");
  }

  void print_third_byte_of_vex(const MarkedInstruction& instruction,
                               const std::string& third_byte) {
    auto byte = 0;
    for (auto i = 7, p = 1; i>=0; --i, p <<= 1)
      if (third_byte[i] == '1' || third_byte[i] == 'X' ||
          ((third_byte[i] == 'W') && instruction.get_rex().w)) {
            byte |= p;
      }
      if (third_byte.find('X') == third_byte.npos) {
        fprintf(out_file, "0x%02x", byte);
      } else {
        std::set<decltype(byte)> bytes { byte };
        for (auto i = 7, p = 1; i>=0; --i, p <<= 1)
          if (third_byte[i] == 'X')
            for (auto byte_it = begin(bytes);
                 byte_it != end(bytes); ++byte_it) {
              auto &byte = *byte_it;
              bytes.insert(byte & ~p);
            }
        auto delimiter = "(";
        for (auto byte_it = begin(bytes); byte_it != end(bytes); ++byte_it) {
          auto &byte = *byte_it;
          fprintf(out_file, "%s0x%02x", delimiter, byte);
          delimiter = " | ";
        }
        fprintf(out_file, ")");
      }
  }

  void print_vex_opcode(const MarkedInstruction& instruction) {
    const auto& opcodes = instruction.get_opcodes();
    auto c5_ok = (opcodes[0] == "0xc4") &&
                 ((opcodes[1] == "RXB.01") ||
                  (opcodes[1] == "RXB.00001")) &&
                 ((*begin(opcodes[2]) == '0') ||
                  (*begin(opcodes[2]) == 'x') ||
                  (*begin(opcodes[2]) == 'X'));
    if (c5_ok) fprintf(out_file, "((");
    auto rex = instruction.get_rex();
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
      for (auto it = begin(third_byte); it != end(third_byte); ++it)
        if (static_cast<size_t>(end(third_byte) - it) >= strlen(symbolic) &&
             strncmp(&*it, symbolic, strlen(symbolic)) == 0) {
          third_byte.replace(it, it + strlen(symbolic), "XXXX");
          break;
        }
    }
#if 0
    /* We need GCC 4.8 or may be even later for the following:  */
    static const std::regex third_byte_check(
                       R"([01xX]\.[01xX][01xX][01xX][01xX]\.[01xX]\.[01][01])");
    if ((third_byte.length() != 11) ||
          !regex_match(begin(third_byte), end(third_byte), third_byte_check)) {
#else
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
    auto third_byte_ok = (arraysize(third_byte_check) == third_byte.length());
    if (third_byte_ok)
      for (size_t set_it = 0; set_it < arraysize(third_byte_check); ++set_it) {
        auto &set = third_byte_check[set_it];
        if (set.find(third_byte[&set - third_byte_check]) == end(set)) {
          third_byte_ok = false;
          break;
        }
      }
    if (third_byte_ok) {
#endif
      if (ia32_mode && third_byte[2] == 'X')
        third_byte[2] = '1';
      third_byte.erase(1, 1);
      third_byte.erase(5, 1);
      third_byte.erase(6, 1);
      print_third_byte_of_vex(instruction, third_byte);
      if (enabled(Actions::kVexPrefix))
        fprintf(out_file, " @vex_prefix3");
      if (c5_ok) {
        fprintf(out_file, ") | (0xc5 ");
        if (!ia32_mode && rex.r)
          third_byte[0] = 'X';
        else
          third_byte[0] = '1';
        print_third_byte_of_vex(instruction, third_byte);
        if (enabled(Actions::kVexPrefix))
          fprintf(out_file, " @vex_prefix_short");
        fprintf(out_file, "))");
      }
      for (auto opcode = begin(opcodes) + 3; opcode != end(opcodes); ++opcode)
        if (opcode->find('/') == opcode->npos)
          fprintf(out_file, " %s", opcode->c_str());
        else
          break;
      fprintf(out_file, ")");
    } else {
      fprintf(stderr, "%s: error - third byte of VEX/XOP command "
                      "in unparseable (%s) in instruction “%s”",
                      short_program_name, third_byte.c_str(),
                      instruction.get_name().c_str());
      exit(1);
    }
  }

  void print_opcode_nomodrm(const MarkedInstruction& instruction) {
    const auto& opcodes = instruction.get_opcodes();
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
      auto delimiter = '(';
      for (auto opcode_it = begin(opcodes);
           opcode_it != end(opcodes); ++opcode_it) {
        auto opcode = *opcode_it;
        if (opcode.find('/') == opcode.npos)
          fprintf(out_file, "%c%s", delimiter, opcode.c_str()),
          delimiter = ' ';
        else
          break;
      }
      fprintf(out_file, ")");
    }
    if (enabled(Actions::kOpcode))
      fprintf(out_file, " >begin_opcode");
    const auto& operands = instruction.get_operands();
    if (enabled(Actions::kParseOperands)) {
      auto operand_index = 0;
      for (auto operand_it = begin(operands);
           operand_it != end(operands); ++operand_it) {
        auto &operand = *operand_it;
        if (operand.enabled && operand.source == 'r') {
          if (operand.size == "x87") {
            if (enabled(Actions::kParseX87Operands)) {
              fprintf(out_file, " @operand%zd_from_opcode_x87", operand_index);
            }
          } else {
            fprintf(out_file, " @operand%zd_from_opcode", operand_index);
          }
        }
        if (operand.enabled || enabled(Actions::kParseOperandPositions))
          ++operand_index;
      }
    }
  }

  void print_immediate_opcode(const MarkedInstruction& instruction) {
    auto print_opcode = false;
    const auto& opcodes = instruction.get_opcodes();
    for (auto opcode_it = begin(opcodes);
           opcode_it != end(opcodes); ++opcode_it) {
        auto &opcode = *opcode_it;
      if (opcode == "/")
          print_opcode = true;
      else if (print_opcode)
        fprintf(out_file, " %s", opcode.c_str());
    }
  }

  void print_opcode_recognition(const MarkedInstruction& instruction,
                                bool memory_access) {
    const auto& opcodes = instruction.get_opcodes();
    if (instruction.get_opcode_in_modrm()) {
      fprintf(out_file, " (");
      for (auto opcode = opcodes.rbegin(); opcode != opcodes.rend(); ++opcode)
        if ((*opcode) != "/" && opcode->find('/') != opcode->npos)
          fprintf(out_file, "opcode_%s", opcode->c_str() + 1);
    }
    if (enabled(Actions::kOpcode))
      fprintf(out_file, " @end_opcode");
    const auto& required_prefixes = instruction.get_required_prefixes();
    for (auto prefix_it = begin(required_prefixes);
           prefix_it != end(required_prefixes); ++prefix_it) {
      auto &prefix = *prefix_it;
      if (prefix == "0x66") {
        fprintf(out_file, " @not_data16_prefix");
        break;
      }
    }
    for (auto prefix_it = begin(required_prefixes);
         prefix_it != end(required_prefixes); ++prefix_it) {
      auto &prefix = *prefix_it;
      if (prefix == "0xf2") {
        fprintf(out_file, " @not_repnz_prefix");
        break;
      }
    }
    for (auto prefix_it = begin(required_prefixes);
         prefix_it != end(required_prefixes); ++prefix_it) {
      auto &prefix = *prefix_it;
      if (prefix == "0xf3") {
        fprintf(out_file, " @not_repz_prefix");
        break;
      }
    }
    if (enabled(Actions::kInstructionName))
      fprintf(out_file, " @instruction_%s",
                                  c_identifier(instruction.get_name()).c_str());
    else if (instruction.get_opcode_in_imm())
      fprintf(out_file, " @last_byte_is_not_immediate");
    else if (instruction.has_flag("nacl-amd64-modifiable") &&
             enabled(Actions::kParseOperands) &&
             !enabled(Actions::kParseOperandPositions))
      fprintf(out_file, " @modifiable_instruction");
    const auto& flags = instruction.get_flags();
    for (auto flag_it = begin(flags); flag_it != end(flags); ++flag_it) {
        auto &flag = *flag_it;
      if (!strncmp(flag.c_str(), "CPUFeature_", 11))
        fprintf(out_file, " @%s", flag.c_str());
    }
    const auto& operands = instruction.get_operands();
    if (enabled(Actions::kParseOperands)) {
      if (enabled(Actions::kParseOperandPositions))
        fprintf(out_file, " @operands_count_is_%zd", operands.size());
      int operand_index = 0;
      for (auto operand_it = begin(operands);
           operand_it != end(operands); ++operand_it) {
        auto &operand = *operand_it;
        if (operand.enabled)
          if (enabled(Actions::kParseOperandPositions) ||
              (((operand.source != 'E') && (operand.source != 'M') &&
                (operand.source != 'N') && (operand.source != 'Q') &&
                (operand.source != 'R') && (operand.source != 'U') &&
                (operand.source != 'W')) || !memory_access))
            fprintf(out_file, " @operand%zd_%s",
                                           operand_index, operand.size.c_str());
        static std::map<char, const char*> operand_type {
          { '1',        "one"                   },
          { 'a',        "rax"                   },
          { 'c',        "rcx"                   },
          { 'd',        "rdx"                   },
          { 'i',        "second_immediate"      },
          { 'o',        "port_dx"               },
          { 't',        "st"                    },
          { 'x',        "ds_rbx"                },
          { 'B',        "from_vex"              },
          { 'H',        "from_vex"              },
          { 'I',        "immediate"             },
          { 'O',        "absolute_disp"         },
          { 'X',        "ds_rsi"                },
          { 'Y',        "es_rdi"                }
        };
        auto it = operand_type.find(operand.source);
        if (it != end(operand_type)) {
          if (operand.enabled)
            fprintf(out_file, " @operand%zd_%s", operand_index, it->second),
            ++operand_index;
        } else if (operand.enabled) {
          if (enabled(Actions::kParseOperandPositions) ||
              (((operand.source != 'E') && (operand.source != 'M') &&
                (operand.source != 'N') && (operand.source != 'Q') &&
                (operand.source != 'R') && (operand.source != 'U') &&
                (operand.source != 'W')) || !memory_access))
            ++operand_index;
        }
      }
    }
    if (enabled(Actions::kParseOperandsStates)) {
      auto operand_index = 0;
      for (auto operand_it = begin(operands);
           operand_it != end(operands); ++operand_it) {
        auto &operand = *operand_it;
        if (operand.enabled) {
          if (operand.write)
            if (operand.read)
              fprintf(out_file, " @operand%zd_readwrite", operand_index);
            else
              fprintf(out_file, " @operand%zd_write", operand_index);
          else
            if (operand.read)
              fprintf(out_file, " @operand%zd_read", operand_index);
            else
              fprintf(out_file, " @operand%zd_unused", operand_index);
        }
        if (operand.enabled || enabled(Actions::kParseOperandPositions))
          ++operand_index;
      }
    }
    if (instruction.get_opcode_in_modrm())
      fprintf(out_file, " any* &");
  }

  void print_immediate_arguments(const MarkedInstruction& instruction) {
    const auto& operands = instruction.get_operands();
    auto operand_index = 0;
    for (auto operand_it = begin(operands);
         operand_it != end(operands); ++operand_it) {
      auto &operand = *operand_it;
      if (operand.enabled || enabled(Actions::kParseOperandPositions))
        ++operand_index;
    }
    for (auto operand = operands.rbegin(); operand != operands.rend();
                                                                    ++operand) {
      if (operand->source == 'i') {
        if (operand->size == "8bit")
          fprintf(out_file, " imm8n2");
        else if (operand->size == "16bit")
          fprintf(out_file, " imm16n2");
        else
          fprintf(stderr,
                    "%s: error - can not determine immediate size: %c%s",
                    short_program_name, operand->source, operand->size.c_str()),
          exit(1);
      } else if (operand->source == 'I') {
        if (operand->size == "2bit")
          fprintf(out_file, " imm2");
        else if (operand->size == "8bit")
          fprintf(out_file, " imm8");
        else if (operand->size == "16bit")
          fprintf(out_file, " imm16");
        else if (operand->size == "32bit")
          fprintf(out_file, " imm32");
        else if (operand->size == "64bit")
          fprintf(out_file, " imm64");
        else
          fprintf(stderr,
                    "%s: error - can not determine immediate size: %c%s",
                    short_program_name, operand->source, operand->size.c_str()),
          exit(1);
      } else if (operand->source == 'J') {
        if (operand->size == "8bit")
          fprintf(out_file, " rel8");
        else if (operand->size == "16bit")
          fprintf(out_file, " rel16");
        else if (operand->size == "32bit")
          fprintf(out_file, " rel32");
        else
          fprintf(stderr,
                    "%s: error - can not determine immediate size: %c%s",
                    short_program_name, operand->source, operand->size.c_str()),
          exit(1);
      } else if (operand->source == 'L') {
        if (operands.size() == 4) {
          fprintf(out_file, " b_%cxxx_0000", ia32_mode ? '0' : 'x');
          if (!enabled(Actions::kInstructionName))
            fprintf(out_file, " @last_byte_is_not_immediate");
        }
        if (operand->enabled && enabled(Actions::kParseOperands))
          fprintf(out_file, " @operand%zd_from_is4", operand_index - 1);
      } else if (operand->source == 'O') {
        fprintf(out_file, ia32_mode ? " disp32" : " disp64");
      }
      if (operand->enabled || enabled(Actions::kParseOperandPositions))
        --operand_index;
    }
    const auto& required_prefixes = instruction.get_required_prefixes();
    for (auto prefix_it = begin(required_prefixes);
           prefix_it != end(required_prefixes); ++prefix_it) {
      auto &prefix = *prefix_it;
      if (prefix == "0xf0") {
        for (auto operand_it = begin(operands);
             operand_it != end(operands); ++operand_it) {
          auto &operand = *operand_it;
          if (operand.source == 'C') {
            fprintf(out_file, " @not_lock_prefix%zd",
                                                  &operand - &*begin(operands));
            break;
          }
        }
        break;
      }
    }
  }

  void print_instruction_end_actions(const MarkedInstruction& instruction,
                                     bool memory_access) {
    if (enabled(Actions::kParseOperands)) {
      if (!enabled(Actions::kParseOperandPositions)) {
        int operands_count = 0;
        auto zero_extends = false;
        const auto& operands = instruction.get_operands();
        for (auto operand_it = begin(operands);
             operand_it != end(operands); ++operand_it) {
          auto& operand = *operand_it;
          if (operand.enabled)
            if (((operand.source != 'E') && (operand.source != 'M') &&
                 (operand.source != 'N') && (operand.source != 'Q') &&
                 (operand.source != 'R') && (operand.source != 'U') &&
                 (operand.source != 'W')) || !memory_access) {
              ++operands_count;
              if (operand.size == "32bit")
                zero_extends = true;
            }
        }
        if (operands_count == 1)
          fprintf(out_file, " @process_1_operand");
        else
          fprintf(out_file, " @process_%d_operands", operands_count);
        if (zero_extends) fprintf(out_file, "_zero_extends");
      }
    }
  }

  void print_one_size_definition_nomodrm(
                                      const MarkedInstruction& instruction) {
    print_operator_delimiter();
    auto opcodes = instruction.get_opcodes();
    if (opcodes[0] == "(0x90|0x91|0x92|0x93|0x94|0x95|0x96|0x97)")
      fprintf(out_file, "((");
    print_legacy_prefixes(instruction);
    print_rex_prefix(instruction);
    print_opcode_nomodrm(instruction);
    if (instruction.get_opcode_in_imm())
      print_immediate_opcode(instruction),
      print_opcode_recognition(instruction, false);
    else
      print_opcode_recognition(instruction, false),
      print_immediate_arguments(instruction);
    if (opcodes[0] == "(0x90|0x91|0x92|0x93|0x94|0x95|0x96|0x97)")
      fprintf(out_file, ") - (0x90|0x48 0x90))");
    print_instruction_end_actions(instruction, false);
  }

  void print_one_size_definition_modrm_register(
                                      const MarkedInstruction& instruction) {
    print_operator_delimiter();
    auto adjusted_instruction = instruction;
    if (mod_reg_is_used(adjusted_instruction))
      adjusted_instruction = adjusted_instruction.set_rex_r();
    if (mod_rm_is_used(adjusted_instruction))
      adjusted_instruction = adjusted_instruction.set_rex_b();
    print_legacy_prefixes(adjusted_instruction);
    print_rex_prefix(adjusted_instruction);
    print_opcode_nomodrm(adjusted_instruction);
    if (!adjusted_instruction.get_opcode_in_imm())
      print_opcode_recognition(adjusted_instruction, false);
    fprintf(out_file, " modrm_registers");
    if (enabled(Actions::kParseOperands)) {
      auto operand_index = 0;
      const auto& operands = adjusted_instruction.get_operands();
      for (auto operand_it = begin(operands);
           operand_it != end(operands); ++operand_it) {
        auto& operand = *operand_it;
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
          if (it != end(operand_type))
            fprintf(out_file, " @operand%zd_from_modrm_%s", operand_index,
                    it->second);
        }
        if (operand.enabled || enabled(Actions::kParseOperandPositions))
          ++operand_index;
      }
    }
    if (adjusted_instruction.get_opcode_in_modrm())
      fprintf(out_file, ")");
    if (adjusted_instruction.get_opcode_in_imm())
      print_immediate_opcode(adjusted_instruction),
      print_opcode_recognition(adjusted_instruction, false);
    else
      print_immediate_arguments(adjusted_instruction);
    print_instruction_end_actions(adjusted_instruction, false);
  }

  void print_one_size_definition_modrm_memory(
                                      const MarkedInstruction& instruction) {
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
      auto adjusted_instruction = instruction;
      if (mod_reg_is_used(adjusted_instruction))
        adjusted_instruction = adjusted_instruction.set_rex_r();
      if (mod_rm_is_used(adjusted_instruction)) {
        if (std::get<1>(mode))
          adjusted_instruction = adjusted_instruction.set_rex_x();
        if (std::get<2>(mode))
          adjusted_instruction = adjusted_instruction.set_rex_b();
      }
      print_legacy_prefixes(adjusted_instruction);
      print_rex_prefix(adjusted_instruction);
      print_opcode_nomodrm(adjusted_instruction);
      if (!adjusted_instruction.get_opcode_in_imm())
        print_opcode_recognition(adjusted_instruction, true);
      if (adjusted_instruction.get_opcode_in_modrm())
        fprintf(out_file, " any");
      else
        fprintf(out_file, " (any");
      if (enabled(Actions::kParseOperands)) {
        auto operand_index = 0;
        const auto& operands = adjusted_instruction.get_operands();
        for (auto operand_it = begin(operands);
             operand_it != end(operands); ++operand_it) {
          auto& operand = *operand_it;
          static const std::map<char, const char*> operand_type {
            { 'C',      "from_modrm_reg"        },
            { 'D',      "from_modrm_reg"        },
            { 'E',      "rm"                    },
            { 'G',      "from_modrm_reg"        },
            { 'M',      "rm"                    },
            { 'N',      "rm"                    },
            { 'P',      "from_modrm_reg"        },
            { 'Q',      "rm"                    },
            { 'R',      "rm"                    },
            { 'S',      "from_modrm_reg_norex"  },
            { 'U',      "rm"                    },
            { 'V',      "from_modrm_reg"        },
            { 'W',      "rm"                    }
          };
          if (operand.enabled) {
            auto it = operand_type.find(operand.source);
            if (it != end(operand_type)) {
              if (strcmp(it->second, "rm") != 0 ||
                  enabled(Actions::kParseOperandPositions)) {
                fprintf(out_file, " @operand%zd_%s", operand_index,
                        it->second);
                ++operand_index;
              }
            } else {
              ++operand_index;
            }
          } else if (enabled(Actions::kParseOperandPositions)) {
            ++operand_index;
          }
        }
      }
      fprintf(out_file, " . any* &%s", std::get<0>(mode));
      if (enabled(Actions::kCheckAccess) &&
          !adjusted_instruction.has_flag("no_memory_access"))
        fprintf(out_file, " @check_access");
      fprintf(out_file, ")");
      if (adjusted_instruction.get_opcode_in_imm())
        print_immediate_opcode(adjusted_instruction),
        print_opcode_recognition(adjusted_instruction, true);
      else
        print_immediate_arguments(adjusted_instruction);
      print_instruction_end_actions(adjusted_instruction, true);
    }
  }

  MarkedInstruction expand_sizes(const MarkedInstruction& instruction,
                                                           bool memory_access) {
    auto operands = instruction.get_operands();
    for (auto operand_it = begin(operands);
         operand_it != end(operands); ++operand_it) {
      auto& operand = *operand_it;
      static const std::map<std::pair<char, std::string>,
                         std::pair<const char*, const char*> > operand_sizes = {
        { { ' ', ""     },      { "xmm",        "128bit"                } },
        { { ' ', "2"    },      { "2bit",       nullptr                 } },
        { { ' ', "7"    },      { "x87",        nullptr                 } },
        { { ' ', "b"    },      { "8bit",       "8bit"                  } },
        { { ' ', "d"    },      { "32bit",      "32bit"                 } },
        { { ' ', "do"   },      { "ymm",        "256bit"                } },
        { { ' ', "dq"   },      { "xmm",        "128bit"                } },
        { { ' ', "fq"   },      { "ymm",        "256bit"                } },
        { { ' ', "o"    },      { "xmm",        "128bit"                } },
        { { ' ', "p"    },      { nullptr,      "farptr"                } },
        { { ' ', "pb"   },      { "mmx_xmm",    "mmx_xmm"               } },
        { { ' ', "pd"   },      { "mmx_xmm",    "mmx_xmm"               } },
        { { ' ', "pdw"  },      { "mmx_xmm",    "mmx_xmm"               } },
        { { ' ', "pdwx" },      { "ymm",        "256bit"                } },
        { { ' ', "pdx"  },      { "ymm",        "256bit"                } },
        { { ' ', "ph"   },      { "mmx_xmm",    "mmx_xmm"               } },
        { { ' ', "phx"  },      { "ymm",        "256bit"                } },
        { { ' ', "pi"   },      { "mmx_xmm",    "mmx_xmm"               } },
        { { ' ', "pix"  },      { "ymm",        "256bit"                } },
        { { ' ', "pj"   },      { "mmx_xmm",    "mmx_xmm"               } },
        { { ' ', "pjx"  },      { "ymm",        "256bit"                } },
        { { ' ', "pk"   },      { "mmx_xmm",    "mmx_xmm"               } },
        { { ' ', "pkx"  },      { "ymm",        "256bit"                } },
        { { ' ', "pq"   },      { "mmx_xmm",    "mmx_xmm"               } },
        { { ' ', "pqw"  },      { "mmx_xmm",    "mmx_xmm"               } },
        { { ' ', "pqwx" },      { "ymm",        "256bit"                } },
        { { ' ', "pqx"  },      { "ymm",        "256bit"                } },
        { { ' ', "ps"   },      { "mmx_xmm",    "mmx_xmm"               } },
        { { ' ', "psx"  },      { "ymm",        "256bit"                } },
        { { ' ', "pw"   },      { "mmx_xmm",    "mmx_xmm"               } },
        { { ' ', "pwx"  },      { "ymm",        "256bit"                } },
        { { ' ', "q"    },      { "64bit",      "64bit"                 } },
        { { 'N', "q"    },      { "mmx",        "64bit"                 } },
        { { 'P', "q"    },      { "mmx",        "64bit"                 } },
        { { 'Q', "q"    },      { "mmx",        "64bit"                 } },
        { { 'U', "q"    },      { "xmm",        "64bit"                 } },
        { { 'V', "q"    },      { "xmm",        "64bit"                 } },
        { { 'W', "q"    },      { "xmm",        "64bit"                 } },
        { { ' ', "r"    },      { "regsize",    "regsize"               } },
        { { 'C', "r"    },      { "creg",       nullptr                 } },
        { { 'D', "r"    },      { "dreg",       nullptr                 } },
        { { ' ', "s"    },      { nullptr,      "selector"              } },
        { { ' ', "sb"   },      { nullptr,      "x87_bcd"               } },
        { { ' ', "sd"   },      { nullptr,      "float64bit"            } },
        { { 'H', "sd"   },      { "xmm",        "float64bit"            } },
        { { 'L', "sd"   },      { "xmm",        "float64bit"            } },
        { { 'U', "sd"   },      { "xmm",        "float64bit"            } },
        { { 'V', "sd"   },      { "xmm",        "float64bit"            } },
        { { 'W', "sd"   },      { "xmm",        "float64bit"            } },
        { { ' ', "se"   },      { nullptr,      "x87_env"               } },
        { { ' ', "si"   },      { nullptr,      "x87_32bit"             } },
        { { ' ', "sq"   },      { nullptr,      "x87_64bit"             } },
        { { ' ', "sr"   },      { nullptr,      "x87_state"             } },
        { { ' ', "ss"   },      { nullptr,      "float32bit"            } },
        { { 'H', "ss"   },      { "xmm",        "float32bit"            } },
        { { 'L', "ss"   },      { "xmm",        "float32bit"            } },
        { { 'U', "ss"   },      { "xmm",        "float32bit"            } },
        { { 'V', "ss"   },      { "xmm",        "float32bit"            } },
        { { 'W', "ss"   },      { "xmm",        "float32bit"            } },
        { { ' ', "st"   },      { nullptr,      "float80bit"            } },
        { { ' ', "sw"   },      { nullptr,      "x87_16bit"             } },
        { { ' ', "sx"   },      { nullptr,      "x87_mmx_xmm_state"     } },
        { { ' ', "w"    },      { "16bit",      "16bit"                 } },
        { { 'S', "w"    },      { "segreg",     nullptr                 } },
        { { ' ', "x"    },      { "ymm",        "256bit"                } },
      };
      auto it = operand_sizes.find(make_pair(operand.source, operand.size));
      if (it == end(operand_sizes))
        it = operand_sizes.find(make_pair(' ', operand.size));
      if (it != end(operand_sizes)) {
        bool memory_operand = memory_access &&
                              (operand.source == 'E' ||
                               operand.source == 'M' ||
                               operand.source == 'Q' ||
                               operand.source == 'W');
        auto operand_size = memory_operand ?
                              it->second.second : /* Memory size.   */
                              it->second.first;   /* Register size. */
        if (operand_size != nullptr) {
          if (strcmp(operand_size, "mmx_xmm") == 0) {
            static const std::map<char, const char*> mmx_xmm_sizes {
              { 'a',    "xmm"           }, /* %xmm0 */
              { 'H',    "xmm"           },
              { 'L',    "xmm"           },
              { 'M',    "128bit"        },
              { 'N',    "mmx"           },
              { 'P',    "mmx"           },
              { 'Q',    "mmx"           },
              { 'U',    "xmm"           },
              { 'V',    "xmm"           },
              { 'W',    "xmm"           }
            };
            auto it_mmx_xmm = mmx_xmm_sizes.find(operand.source);
            if (it_mmx_xmm != end(mmx_xmm_sizes))
              operand_size = it_mmx_xmm->second;
            else
              fprintf(stderr,
                      "%s: error - can not determine operand size: %c%s",
                      short_program_name, operand.source, operand.size.c_str()),
              exit(1);
          }
          operand.size = operand_size;
          if ((!enabled(Actions::kParseImmediateOperands) &&
               (operand.source == 'I' || operand.source == 'i')) ||
              (!enabled(Actions::kParseRelativeOperands) &&
               (operand.source == 'J' || operand.source == 'O')) ||
              (!enabled(Actions::kParseX87Operands) &&
                (strncmp(operand_size, "x87", 3) == 0 ||
                 strncmp(operand_size, "float", 5) == 0)) ||
              (!enabled(Actions::kParseMMXOperands) &&
                strcmp(operand_size, "mmx") == 0) ||
              (!enabled(Actions::kParseXMMOperands) &&
                strcmp(operand_size, "xmm") == 0) ||
              (!enabled(Actions::kParseYMMOperands) &&
                strcmp(operand_size, "ymm") == 0) ||
              (!enabled(Actions::kParseNonWriteRegisters) &&
                !operand.write))
            operand.enabled = false;
        } else {
          fprintf(stderr, "%s: error - can not determine operand size: %c%s",
                      short_program_name, operand.source, operand.size.c_str());
          exit(1);
        }
      } else {
        fprintf(stderr, "%s: error - can not determine operand size: %c%s",
                      short_program_name, operand.source, operand.size.c_str());
        exit(1);
      }
    }
    return instruction.replace_operands(operands);
  }

  void print_one_size_definition(const MarkedInstruction& instruction) {
    /* 64bit commands are not supported in ia32 mode.  */
    if (ia32_mode && instruction.get_rex().w) return;

    bool modrm_memory = false;
    bool modrm_register = false;
    char operand_source = ' ';
    const auto& operands = instruction.get_operands();
    for (auto operand_it = begin(operands);
         operand_it != end(operands); ++operand_it) {
      auto& operand = *operand_it;
      static std::map<char, std::pair<bool, bool> > operand_map {
        { 'E', std::make_pair(true,  true)  },
        { 'M', std::make_pair(true,  false) },
        { 'N', std::make_pair(false, true)  },
        { 'Q', std::make_pair(true,  true)  },
        { 'R', std::make_pair(false, true)  },
        { 'U', std::make_pair(false, true)  },
        { 'W', std::make_pair(true,  true)  }
      };
      auto it = operand_map.find(operand.source);
      if (it != end(operand_map)) {
        if ((modrm_memory || modrm_register) &&
            ((modrm_memory != it->second.first) ||
             (modrm_register != it->second.second)))
          fprintf(stderr,
                  "%s: error - conflicting operand sources: '%c' and '%c'",
                  short_program_name, operand_source, operand.source),
          exit(1);
        modrm_memory = it->second.first;
        modrm_register = it->second.second;
        operand_source = operand.source;
      }
    }
    if (modrm_memory || modrm_register) {
      if (modrm_memory) {
        auto instruction_with_expanded_sizes = expand_sizes(instruction, true);
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

  void print_instruction_px(const MarkedInstruction& instruction) {
    auto operands = instruction.get_operands();
    for (auto operand_it = begin(operands);
         operand_it != end(operands); ++operand_it) {
      auto& operand = *operand_it;
      if ((*operand.size.rbegin() == 'x') &&
          ((operand.size.length() == 1) || (*begin(operand.size) == 'p'))) {
        for (auto operand_it = begin(operands);
             operand_it != end(operands); ++operand_it) {
          auto& operand = *operand_it;
          if ((*operand.size.rbegin() == 'x') &&
              ((operand.size.length() == 1) || (*begin(operand.size) == 'p')))
            operand.size.resize(operand.size.length() - 1);
        }
        auto opcodes = instruction.get_opcodes();
        for (auto opcode_it = begin(opcodes);
               opcode_it != end(opcodes); ++opcode_it) {
          auto &opcode = *opcode_it;
          auto Lbit = opcode.find(".L.");
          if (Lbit != opcode.npos) {
            opcode[++Lbit] = '1';
            print_one_size_definition(instruction.replace_opcodes(opcodes));
            opcode[Lbit] = '0';
            print_one_size_definition(instruction.
                                      replace_opcodes(opcodes).
                                      replace_operands(operands));
            return;
          }
        }
        fprintf(stderr, "%s: error - can not set 'L' bit in instruction '%s'",
                            short_program_name, instruction.get_name().c_str());
        exit(1);
      }
    }
    print_one_size_definition(instruction);
  }

  void print_instruction_vyz(const MarkedInstruction& instruction) {
    const auto& operands = instruction.get_operands();
    for (auto operand_it = begin(operands);
         operand_it != end(operands); ++operand_it) {
      auto& operand = *operand_it;
      if (operand.size == "v" || operand.size == "z") {
        print_instruction_px(instruction.add_requred_prefix("data16"));
        print_instruction_px(instruction.clear_rex_w());
        auto instruction_w = instruction.set_rex_w();
        print_instruction_px(instruction_w);
        if (enabled(Actions::kNaClForbidden))
          print_instruction_px(instruction_w.add_requred_prefix("data16"));
        return;
      }
    }
    for (auto operand_it = begin(operands);
         operand_it != end(operands); ++operand_it) {
      auto& operand = *operand_it;
      if (operand.size == "y" ||
          (operand.source == 'S' && operand.size == "w")) {
        print_instruction_px(instruction.clear_rex_w());
        auto instruction_w = instruction.set_rex_w();
        print_instruction_px(instruction.set_rex_w());
        return;
      }
    }
    print_instruction_px(instruction);
    const auto& required_prefixes = instruction.get_required_prefixes();
    if (!instruction.has_flag("norex") && !instruction.has_flag("norexw") &&
        !instruction.get_rex().w &&
        required_prefixes.find("data16") == end(required_prefixes))
      print_instruction_px(instruction.set_rex_w());
  }

  void print_one_instruction_definition(void) {
    for (auto instruction_it = begin(instructions);
         instruction_it != end(instructions); ++instruction_it) {
      auto& instruction = *instruction_it;
      MarkedInstruction marked_instruction(instruction);
      auto operands = marked_instruction.get_operands();
      bool found_operand_with_empty_size = false;
      for (auto it = begin(operands); it != end(operands); ++it) {
        if (it->size == "") {
          found_operand_with_empty_size = true;
          break;
        }
      }
      if (found_operand_with_empty_size) {
        for (auto operand_it = begin(operands);
             operand_it != end(operands); ++operand_it) {
          auto& operand = *operand_it;
          if (operand.size == "") operand.size = "b";
        }
        print_instruction_vyz(marked_instruction.replace_operands(operands));
        auto opcodes = marked_instruction.get_opcodes();
        for (auto opcode = opcodes.rbegin(); opcode != opcodes.rend(); ++opcode)
          if (opcode->find('/') == opcode->npos) {
            /* 'w' bit is last bit both in binary and textual form.  */
            *(opcode->rbegin()) += 0x1;
            break;
          }
        operands = marked_instruction.get_operands();
        for (auto operand_it = begin(operands);
             operand_it != end(operands); ++operand_it) {
          auto& operand = *operand_it;
          if (operand.size == "") {
            if (operand.source == 'I')
              operand.size = "z";
            else
              operand.size = "v";
          }
        }
        print_instruction_vyz(marked_instruction.
                              replace_opcodes(opcodes).
                              replace_operands(operands));
      } else {
        print_instruction_vyz(marked_instruction);
      }
    }
  }

  struct compare_action {
    const char* action;

    explicit compare_action(const char* y) : action(y) {
    }

    bool operator()(const char* x) const {
      return !strcmp(x, action);
    }
  };

}

int main(int argc, char *argv[]) {
  /* basename(3) may change the passed argument thus we are using copy
     of argv[0].  This creates tiny memory leak but since we only do that
     once per program invocation it's contained.  */
  short_program_name = basename(strdup(argv[0]));

  current_dir_name = get_current_dir_name();

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
    size_t current_dir_name_len = strlen(current_dir_name);
    if (out_file_name && !const_file_name) {
      const_name_len = strlen(out_file_name) + 10;
      const_file_name = static_cast<char *>(malloc(const_name_len));
      strcpy(const_cast<char *>(const_file_name), out_file_name);
      const char* dot_position = strrchr(const_file_name, '.');
      if (!dot_position) {
        dot_position = strrchr(const_file_name, '\0');
      }
      strcpy(const_cast<char *>(dot_position), "_consts.c");
    }
    if (!(const_file = fopen(const_file_name, "w"))) {
      fprintf(stderr, "%s: can not open '%s' file (%s)\n",
              short_program_name, const_file_name, strerror(errno));
       return 1;
    }
    fprintf(out_file, "/* native_client/%s\n"
" * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.\n"
" * Compiled for %s mode.\n"
" */\n\n", strncmp(current_dir_name, out_file_name, current_dir_name_len) ?
             out_file_name :
             out_file_name + current_dir_name_len + 1,
           ia32_mode ? "ia32" : "x86-64");
    fprintf(const_file, "/* native_client/%s\n"
" * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.\n"
" * Compiled for %s mode.\n"
" */\n\n", strncmp(current_dir_name, const_file_name, current_dir_name_len) ?
             const_file_name :
             const_file_name + current_dir_name_len + 1,
           ia32_mode ? "ia32" : "x86-64");
    if (const_name_len) {
      free(const_cast<char *>(const_file_name));
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
