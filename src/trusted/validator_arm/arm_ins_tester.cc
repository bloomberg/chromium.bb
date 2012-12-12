/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif

// This tester processes line based data from stdin
// The format is <address>: opcode
// address is optional and defaults to 0x20004
// address and opcode must both be hex numbers (with or without "0x")
// The tester will report any problems with the instruction
// and whether and how it needs to be sandboxed.
//
// For quick command line checks, the following bash feature comes in handy:
//  scons-out/opt-linux-x86-32/staging/arm_ins_tester <<< ": 0xff66666"

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/validator_arm/cpuid_arm.h"
#include "native_client/src/trusted/validator_arm/model.h"
#include "native_client/src/trusted/validator_arm/problem_reporter.h"
#include "native_client/src/trusted/validator_arm/validator.h"


using std::string;
using std::vector;


int kDefaultBaseAddr = 0x20004;
const uint32_t kOneGig = 1U * 1024 * 1024 * 1024;
static const size_t kBufferSize = 256;


void Fatal(const string& message) {
  std::cout << message << "\n";
  exit(1);
}


class NcvalProblemReporter : public nacl_arm_val::ProblemReporter {
 public:
  virtual bool should_continue() {
      return num_reported_errors_ == 0;
  }

  void reset(uint32_t address, uint32_t instruction) {
      address_ = address;
      instruction_ = instruction;
      num_reported_errors_ = 0;
  }

 private:
  int address_;
  int instruction_;
  int num_reported_errors_;

  void report(const char* message) {
    printf("%08x: %08x [%s]\n", address_, instruction_, message);
    ++num_reported_errors_;
  }

 protected:
  virtual void ReportProblemInternal(
      uint32_t vaddr,
      nacl_arm_val::ValidatorProblem problem,
      nacl_arm_val::ValidatorProblemMethod method,
      nacl_arm_val::ValidatorProblemUserData user_data) {
      UNREFERENCED_PARAMETER(vaddr);
      UNREFERENCED_PARAMETER(method);
      UNREFERENCED_PARAMETER(user_data);

    switch (problem) {
      // problems which can be infered by looking at just the instruction
      case nacl_arm_val::kProblemUnsafe:
      case nacl_arm_val::kProblemIllegalPcLoadStore:
      case nacl_arm_val::kProblemReadOnlyRegister:
      case nacl_arm_val::kProblemIllegalUseOfThreadPointer:
        report("invalid");
        break;
        // problems which can be infered by looking at context/addresses
          // we punt on these for now
      case nacl_arm_val::kProblemBranchSplitsPattern:
      case nacl_arm_val::kProblemBranchInvalidDest:
      case nacl_arm_val::kProblemPatternCrossesBundle:
      case nacl_arm_val::kProblemMisalignedCall:
        break;
        // problems which can be infered by looking at context
        // we use this to determined that an instruction needs masking
      case nacl_arm_val::kProblemUnsafeLoadStore:
        report("data-pre-mask-needed");
        break;
      case nacl_arm_val::kProblemUnsafeDataWrite:
        report("data-post-mask-needed");
        break;
      case nacl_arm_val::kProblemUnsafeBranch:
        report("code-pre-mask-needed");
        break;
      case nacl_arm_val::kProblemConstructionFailed:
        Fatal("ERROR: could not initialize validator\n");
        break;
        case nacl_arm_val::kValidatorProblemSize:
          break;
        // do not use "default:"
        // we cover all cases in this switch statement
        // so that we get compile complaints when new enums are added
    }
  }
};


void TestOneInstruction(int address,
                        int instruction,
                        nacl_arm_val::SfiValidator& validator,
                        NcvalProblemReporter& reporter) {
    reporter.reset(address, instruction);
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&instruction);

    nacl_arm_val::CodeSegment segment(bytes, address, 4);
    vector<nacl_arm_val::CodeSegment> segments;
    segments.push_back(segment);

    validator.validate(segments, &reporter);
}


void SplitString(const string& s, vector<string>* tokens) {
  std::istringstream ss(s);
  copy(std::istream_iterator<string>(ss),
       std::istream_iterator<string>(),
       std::back_inserter<vector<string> >(*tokens));
}


int main() {
  NaClCPUFeaturesArm cpu_features;
  NaClClearCPUFeaturesArm(&cpu_features);
  // TODO(robertm): make this controllable via command line switches, e.f.:
  // NaClSetCPUFeatureArm(&cpu_features, NaClCPUFeatureArm_CanUseTstMem, 1);

  nacl_arm_val::SfiValidator validator(
      16,  // bytes per bundle
      kOneGig,  // code region size
      kOneGig,  // data region size
      nacl_arm_dec::RegisterList(nacl_arm_dec::Register::Tp()),
      nacl_arm_dec::RegisterList(nacl_arm_dec::Register::Sp()),
      &cpu_features);

  NcvalProblemReporter reporter;

  std::string line;
  while (std::getline(std::cin, line)) {
    // skip comments and blank lines
    if (line[0] == '#' || line.size() == 0) continue;
    vector<string> tokens;
    SplitString(line, &tokens);
    if (tokens.size() < 2 ||
        ':' != *tokens[0].rbegin()) {
        Fatal("bad line: " + line);
    }

    // chop of the trailing colon
    tokens[0] = tokens[0].substr(0, tokens[0].size() - 1);
    uint32_t address = kDefaultBaseAddr;
    char* last;
    if (tokens[0].size() > 1) {
        address = strtoul(tokens[0].c_str(), &last, 16);
        if ('\0' != *last) {
            Fatal("address must be valid hex number: " + line);
        }
    }
    uint32_t instruction = strtoul(tokens[1].c_str(), &last, 16);
    if ('\0' != *last) {
        Fatal("instruction must be valid hex number: " + line);
    }

    TestOneInstruction(address, instruction, validator, reporter);
  }

  return 0;
}
