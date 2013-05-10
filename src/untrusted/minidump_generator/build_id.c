/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <string.h>

#include "native_client/src/include/elf32.h"


/*
 * The next-generation (upstream) linkers define this symbol when the
 * ELF file and program headers are visible in the address space.  We
 * use a weak reference to be compatible with the old nacl-binutils
 * linkers and layout, where this symbol is not defined and the
 * headers are not visible in the address space.
 */
extern const Elf32_Ehdr __ehdr_start __attribute__((weak));

static uintptr_t note_align(uintptr_t value) {
  return (value + 3) & ~3;
}

int nacl_get_build_id(const char **data, size_t *size) {
  if (&__ehdr_start != NULL &&
      __ehdr_start.e_ident[EI_CLASS] == ELFCLASS32 &&
      __ehdr_start.e_phentsize == sizeof(Elf32_Phdr)) {
    int phnum = __ehdr_start.e_phnum;
    uintptr_t start_addr = (uintptr_t) &__ehdr_start;
    Elf32_Phdr *phdrs = (Elf32_Phdr *) (start_addr + __ehdr_start.e_phoff);
    for (int i = 0; i < phnum; ++i) {
      Elf32_Phdr *phdr = &phdrs[i];
      if (phdr->p_type == PT_NOTE) {
        Elf32_Nhdr *note = (Elf32_Nhdr *) (start_addr + phdr->p_offset);
        uintptr_t note_end = (uintptr_t) note + phdr->p_memsz;
        while ((uintptr_t) note < note_end) {
          uintptr_t name_ptr = (uintptr_t) &note[1];
          assert(name_ptr <= note_end);
          uintptr_t desc_ptr = note_align(name_ptr + note->n_namesz);
          uintptr_t next_ptr = note_align(desc_ptr + note->n_descsz);
          assert(next_ptr <= note_end);
          if (note->n_type == NT_GNU_BUILD_ID &&
              note->n_namesz == sizeof(ELF_NOTE_GNU) &&
              memcmp((const char *) name_ptr, ELF_NOTE_GNU,
                     sizeof(ELF_NOTE_GNU)) == 0) {
            *data = (const char *) desc_ptr;
            *size = note->n_descsz;
            return 1;
          }
          note = (Elf32_Nhdr *) next_ptr;
        }
      }
    }
  }
  return 0;
}
