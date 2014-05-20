/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>

#include "native_client/pnacl/support/relocate.h"
#include "native_client/src/include/elf32.h"
#include "native_client/src/include/elf_constants.h"


#if defined(__native_client_nonsfi__)

/*
 * Using "hidden" visibility ensures these symbols aren't referenced
 * through the GOT, which hasn't been relocated yet.
 */
extern const Elf32_Dyn _DYNAMIC[] __attribute__((visibility("hidden")));
extern char _begin[] __attribute__((visibility("hidden")));

static int find_dt_entry(int dt_tag, uintptr_t *result) {
  const Elf32_Dyn *dyn;
  for (dyn = _DYNAMIC; dyn->d_tag != DT_NULL; dyn++) {
    if (dyn->d_tag == dt_tag) {
      *result = dyn->d_un.d_val;
      return 1;
    }
  }
  return 0;
}

/*
 * It is not straightforward to print a failure message at this point
 * (it would require querying the IRT for write()), so crash instead.
 */
static __attribute__((noinline))
void unhandled_relocation(void) {
  __builtin_trap();
}

void apply_relocations(void) {
  /*
   * Handle the case where the executable is linked as "-static", where
   * there is no PT_DYNAMIC segment and no relocations to apply.  Note that
   * the linker doesn't allow _DYNAMIC to be both weak and undefined when
   * it's also hidden, so we use -defsym in pnacl-nativeld.py to define it
   * instead.  Also note that we can't check "_DYNAMIC == NULL" here: the
   * compiler may assume that is always false (for a non-weak symbol) and
   * optimise the check away.
   *
   * TODO(mseaborn): Remove this once we link non-IRT-using non-SFI
   * executables with "-pie" instead of "-static".
   */
  if (_DYNAMIC == (Elf32_Dyn *) 1)
    return;

  uintptr_t rel_offset;
  uintptr_t rel_size;
  if (!find_dt_entry(DT_REL, &rel_offset) ||
      !find_dt_entry(DT_RELSZ, &rel_size)) {
    /* Nothing to do.  A very simple nexe might have no relocations. */
    return;
  }
  const Elf32_Rel *rels = (const Elf32_Rel *) (_begin + rel_offset);
  const Elf32_Rel *rels_end = (const Elf32_Rel *) (_begin + rel_offset
                                                   + rel_size);
  for (; rels < rels_end; rels++) {
    uint32_t reloc_type = ELF32_R_TYPE(rels->r_info);
    void *addr = _begin + rels->r_offset;
    switch (reloc_type) {
#if defined(__i386__)
      case R_386_RELATIVE:
#elif defined(__arm__)
      case R_ARM_RELATIVE:
#else
# error Unhandled architecture
#endif
        *(uint32_t *) addr += (uint32_t) _begin;
        break;
      default:
        unhandled_relocation();
    }
  }
}

#endif
