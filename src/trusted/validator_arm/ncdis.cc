/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* Implements an ARM disassembler (modulo instructions recognized by
 * the NACL validator).
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include <iostream>
#include <fstream>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/validator_x86/ncfileutil.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/trusted/validator_arm/arm_insts_rt.h"
#include "native_client/src/trusted/validator_arm/ncdecode.h"
#include "native_client/src/trusted/validator_arm/segment_parser.h"
#include "native_client/src/trusted/validator_arm/string_split.h"

#define BUFFER_SIZE 256

static const char *progname;

// Counts the number of (non-fatal) errors found
// while running the application.
static size_t number_errors = 0;

// Reports a non-fatal error. Assumes caller can recover
// and continue.
static void error(const char *fmt, ...) {
  va_list ap;
  fprintf(stderr, "%s: error: ", progname);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  ++number_errors;
}

// Reports a fatal error. Assumes that caller can't recover
// and processing should terminate.
static void fatal(const char *fmt, ...) {
  va_list ap;
  fprintf(stderr, "%s: fatal error: ", progname);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(++number_errors);
}

/*
// Print the decoded instruction.
*/
static void PrintInst(const struct NcDecodedInstruction *mstate) {
  char print_buffer[BUFFER_SIZE];
  DescribeInst(print_buffer, sizeof(print_buffer), mstate);
  fprintf(stdout, "%8x:\t%08x \t%s\n", mstate->vpc, mstate->inst, print_buffer);
}

// dissassemble the given code segment.
static void DisassembleCodeSegment(CodeSegment* code_segment) {
  NcDecodeState state(*code_segment);
  for (state.GotoStartPc(); state.HasValidPc(); state.NextInstruction()) {
    PrintInst(&(state.CurrentInstruction()));
  }
}

/*
// Analyze the sections in an executable file.
*/
static int DisassembleSections(ncfile *ncf) {
  int badsections = 0;
  int ii;
  Elf_Shdr *shdr = ncf->sheaders;

  for (ii = 0; ii < ncf->shnum; ii++) {
    printf("section %d sh_addr %x offset %x flags %x\n",
           ii, (uint32_t)shdr[ii].sh_addr,
           (uint32_t)shdr[ii].sh_offset, (uint32_t)shdr[ii].sh_flags);
    if ((shdr[ii].sh_flags & SHF_EXECINSTR) != SHF_EXECINSTR)
      continue;
    printf("parsing section %d\n", ii);
    CodeSegment code_segment;
    ElfCodeSegmentInitialize(&code_segment, shdr, ii, ncf);
    DisassembleCodeSegment(&code_segment);
  }
  return -badsections;
}

/*
// Analyze the code segments from an executable file.
*/
static void DisassembleNcFile(ncfile *ncf, const char *fname) {
  if (DisassembleSections(ncf) < 0) {
    fprintf(stderr, "%s: text validate failed\n", fname);
  }
}

/*
 * When non-zero, flag contains instruction to unit test and decode.
 */
static uint32_t FLAGS_decode_instruction = 0;

/*
 * Flag defining the value of the pc to use when decoding an instruction
 * through the FLAGS_decode_instruction.
 */
static uint32_t FLAGS_decode_pc = 0;

/*
 * Flag defining if we should print OpInfo data when decoding an
 * instruction.
 */
static Bool FLAGS_show_op_info = FALSE;

/*
 * Flag defining an input file to use as command line arguments
 * (one per input line). When specified, run the disassembler
 * on each command line. "" denotes no command file specified.
 * "-" denotes standard input should be used as the command file.
 */
static char* FLAGS_commands = const_cast<char*>("");

/*
 * Flag defining an input file to use to generate a code segment
 * to decode. "-' denotes standard input should be used to
 * parse the code segment to decode.
 */
static char* FLAGS_decode_segment = const_cast<char*>("");

/*
 * Flag when used in combination with commands flags, will turn
 * on input copy rules, making the generated output contain
 * comments and the command line arguments as part of the corresponding
 * generated output. For more details on this, see ProcessInputFile
 * below.
 */
static Bool FLAGS_self_document = FALSE;

/*
 * Store default values of flags on first call. On subsequent
 * calls, resets the flags to the default value.
 *
 * *WARNING* In order for this to work, this function must
 * be called before GrokFlags.
 */
static void ResetFlags() {
  static Bool DEFAULT_name_cond = FLAGS_name_cond;
  static uint32_t DEFAULT_decode_instruction = FLAGS_decode_instruction;
  static uint32_t DEFAULT_decode_pc = FLAGS_decode_pc;
  static Bool DEFAULT_show_op_info = FLAGS_show_op_info;
  static char* DEFAULT_commands = FLAGS_commands;
  static char* DEFAULT_decode_segment = FLAGS_decode_segment;
  static Bool DEFAULT_self_document = FLAGS_self_document;
  FLAGS_name_cond = DEFAULT_name_cond;
  FLAGS_decode_instruction = DEFAULT_decode_instruction;
  FLAGS_decode_pc = DEFAULT_decode_pc;
  FLAGS_show_op_info = DEFAULT_show_op_info;
  FLAGS_commands = DEFAULT_commands;
  FLAGS_decode_segment = DEFAULT_decode_segment;
  FLAGS_self_document = DEFAULT_self_document;
}

/*
 * Process command line arguments to remove options from
 * argv. Return the new argc value after the options have
 * been removed (and processed).
 */
