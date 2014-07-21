// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pack ARM relative relocations into a more compact form.
//
// Applies two packing strategies.  The first is run-length encoding, which
// turns a large set of ARM relative relocations into a much smaller set
// of delta-count pairs, prefixed with a two-word header comprising the
// count of pairs and the initial relocation offset.  The second is LEB128
// encoding, which compacts the result of run-length encoding.
//
// Once packed, data is prefixed by an identifier that allows for any later
// versioning of packing strategies.
//
// A complete packed stream might look something like:
//
//   "APR1"   pairs  init_offset count1 delta1 count2 delta2 ...
//   41505231 f2b003 b08ac716    e001   04     01     10     ...

#ifndef TOOLS_RELOCATION_PACKER_SRC_PACKER_H_
#define TOOLS_RELOCATION_PACKER_SRC_PACKER_H_

#include <stdint.h>
#include <vector>

#include "elf.h"
#include "elf_traits.h"

namespace relocation_packer {

// A RelocationPacker packs vectors of ARM relative relocations into more
// compact forms, and unpacks them to reproduce the pre-packed data.
class RelocationPacker {
 public:
  // Pack ARM relative relocations into a more compact form.
  // |relocations| is a vector of ARM relative relocation structs.
  // |packed| is the vector of packed bytes into which relocations are packed.
  static void PackRelativeRelocations(const std::vector<ELF::Rel>& relocations,
                                      std::vector<uint8_t>* packed);

  // Unpack ARM relative relocations from their more compact form.
  // |packed| is the vector of packed relocations.
  // |relocations| is a vector of unpacked ARM relative relocation structs.
  static void UnpackRelativeRelocations(const std::vector<uint8_t>& packed,
                                        std::vector<ELF::Rel>* relocations);
};

}  // namespace relocation_packer

#endif  // TOOLS_RELOCATION_PACKER_SRC_PACKER_H_
