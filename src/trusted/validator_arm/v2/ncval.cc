/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 * Copyright 2009, Google Inc.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <vector>
#include <algorithm>

#include "native_client/src/trusted/validator_arm/v2/model.h"
#include "native_client/src/trusted/validator_arm/v2/validator.h"
#include "native_client/src/trusted/validator_x86/ncfileutil.h"

using nacl_arm_val::SfiValidator;
using nacl_arm_val::CodeSegment;

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
class CommandLineProblemSink : public nacl_arm_val::ProblemSink {
 public:
  virtual void report_problem(uint32_t vaddr,
                              nacl_arm_dec::SafetyLevel safety,
                              const std::string &problem_code,
                              uint32_t ref_vaddr) {
    fprintf(stderr, "ncval: %08X %d %s %08X\n", vaddr, safety,
        problem_code.c_str(), ref_vaddr);
  }
  virtual bool should_continue() {
    // Collect *all* problems before returning!
    return true;
  }
};

int validate(const ncfile *ncf) {
  SfiValidator validator(
      16,  // bytes per bundle
      2U * 1024 * 1024 * 1024,  // code region size
      2U * 1024 * 1024 * 1024,  // data region size
      nacl_arm_dec::Register(9),  // read only register
      nacl_arm_dec::Register(13));  // stack pointer
  CommandLineProblemSink sink;

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

  bool success = validator.validate(segments, &sink);
  if (!success) return 1;
  return 0;
}

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
    return 2;
  }

  const char *filename = argv[1];

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