static int GrokFlags(int argc, const char *argv[]) {
  int i;
  int new_argc = (argc == 0 ? 0 : 1);
  for (i = 1; i < argc; ++i) {
    if (GrokBoolFlag("--name_cond", argv[i],
                     &FLAGS_name_cond) ||
        GrokUint32HexFlag("--decode_instruction", argv[i],
                     &FLAGS_decode_instruction) ||
        GrokUint32HexFlag("--decode_pc", argv[i],
                          &FLAGS_decode_pc) ||
        GrokCstringFlag("--commands", argv[i],
                        &FLAGS_commands) ||
        GrokBoolFlag("--show_op_info", argv[i],
                     &FLAGS_show_op_info) ||
        GrokCstringFlag("--decode_segment", argv[i],
                        &FLAGS_decode_segment) ||
        GrokBoolFlag("--self_document", argv[i],
                     &FLAGS_self_document)) {
    } else {
      /* Default to not a flag. */
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}

/*
 * process the command line arguments.
 */
static const char* GrokArgv(int argc, const char *argv[]) {
  progname = argv[0];

  if (2 != argc) {
    fatal("expected: '%s executable'", progname);
  }
  return argv[argc-1];
}

// Maximum number of argument that can appear on a line, in the commands
// file.
static const size_t kMaxArgc = 1024;

static void ProcessCommandLine(int argc, const char* argv[]);

// Given the input stream of commands (one per line), process each
// line of command line arguments, and process them.
//
// Note: If flag --self_document is specified, comment lines and
// whitespace lines will automatically be copied to stdout. In
// addition, command line arguments (up to the # of the comment character)
// will be copied to stdout (without a trialing newline), assuming
// that the output of the command line argumets is a single line. Hence, to self
// document the expected output, one should follow each line of commmand
// line arguments with a comment describing the expected output. If done
// correctly, this should allow the input file to be both the input, and
// a gold standard for comparing against the output of the application run.
static void ProcessInputFile(std::istream& input_file) {
  // Process each input line like a list of command line arguments.
  static nacl::string whitespace(" \t");
  const Bool self_document = FLAGS_self_document;
  nacl::string input_line;
  while (std::getline(input_file, input_line)) {
    // Tokenize the command line and process.
    nacl::string commands(input_line);  // extracted commands

    // Start by removing trailing comments.
    nacl::string::size_type idx = commands.find("#");
    if (idx != nacl::string::npos) {
      commands = commands.substr(0, idx);
    }

    // Pull out arguments
    std::vector<nacl::string> args;
    SplitStringUsing(commands, whitespace.c_str(), &args);

    // If command line arguments found, build new argc/argv and
    // (recursively) process.
    if (args.empty()) {
      if (self_document) {
        printf("%s\n", input_line.c_str());
      }
    } else {
      if (self_document) {
        printf("%s#", commands.c_str());
      }
      if (args.size() >= kMaxArgc - 1) {
        error("Too many command line arguments (ignoring): %s\n",
              input_line.c_str());
        continue;
      }
      // Convert to argc/argv model.
      static const char* new_args[1024];
      int new_argc = args.size() + 1;
      new_args[0] = progname;
      for (int i = 0; i < new_argc; ++i) {
        new_args[i+1] = args[i].c_str();
      }
      ProcessCommandLine(new_argc, new_args);
    }
  }
}

// Run the disassembler using the given command line arguments.
static void ProcessCommandLine(int argc, const char* argv[]) {
  ResetFlags();
  argc = GrokFlags(argc, argv);
  if (strcmp(FLAGS_commands, "") != 0) {
    if (strcmp(FLAGS_commands, "-") == 0) {
      ProcessInputFile(std::cin);
    } else {
      std::ifstream input_file(FLAGS_commands);
      if (!input_file) {
        error("Can't open commands file: %s", FLAGS_commands);
      } else {
        ProcessInputFile(input_file);
        input_file.close();
      }
    }
  } else if (strcmp(FLAGS_decode_segment, "") != 0) {
    std::istream* input = NULL;
    if (strcmp(FLAGS_decode_segment, "-") == 0) {
      input = &std::cin;
    } else {
      input = new std::ifstream(FLAGS_decode_segment);
      if (!input) {
        error("Can't open code segment: %s", FLAGS_decode_segment);
        delete input;
        input = NULL;
      }
    }
    if (NULL != input) {
      SegmentParser segment_parser(input);
      CodeSegment code_segment;
      segment_parser.Initialize(&code_segment);
      DisassembleCodeSegment(&code_segment);
      if (input != &std::cin) {
        delete input;
      }
    }
  } else if (FLAGS_decode_instruction == 0) {
    const char *loadname = GrokArgv(argc, argv);
    ncfile *ncf;

    if (NULL != loadname) {
      fprintf(stdout, "load: %s\n", loadname);

      ncf = nc_loadfile(loadname);
      if (NULL == ncf) {
        fatal("nc_loadfile(%s): %s\n", strerror(errno));
      }

      DisassembleNcFile(ncf, loadname);

      nc_freefile(ncf);
    }
  } else {
    CodeSegment code_segment;
    CodeSegmentInitialize(
        &code_segment,
        reinterpret_cast<uint8_t*>((&FLAGS_decode_instruction)),
        FLAGS_decode_pc,
        4);
    NcDecodeState state(code_segment);
    state.GotoStartPc();
    PrintInst(&(state.CurrentInstruction()));
    if (FLAGS_show_op_info) {
      char buffer[1024];
      DescribeOpInfo(buffer,
                     sizeof(buffer),
                     state.CurrentInstruction().matched_inst);
      printf("OpInfo: %s\n", buffer);
      DescribeInstValues(buffer, sizeof(buffer),
                         &(state.CurrentInstruction().values),
                         state.CurrentInstruction().matched_inst->inst_type);
      printf("values: %s\n", buffer);
    }
  }
}

/*
// Run the disassembler on the given arguments.
*/
int main(int argc, const char *argv[]) {
  ProcessCommandLine(argc, argv);
  return number_errors;
}
