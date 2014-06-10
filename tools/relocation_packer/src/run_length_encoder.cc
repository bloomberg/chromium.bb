// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(simonb): Extend for 64-bit target libraries.

#include "run_length_encoder.h"

#include <string.h>
#include <string>
#include <vector>

#include "debug.h"

namespace relocation_packer {

namespace {

// Generate a vector of deltas between the r_offset fields of adjacent
// R_ARM_RELATIVE relocations.
void GetDeltas(const std::vector<Elf32_Rel>& relocations,
               std::vector<Elf32_Addr>* deltas) {
  CHECK(relocations.size() >= 2);

  for (size_t i = 0; i < relocations.size() - 1; ++i) {
    const Elf32_Addr first = relocations[i].r_offset;
    const Elf32_Addr second = relocations[i + 1].r_offset;
    // Requires that offsets are 'strictly increasing'.  The packing
    // algorithm fails if this does not hold.
    CHECK(second > first);
    deltas->push_back(second - first);
  }
}

// Condense a set of r_offset deltas into a run-length encoded packing.
// Represented as count-delta pairs, where count is the run length and
// delta the common difference between adjacent r_offsets.
void Condense(const std::vector<Elf32_Addr>& deltas,
              std::vector<Elf32_Word>* packed) {
  CHECK(!deltas.empty());
  size_t count = 0;
  Elf32_Addr current = deltas[0];

  // Identify spans of identically valued deltas.
  for (size_t i = 0; i < deltas.size(); ++i) {
    const Elf32_Addr delta = deltas[i];
    if (delta == current) {
      count++;
    } else {
      // We reached the end of a span of identically valued deltas.
      packed->push_back(count);
      packed->push_back(current);
      current = delta;
      count = 1;
    }
  }

  // Write the final span.
  packed->push_back(count);
  packed->push_back(current);
}

// Uncondense a set of r_offset deltas from a run-length encoded packing.
// The initial address for uncondensing, the start index for the first
// condensed slot in packed, and the count of pairs are provided.
void Uncondense(Elf32_Addr addr,
                const std::vector<Elf32_Word>& packed,
                size_t start_index,
                size_t end_index,
                std::vector<Elf32_Rel>* relocations) {
  // The first relocation is just one created from the initial address.
  const Elf32_Rel initial = {addr, R_ARM_RELATIVE};
  relocations->push_back(initial);

  // Read each count and delta pair, beginning at the start index and
  // finishing at the end index.
  for (size_t i = start_index; i < end_index; i += 2) {
    size_t count = packed[i];
    const Elf32_Addr delta = packed[i + 1];
    CHECK(count > 0 && delta > 0);

    // Generate relocations for this count and delta pair.
    while (count) {
      addr += delta;
      const Elf32_Rel relocation = {addr, R_ARM_RELATIVE};
      relocations->push_back(relocation);
      count--;
    }
  }
}

}  // namespace

// Encode R_ARM_RELATIVE relocations into a run-length encoded (packed)
// representation.
void RelocationRunLengthCodec::Encode(const std::vector<Elf32_Rel>& relocations,
                                      std::vector<Elf32_Word>* packed) {
  // If we have zero or one relocation only then there is no packing
  // possible; a run-length encoding needs a run.
  if (relocations.size() < 2)
    return;

  std::vector<Elf32_Addr> deltas;
  GetDeltas(relocations, &deltas);

  // Reserve space for the element count.
  packed->push_back(0);

  // Initialize the packed data with the first offset, then follow up with
  // the condensed deltas vector.
  packed->push_back(relocations[0].r_offset);
  Condense(deltas, packed);

  // Fill in the packed pair count.
  packed->at(0) = (packed->size() - 2) >> 1;
}

// Decode R_ARM_RELATIVE reloctions from a run-length encoded (packed)
// representation.
void RelocationRunLengthCodec::Decode(const std::vector<Elf32_Word>& packed,
                                      std::vector<Elf32_Rel>* relocations) {
  // We need at least one packed pair after the packed pair count to be
  // able to unpack.
  if (packed.size() < 3)
    return;

  // Ensure that the packed data offers enough pairs.  There may be zero
  // padding on it that we ignore.
  CHECK(packed[0] <= (packed.size() - 2) >> 1);

  // The first packed vector element is the pairs count and the second the
  // initial address.  Start uncondensing pairs at the third, and finish
  // at the end of the pairs data.
  const size_t pairs_count = packed[0];
  const Elf32_Addr addr = packed[1];
  Uncondense(addr, packed, 2, 2 + (pairs_count << 1), relocations);
}

}  // namespace relocation_packer
