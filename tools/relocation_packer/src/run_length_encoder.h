// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Run-length encode and decode ARM relative relocations.
//
// ARM relative relocations are the bulk of dynamic relocations (the
// .rel.dyn section) in libchrome<version>.so, and the ELF standard
// representation of them is wasteful.
//
// A relocation is 8 bytes (16 bytes on 64 bit plaforms), split into offset
// and info fields.  Offsets strictly increase, and each is commonly a
// few bytes different from its predecessor.  There are long runs where
// the difference does not change.  The info field is always 0x17.  Example,
// from 'readelf -x4 libchrome.<version>.so':
//
//   offset   info     offset   info
//   808fef01 17000000 848fef01 17000000 ................
//   888fef01 17000000 8c8fef01 17000000 ................
//   908fef01 17000000 948fef01 17000000 ................
//
// Run length encoding packs this data more efficiently, by representing it
// as a delta and a count of entries each differing from its predecessor
// by this delta.  The above can be represented as a start address followed
// by an encoded count of 6 and offset difference of 4:
//
//   start    count    diff
//   808fef01 00000006 00000004
//
// Because ARM relative relocation offsets strictly increase, the complete
// set of ARM relative relocations in libchrome.<version>.so can be
// represented by a single start address followed by one or more difference
// and count encoded word pairs:
//
//   start    run1 count run1 diff  run2 count run2 diff
//   808fef01 00000006   00000004   00000010   00000008 ...
//
// Decoding regenerates ARM relative relocations beginning at address
// 'start' and for each encoded run, incrementing the address by 'difference'
// for 'count' iterations and emitting a new ARM relative relocation.
//
// Once encoded, data is prefixed by a single word count of packed delta and
// count pairs.  A final run-length encoded ARM relative relocations vector
// might therefore look something like:
//
//   pairs    start    run 1             run 2             ... run 15
//   0000000f 808fef01 00000006 00000004 00000010 00000008 ...
// Interpreted as:
//   pairs=15 start=.. count=6,delta=4   count=16,delta=8

#ifndef TOOLS_RELOCATION_PACKER_SRC_RUN_LENGTH_ENCODER_H_
#define TOOLS_RELOCATION_PACKER_SRC_RUN_LENGTH_ENCODER_H_

#include <vector>

#include "elf.h"
#include "elf_traits.h"

namespace relocation_packer {

// A RelocationRunLengthCodec packs vectors of ARM relative relocations
// into more compact forms, and unpacks them to reproduce the pre-packed data.
class RelocationRunLengthCodec {
 public:
  // Encode ARM relative relocations into a more compact form.
  // |relocations| is a vector of ARM relative relocation structs.
  // |packed| is the vector of packed words into which relocations are packed.
  static void Encode(const std::vector<ELF::Rel>& relocations,
                     std::vector<ELF::Xword>* packed);

  // Decode ARM relative relocations from their more compact form.
  // |packed| is the vector of packed relocations.
  // |relocations| is a vector of unpacked ARM relative relocation structs.
  static void Decode(const std::vector<ELF::Xword>& packed,
                     std::vector<ELF::Rel>* relocations);
};

}  // namespace relocation_packer

#endif  // TOOLS_RELOCATION_PACKER_SRC_RUN_LENGTH_ENCODER_H_
