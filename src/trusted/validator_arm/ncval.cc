/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <string>
#include <vector>
#include <algorithm>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/validator/ncfileutil.h"
#include "native_client/src/trusted/validator_arm/model.h"
#include "native_client/src/trusted/validator_arm/problem_reporter.h"
#include "native_client/src/trusted/validator_arm/validator.h"

using nacl_arm_val::SfiValidator;
using nacl_arm_val::CodeSegment;
using nacl_arm_val::ProblemReporter;
using nacl_arm_val::ValidatorProblem;
using nacl_arm_val::kValidatorProblemSize;
using nacl_arm_dec::MAY_BE_SAFE;

using std::string;
using std::vector;


/*
 * Reports problems in an easily-parsed textual format, for consumption by a
 * validation-reporting script.
 *
 * The format is as follows:
 *   ncval: <hex vaddr> <decimal safety> <problem ID string> <hex ref vaddr>
 *
 * For possible safety levels, see inst_classes.h.
 *
 * For possible problem ID strings, see validator.h.
 */
class NcvalProblemReporter : public ProblemReporter {
 public:
  virtual bool should_continue() {
    // Collect *all* problems before returning!
    return true;
  }

 protected:
  virtual void ReportProblemInternal(
      uint32_t vaddr,
      nacl_arm_val::ValidatorProblem problem,
      nacl_arm_val::ValidatorProblemMethod method,
      nacl_arm_val::ValidatorProblemUserData user_data);
};

static const size_t kBufferSize = 256;

void NcvalProblemReporter::
ReportProblemInternal(uint32_t vaddr,
                      nacl_arm_val::ValidatorProblem problem,
                      nacl_arm_val::ValidatorProblemMethod method,
                      nacl_arm_val::ValidatorProblemUserData user_data) {
  char buffer[kBufferSize];
  ToText(buffer, kBufferSize, vaddr, problem, method, user_data);
  fprintf(stderr, "%s\n", buffer);
}

const uint32_t kOneGig = 1U * 1024 * 1024 * 1024;

int validate(const ncfile *ncf) {
  SfiValidator validator(
      16,  // bytes per bundle
      // TODO(cbiffle): maybe check region sizes from ELF headers?
      //                verify that instructions are in right region
      kOneGig,  // code region size
      kOneGig,  // data region size
      nacl_arm_dec::RegisterList(nacl_arm_dec::Register::Tp()),
      nacl_arm_dec::RegisterList(nacl_arm_dec::Register::Sp()));

  NcvalProblemReporter reporter;

  Elf_Shdr *shdr = ncf->sheaders;

  vector<CodeSegment> segments;
  for (int i = 0; i < ncf->shnum; i++) {
    if ((shdr[i].sh_flags & SHF_EXECINSTR) != SHF_EXECINSTR) {
      continue;
    }

    CodeSegment segment(ncf->data + (shdr[i].sh_addr - ncf->vbase),
        shdr[i].sh_addr, shdr[i].sh_size);
    segments.push_back(segment);
  }

  std::sort(segments.begin(), segments.end());

  bool success = validator.validate(segments, &reporter);
  if (!success) return 1;
  return 0;
}

int main(int argc, const char *argv[]) {
  const char *filename = NULL;

  for (int i = 1; i < argc; ++i) {
    string o = argv[i];
    if (filename != NULL) {
      // trigger error when filename is overwritten
      filename = NULL;
      break;
    }
    filename = argv[i];
  }

  if (NULL == filename) {
    fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
    return 2;
  }

  ncfile *ncf = nc_loadfile(filename);
  if (!ncf) {
    fprintf(stderr, "Unable to load %s: %s\n", filename, strerror(errno));
    return 1;
  }

  // TODO(cbiffle): check OS ABI, ABI version, align mask

  int exit_code = validate(ncf);
  nc_freefile(ncf);
  return exit_code;
}
