// Copyright (c) 2012, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "common/linux/elfutils.h"

#include <assert.h>
#include <string.h>

#include "common/linux/linux_libc_support.h"

namespace google_breakpad {

namespace {

template<typename ElfClass>
void FindElfClassSection(const char *elf_base,
                         const char *section_name,
                         uint32_t section_type,
                         const void **section_start,
                         int *section_size) {
  typedef typename ElfClass::Ehdr Ehdr;
  typedef typename ElfClass::Shdr Shdr;

  assert(elf_base);
  assert(section_start);
  assert(section_size);

  assert(my_strncmp(elf_base, ELFMAG, SELFMAG) == 0);

  int name_len = my_strlen(section_name);

  const Ehdr* elf_header = reinterpret_cast<const Ehdr*>(elf_base);
  assert(elf_header->e_ident[EI_CLASS] == ElfClass::kClass);

  const Shdr* sections =
      reinterpret_cast<const Shdr*>(elf_base + elf_header->e_shoff);
  const Shdr* string_section = sections + elf_header->e_shstrndx;

  const Shdr* section = NULL;
  for (int i = 0; i < elf_header->e_shnum; ++i) {
    if (sections[i].sh_type == section_type) {
      const char* current_section_name = (char*)(elf_base +
                                                 string_section->sh_offset +
                                                 sections[i].sh_name);
      if (!my_strncmp(current_section_name, section_name, name_len)) {
        section = &sections[i];
        break;
      }
    }
  }
  if (section != NULL && section->sh_size > 0) {
    *section_start = elf_base + section->sh_offset;
    *section_size = section->sh_size;
  }
}

}  // namespace

bool FindElfSection(const void *elf_mapped_base,
                    const char *section_name,
                    uint32_t section_type,
                    const void **section_start,
                    int *section_size,
                    int *elfclass) {
  assert(elf_mapped_base);
  assert(section_start);
  assert(section_size);

  *section_start = NULL;
  *section_size = 0;

  const char* elf_base =
    static_cast<const char*>(elf_mapped_base);
  const ElfW(Ehdr)* elf_header =
    reinterpret_cast<const ElfW(Ehdr)*>(elf_base);
  if (my_strncmp(elf_base, ELFMAG, SELFMAG) != 0)
    return false;

  if (elfclass) {
    *elfclass = elf_header->e_ident[EI_CLASS];
  }

  if (elf_header->e_ident[EI_CLASS] == ELFCLASS32) {
    FindElfClassSection<ElfClass32>(elf_base, section_name, section_type,
                                    section_start, section_size);
    return *section_start != NULL;
  } else if (elf_header->e_ident[EI_CLASS] == ELFCLASS64) {
    FindElfClassSection<ElfClass64>(elf_base, section_name, section_type,
                                    section_start, section_size);
    return *section_start != NULL;
  }

  return false;
}

}  // namespace google_breakpad
