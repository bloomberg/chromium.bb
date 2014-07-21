// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ELF shared object file updates handler.
//
// Provides functions to remove ARM relative relocations from the .rel.dyn
// section and pack them in .android.rel.dyn, and unpack to return the file
// to its pre-packed state.
//
// Files to be packed or unpacked must include an existing .android.rel.dyn
// section.  A standard libchrome.<version>.so will not contain this section,
// so the following can be used to add one:
//
//   echo -n 'NULL' >/tmp/small
//   arm-linux-gnueabi-objcopy
//       --add-section .android.rel.dyn=/tmp/small
//       libchrome.<version>.so libchrome.<version>.so.packed
//   rm /tmp/small
//
// To use, open the file and pass the file descriptor to the constructor,
// then pack or unpack as desired.  Packing or unpacking will flush the file
// descriptor on success.  Example:
//
//   int fd = open(..., O_RDWR);
//   ElfFile elf_file(fd);
//   bool status;
//   if (is_packing)
//     status = elf_file.PackRelocations();
//   else
//     status = elf_file.UnpackRelocations();
//   close(fd);
//
// SetPadding() causes PackRelocations() to pad .rel.dyn with NONE-type
// entries rather than cutting a hole out of the shared object file.  This
// keeps all load addresses and offsets constant, and enables easier
// debugging and testing.
//
// A packed shared object file has all of its ARM relative relocations
// removed from .rel.dyn, and replaced as packed data in .android.rel.dyn.
// The resulting file is shorter than its non-packed original.
//
// Unpacking a packed file restores the file to its non-packed state, by
// expanding the packed data in android.rel.dyn, combining the ARM relative
// relocations with the data already in .rel.dyn, and then writing back the
// now expanded .rel.dyn section.

#ifndef TOOLS_RELOCATION_PACKER_SRC_ELF_FILE_H_
#define TOOLS_RELOCATION_PACKER_SRC_ELF_FILE_H_

#include <string.h>

#include "elf.h"
#include "libelf.h"
#include "packer.h"

namespace relocation_packer {

// An ElfFile reads shared objects, and shuttles ARM relative relocations
// between .rel.dyn and .android.rel.dyn sections.
class ElfFile {
 public:
  explicit ElfFile(int fd) { memset(this, 0, sizeof(*this)); fd_ = fd; }
  ~ElfFile() {}

  // Set padding mode.  When padding, PackRelocations() will not shrink
  // the .rel.dyn section, but instead replace ARM relative with
  // NONE-type entries.
  // |flag| is true to pad .rel.dyn, false to shrink it.
  inline void SetPadding(bool flag) { is_padding_rel_dyn_ = flag; }

  // Transfer ARM relative relocations from .rel.dyn to a packed
  // representation in .android.rel.dyn.  Returns true on success.
  bool PackRelocations();

  // Transfer ARM relative relocations from a packed representation in
  // .android.rel.dyn to .rel.dyn.  Returns true on success.
  bool UnpackRelocations();

 private:
  // Load a new ElfFile from a filedescriptor.  If flushing, the file must
  // be open for read/write.  Returns true on successful ELF file load.
  // |fd| is an open file descriptor for the shared object.
  bool Load();

  // Write ELF file changes.
  void Flush();

  // If set, pad rather than shrink .rel.dyn.  Primarily for debugging,
  // allows packing to be checked without affecting load addresses.
  bool is_padding_rel_dyn_;

  // File descriptor opened on the shared object.
  int fd_;

  // Libelf handle, assigned by Load().
  Elf* elf_;

  // Sections that we manipulate, assigned by Load().
  Elf_Scn* rel_dyn_section_;
  Elf_Scn* dynamic_section_;
  Elf_Scn* android_rel_dyn_section_;
};

}  // namespace relocation_packer

#endif  // TOOLS_RELOCATION_PACKER_SRC_ELF_FILE_H_
