// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_elf_relocations.h"

#include <assert.h>
#include <errno.h>

#include "crazy_linker_debug.h"
#include "crazy_linker_elf_symbols.h"
#include "crazy_linker_elf_view.h"
#include "crazy_linker_error.h"
#include "crazy_linker_leb128.h"
#include "crazy_linker_system.h"
#include "crazy_linker_util.h"
#include "linker_phdr.h"

#define DEBUG_RELOCATIONS 0

#define RLOG(...) LOG_IF(DEBUG_RELOCATIONS, __VA_ARGS__)
#define RLOG_ERRNO(...) LOG_ERRNO_IF(DEBUG_RELOCATIONS, __VA_ARGS__)

#ifndef DF_SYMBOLIC
#define DF_SYMBOLIC 2
#endif

#ifndef DF_TEXTREL
#define DF_TEXTREL 4
#endif

#ifndef DT_FLAGS
#define DT_FLAGS 30
#endif

// Extension dynamic tags for Android packed relocations.
#ifndef DT_LOOS
#define DT_LOOS 0x6000000d
#endif
#ifndef DT_ANDROID_REL
#define DT_ANDROID_REL (DT_LOOS + 2)
#endif
#ifndef DT_ANDROID_RELSZ
#define DT_ANDROID_RELSZ (DT_LOOS + 3)
#endif
#ifndef DT_ANDROID_RELA
#define DT_ANDROID_RELA (DT_LOOS + 4)
#endif
#ifndef DT_ANDROID_RELASZ
#define DT_ANDROID_RELASZ (DT_LOOS + 5)
#endif

// Processor-specific relocation types supported by the linker.
#ifdef __arm__

/* arm32 relocations */
#define R_ARM_ABS32 2
#define R_ARM_REL32 3
#define R_ARM_GLOB_DAT 21
#define R_ARM_JUMP_SLOT 22
#define R_ARM_COPY 20
#define R_ARM_RELATIVE 23

#define RELATIVE_RELOCATION_CODE R_ARM_RELATIVE

#endif  // __arm__

#ifdef __aarch64__

/* arm64 relocations */
#define R_AARCH64_ABS64 257
#define R_AARCH64_COPY 1024
#define R_AARCH64_GLOB_DAT 1025
#define R_AARCH64_JUMP_SLOT 1026
#define R_AARCH64_RELATIVE 1027

#define RELATIVE_RELOCATION_CODE R_AARCH64_RELATIVE

#endif  // __aarch64__

#ifdef __i386__

/* i386 relocations */
#define R_386_32 1
#define R_386_PC32 2
#define R_386_GLOB_DAT 6
#define R_386_JMP_SLOT 7
#define R_386_RELATIVE 8

#endif  // __i386__

#ifdef __x86_64__

/* x86_64 relocations */
#define R_X86_64_64 1
#define R_X86_64_PC32 2
#define R_X86_64_GLOB_DAT 6
#define R_X86_64_JMP_SLOT 7
#define R_X86_64_RELATIVE 8

#endif  // __x86_64__

namespace crazy {

namespace {

// List of known relocation types the relocator knows about.
enum RelocationType {
  RELOCATION_TYPE_UNKNOWN = 0,
  RELOCATION_TYPE_ABSOLUTE = 1,
  RELOCATION_TYPE_RELATIVE = 2,
  RELOCATION_TYPE_PC_RELATIVE = 3,
  RELOCATION_TYPE_COPY = 4,
};

// Convert an ELF relocation type info a RelocationType value.
RelocationType GetRelocationType(ELF::Word r_type) {
  switch (r_type) {
#ifdef __arm__
    case R_ARM_JUMP_SLOT:
    case R_ARM_GLOB_DAT:
    case R_ARM_ABS32:
      return RELOCATION_TYPE_ABSOLUTE;

    case R_ARM_REL32:
    case R_ARM_RELATIVE:
      return RELOCATION_TYPE_RELATIVE;

    case R_ARM_COPY:
      return RELOCATION_TYPE_COPY;
#endif

#ifdef __aarch64__
    case R_AARCH64_JUMP_SLOT:
    case R_AARCH64_GLOB_DAT:
    case R_AARCH64_ABS64:
      return RELOCATION_TYPE_ABSOLUTE;

    case R_AARCH64_RELATIVE:
      return RELOCATION_TYPE_RELATIVE;

    case R_AARCH64_COPY:
      return RELOCATION_TYPE_COPY;
#endif

#ifdef __i386__
    case R_386_JMP_SLOT:
    case R_386_GLOB_DAT:
    case R_386_32:
      return RELOCATION_TYPE_ABSOLUTE;

    case R_386_RELATIVE:
      return RELOCATION_TYPE_RELATIVE;

    case R_386_PC32:
      return RELOCATION_TYPE_PC_RELATIVE;
#endif

#ifdef __x86_64__
    case R_X86_64_JMP_SLOT:
    case R_X86_64_GLOB_DAT:
    case R_X86_64_64:
      return RELOCATION_TYPE_ABSOLUTE;

    case R_X86_64_RELATIVE:
      return RELOCATION_TYPE_RELATIVE;

    case R_X86_64_PC32:
      return RELOCATION_TYPE_PC_RELATIVE;
#endif

#ifdef __mips__
    case R_MIPS_REL32:
      return RELOCATION_TYPE_RELATIVE;
#endif

    default:
      return RELOCATION_TYPE_UNKNOWN;
  }
}

}  // namespace

bool ElfRelocations::Init(const ElfView* view, Error* error) {
  // Save these for later.
  phdr_ = view->phdr();
  phdr_count_ = view->phdr_count();
  load_bias_ = view->load_bias();

  // We handle only Rel or Rela, but not both. If DT_RELA or DT_RELASZ
  // then we require DT_PLTREL to agree.
  bool has_rela_relocations = false;
  bool has_rel_relocations = false;

  // Parse the dynamic table.
  ElfView::DynamicIterator dyn(view);
  for (; dyn.HasNext(); dyn.GetNext()) {
    ELF::Addr dyn_value = dyn.GetValue();
    uintptr_t dyn_addr = dyn.GetAddress(view->load_bias());

    const ELF::Addr tag = dyn.GetTag();
    switch (tag) {
      case DT_PLTREL:
        RLOG("  DT_PLTREL value=%d\n", dyn_value);
        if (dyn_value != DT_REL && dyn_value != DT_RELA) {
          *error = "Invalid DT_PLTREL value in dynamic section";
          return false;
        }
        relocations_type_ = dyn_value;
        break;
      case DT_JMPREL:
        RLOG("  DT_JMPREL addr=%p\n", dyn_addr);
        plt_relocations_ = dyn_addr;
        break;
      case DT_PLTRELSZ:
        plt_relocations_size_ = dyn_value;
        RLOG("  DT_PLTRELSZ size=%d\n", dyn_value);
        break;
      case DT_RELA:
      case DT_REL:
        RLOG("  %s addr=%p\n",
             (tag == DT_RELA) ? "DT_RELA" : "DT_REL",
             dyn_addr);
        if (relocations_) {
          *error = "Unsupported DT_RELA/DT_REL combination in dynamic section";
          return false;
        }
        relocations_ = dyn_addr;
        if (tag == DT_RELA)
          has_rela_relocations = true;
        else
          has_rel_relocations = true;
        break;
      case DT_RELASZ:
      case DT_RELSZ:
        RLOG("  %s size=%d\n",
             (tag == DT_RELASZ) ? "DT_RELASZ" : "DT_RELSZ",
             dyn_addr);
        if (relocations_size_) {
          *error = "Unsupported DT_RELASZ/DT_RELSZ combination in dyn section";
          return false;
        }
        relocations_size_ = dyn_value;
        if (tag == DT_RELASZ)
          has_rela_relocations = true;
        else
          has_rel_relocations = true;
        break;
      case DT_ANDROID_RELA:
      case DT_ANDROID_REL:
        RLOG("  %s addr=%p\n",
             (tag == DT_ANDROID_RELA) ? "DT_ANDROID_RELA" : "DT_ANDROID_REL",
             dyn_addr);
        if (android_relocations_) {
          *error = "Unsupported DT_ANDROID_RELA/DT_ANDROID_REL "
                   "combination in dynamic section";
          return false;
        }
        android_relocations_ = reinterpret_cast<uint8_t*>(dyn_addr);
        if (tag == DT_ANDROID_RELA)
          has_rela_relocations = true;
        else
          has_rel_relocations = true;
        break;
      case DT_ANDROID_RELASZ:
      case DT_ANDROID_RELSZ:
        RLOG("  %s size=%d\n",
             (tag == DT_ANDROID_RELASZ)
                 ? "DT_ANDROID_RELASZ" : "DT_ANDROID_RELSZ", dyn_addr);
        if (android_relocations_size_) {
          *error = "Unsupported DT_ANDROID_RELASZ/DT_ANDROID_RELSZ "
                   "combination in dyn section";
          return false;
        }
        android_relocations_size_ = dyn_value;
        if (tag == DT_ANDROID_RELASZ)
          has_rela_relocations = true;
        else
          has_rel_relocations = true;
        break;
      case DT_PLTGOT:
        // Only used on MIPS currently. Could also be used on other platforms
        // when lazy binding (i.e. RTLD_LAZY) is implemented.
        RLOG("  DT_PLTGOT addr=%p\n", dyn_addr);
        plt_got_ = reinterpret_cast<ELF::Addr*>(dyn_addr);
        break;
      case DT_TEXTREL:
        RLOG("  DT_TEXTREL\n");
        has_text_relocations_ = true;
        break;
      case DT_SYMBOLIC:
        RLOG("  DT_SYMBOLIC\n");
        has_symbolic_ = true;
        break;
      case DT_FLAGS:
        if (dyn_value & DF_TEXTREL)
          has_text_relocations_ = true;
        if (dyn_value & DF_SYMBOLIC)
          has_symbolic_ = true;
        RLOG("  DT_FLAGS has_text_relocations=%s has_symbolic=%s\n",
             has_text_relocations_ ? "true" : "false",
             has_symbolic_ ? "true" : "false");
        break;
#if defined(__mips__)
      case DT_MIPS_SYMTABNO:
        RLOG("  DT_MIPS_SYMTABNO value=%d\n", dyn_value);
        mips_symtab_count_ = dyn_value;
        break;

      case DT_MIPS_LOCAL_GOTNO:
        RLOG("  DT_MIPS_LOCAL_GOTNO value=%d\n", dyn_value);
        mips_local_got_count_ = dyn_value;
        break;

      case DT_MIPS_GOTSYM:
        RLOG("  DT_MIPS_GOTSYM value=%d\n", dyn_value);
        mips_gotsym_ = dyn_value;
        break;
#endif
      default:
        ;
    }
  }

  if (has_rel_relocations && has_rela_relocations) {
    *error = "Combining relocations with and without addends is not "
             "currently supported";
    return false;
  }

  // If DT_PLTREL did not explicitly assign relocations_type_, set it
  // here based on the type of relocations found.
  if (relocations_type_ != DT_REL && relocations_type_ != DT_RELA) {
    if (has_rel_relocations)
      relocations_type_ = DT_REL;
    else if (has_rela_relocations)
      relocations_type_ = DT_RELA;
  }

  if (relocations_type_ == DT_REL && has_rela_relocations) {
    *error = "Found relocations with addends in dyn section, "
             "but DT_PLTREL is DT_REL";
    return false;
  }
  if (relocations_type_ == DT_RELA && has_rel_relocations) {
    *error = "Found relocations without addends in dyn section, "
             "but DT_PLTREL is DT_RELA";
    return false;
  }

  return true;
}

bool ElfRelocations::ApplyAll(const ElfSymbols* symbols,
                              SymbolResolver* resolver,
                              Error* error) {
  LOG("%s: Enter\n", __FUNCTION__);

  if (has_text_relocations_) {
    if (phdr_table_unprotect_segments(phdr_, phdr_count_, load_bias_) < 0) {
      error->Format("Can't unprotect loadable segments: %s", strerror(errno));
      return false;
    }
  }

  if (!ApplyAndroidRelocations(symbols, resolver, error))
    return false;

  if (relocations_type_ == DT_REL) {
    if (!ApplyRelRelocs(reinterpret_cast<ELF::Rel*>(relocations_),
                        relocations_size_ / sizeof(ELF::Rel),
                        symbols,
                        resolver,
                        error))
      return false;
    if (!ApplyRelRelocs(reinterpret_cast<ELF::Rel*>(plt_relocations_),
                        plt_relocations_size_ / sizeof(ELF::Rel),
                        symbols,
                        resolver,
                        error))
      return false;
  }

  if (relocations_type_ == DT_RELA) {
    if (!ApplyRelaRelocs(reinterpret_cast<ELF::Rela*>(relocations_),
                         relocations_size_ / sizeof(ELF::Rela),
                         symbols,
                         resolver,
                         error))
      return false;
    if (!ApplyRelaRelocs(reinterpret_cast<ELF::Rela*>(plt_relocations_),
                         plt_relocations_size_ / sizeof(ELF::Rela),
                         symbols,
                         resolver,
                         error))
      return false;
  }

#ifdef __mips__
  if (!RelocateMipsGot(symbols, resolver, error))
    return false;
#endif

  if (has_text_relocations_) {
    if (phdr_table_protect_segments(phdr_, phdr_count_, load_bias_) < 0) {
      error->Format("Can't reprotect loadable segments: %s", strerror(errno));
      return false;
    }
  }

  LOG("%s: Done\n", __FUNCTION__);
  return true;
}

// Helper class for Android packed relocations.  Encapsulates the packing
// flags used by Android for packed relocation groups.
class AndroidPackedRelocationGroupFlags {
 public:
  explicit AndroidPackedRelocationGroupFlags(size_t flags) : flags_(flags) { }

  bool is_relocation_grouped_by_info() const {
    return hasFlag(kRelocationGroupedByInfoFlag);
  }
  bool is_relocation_grouped_by_offset_delta() const {
    return hasFlag(kRelocationGroupedByOffsetDeltaFlag);
  }
  bool is_relocation_grouped_by_addend() const {
    return hasFlag(kRelocationGroupedByAddendFlag);
  }
  bool is_relocation_group_has_addend() const {
    return hasFlag(kRelocationGroupHasAddendFlag);
  }

 private:
  bool hasFlag(size_t flag) const { return (flags_ & flag) != 0; }

  static const size_t kRelocationGroupedByInfoFlag = 1 << 0;
  static const size_t kRelocationGroupedByOffsetDeltaFlag = 1 << 1;
  static const size_t kRelocationGroupedByAddendFlag = 1 << 2;
  static const size_t kRelocationGroupHasAddendFlag = 1 << 3;

  const size_t flags_;
};

bool ElfRelocations::ForEachAndroidRelocation(RelocationHandler handler,
                                              void* opaque) {
  // Skip over the "APS2" signature.
  Sleb128Decoder decoder(android_relocations_ + 4,
                         android_relocations_size_ - 4);

  // Unpacking into a relocation with addend, both for REL and RELA, is
  // convenient at this point. If REL, the handler needs to take care of
  // any conversion before use.
  ELF::Rela relocation;
  memset(&relocation, 0, sizeof(relocation));

  // Read the relocation count and initial offset.
  const size_t relocation_count = decoder.pop_front();
  relocation.r_offset = decoder.pop_front();

  LOG("%s: relocation_count=%d, initial r_offset=%p\n",
      __FUNCTION__,
      relocation_count,
      relocation.r_offset);

  size_t relocations_handled = 0;
  while (relocations_handled < relocation_count) {
    // Read the start of the group header to obtain its size and flags.
    const size_t group_size = decoder.pop_front();
    AndroidPackedRelocationGroupFlags group_flags(decoder.pop_front());

    // Read other group header fields, depending on the flags read above.
    size_t group_r_offset_delta = 0;
    if (group_flags.is_relocation_grouped_by_offset_delta())
      group_r_offset_delta = decoder.pop_front();

    if (group_flags.is_relocation_grouped_by_info())
      relocation.r_info = decoder.pop_front();

    if (group_flags.is_relocation_group_has_addend() &&
        group_flags.is_relocation_grouped_by_addend())
      relocation.r_addend += decoder.pop_front();
    else if (!group_flags.is_relocation_group_has_addend())
      relocation.r_addend = 0;

    // Expand the group into individual relocations.
    for (size_t group_index = 0; group_index < group_size; group_index++) {
      if (group_flags.is_relocation_grouped_by_offset_delta())
        relocation.r_offset += group_r_offset_delta;
      else
        relocation.r_offset += decoder.pop_front();

      if (!group_flags.is_relocation_grouped_by_info())
        relocation.r_info = decoder.pop_front();

      if (group_flags.is_relocation_group_has_addend() &&
          !group_flags.is_relocation_grouped_by_addend())
        relocation.r_addend += decoder.pop_front();

      // Pass the relocation to the supplied handler function. If the handler
      // returns false we view this as failure and return false to our caller.
      if (!handler(this, &relocation, opaque)) {
        LOG("%s: failed handling relocation %d\n",
            __FUNCTION__,
            relocations_handled);
        return false;
      }

      relocations_handled++;
    }
  }

  LOG("%s: relocations_handled=%d\n", __FUNCTION__, relocations_handled);
  return true;
}

namespace {

// Validate the Android packed relocations signature.
bool IsValidAndroidPackedRelocations(const uint8_t* android_relocations,
                                     size_t android_relocations_size) {
  if (android_relocations_size < 4)
    return false;

  // Check for an initial APS2 Android packed relocations header.
  return (android_relocations[0] == 'A' &&
          android_relocations[1] == 'P' &&
          android_relocations[2] == 'S' &&
          android_relocations[3] == '2');
}

// Narrow a Rela to its equivalent Rel. The r_addend field in the input
// Rela must be zero.
void ConvertRelaToRel(const ELF::Rela* rela, ELF::Rel* rel) {
  assert(rela->r_addend == 0);
  rel->r_offset = rela->r_offset;
  rel->r_info = rela->r_info;
}

}  // namespace

// Args for ApplyAndroidRelocation handler function.
struct ApplyAndroidRelocationArgs {
  ELF::Addr relocations_type;
  const ElfSymbols* symbols;
  ElfRelocations::SymbolResolver* resolver;
  Error* error;
};

// Static ForEachAndroidRelocation() handler.
bool ElfRelocations::ApplyAndroidRelocation(ElfRelocations* relocations,
                                            const ELF::Rela* relocation,
                                            void* opaque) {
  // Unpack args from opaque.
  ApplyAndroidRelocationArgs* args =
      reinterpret_cast<ApplyAndroidRelocationArgs*>(opaque);
  const ELF::Addr relocations_type = args->relocations_type;
  const ElfSymbols* symbols = args->symbols;
  ElfRelocations::SymbolResolver* resolver = args->resolver;
  Error* error = args->error;

  // For REL relocations, convert from RELA to REL and apply the conversion.
  // For RELA relocations, apply RELA directly.
  if (relocations_type == DT_REL) {
    ELF::Rel converted;
    ConvertRelaToRel(relocation, &converted);
    return relocations->ApplyRelReloc(&converted, symbols, resolver, error);
  }

  if (relocations_type == DT_RELA)
    return relocations->ApplyRelaReloc(relocation, symbols, resolver, error);

  return true;
}

bool ElfRelocations::ApplyAndroidRelocations(const ElfSymbols* symbols,
                                             SymbolResolver* resolver,
                                             Error* error) {
  if (!android_relocations_)
    return true;

  if (!IsValidAndroidPackedRelocations(android_relocations_,
                                       android_relocations_size_))
    return false;

  ApplyAndroidRelocationArgs args;
  args.relocations_type = relocations_type_;
  args.symbols = symbols;
  args.resolver = resolver;
  args.error = error;
  return ForEachAndroidRelocation(&ApplyAndroidRelocation, &args);
}

bool ElfRelocations::ApplyResolvedRelaReloc(const ELF::Rela* rela,
                                            ELF::Addr sym_addr,
                                            bool resolved CRAZY_UNUSED,
                                            Error* error) {
  const ELF::Word rela_type = ELF_R_TYPE(rela->r_info);
  const ELF::Word CRAZY_UNUSED rela_symbol = ELF_R_SYM(rela->r_info);
  const ELF::Sword CRAZY_UNUSED addend = rela->r_addend;

  const ELF::Addr reloc = static_cast<ELF::Addr>(rela->r_offset + load_bias_);

  RLOG("  rela reloc=%p offset=%p type=%d addend=%p\n",
       reloc,
       rela->r_offset,
       rela_type,
       addend);

  // Apply the relocation.
  ELF::Addr* CRAZY_UNUSED target = reinterpret_cast<ELF::Addr*>(reloc);
  switch (rela_type) {
#ifdef __aarch64__
    case R_AARCH64_JUMP_SLOT:
      RLOG("  R_AARCH64_JUMP_SLOT target=%p addr=%p\n",
           target,
           sym_addr + addend);
      *target = sym_addr + addend;
      break;

    case R_AARCH64_GLOB_DAT:
      RLOG("  R_AARCH64_GLOB_DAT target=%p addr=%p\n",
           target,
           sym_addr + addend);
      *target = sym_addr + addend;
      break;

    case R_AARCH64_ABS64:
      RLOG("  R_AARCH64_ABS64 target=%p (%p) addr=%p\n",
           target,
           *target,
           sym_addr + addend);
      *target += sym_addr + addend;
      break;

    case R_AARCH64_RELATIVE:
      RLOG("  R_AARCH64_RELATIVE target=%p (%p) bias=%p\n",
           target,
           *target,
           load_bias_ + addend);
      if (__builtin_expect(rela_symbol, 0)) {
        *error = "Invalid relative relocation with symbol";
        return false;
      }
      *target = load_bias_ + addend;
      break;

    case R_AARCH64_COPY:
      // NOTE: These relocations are forbidden in shared libraries.
      RLOG("  R_AARCH64_COPY\n");
      *error = "Invalid R_AARCH64_COPY relocation in shared library";
      return false;
#endif  // __aarch64__

#ifdef __x86_64__
    case R_X86_64_JMP_SLOT:
      *target = sym_addr + addend;
      break;

    case R_X86_64_GLOB_DAT:
      *target = sym_addr + addend;
      break;

    case R_X86_64_RELATIVE:
      if (rela_symbol) {
        *error = "Invalid relative relocation with symbol";
        return false;
      }
      *target = load_bias_ + addend;
      break;

    case R_X86_64_64:
      *target = sym_addr + addend;
      break;

    case R_X86_64_PC32:
      *target = sym_addr + (addend - reloc);
      break;
#endif  // __x86_64__

    default:
      error->Format("Invalid relocation type (%d)", rela_type);
      return false;
  }

  return true;
}

bool ElfRelocations::ApplyResolvedRelReloc(const ELF::Rel* rel,
                                           ELF::Addr sym_addr,
                                           bool resolved CRAZY_UNUSED,
                                           Error* error) {
  const ELF::Word rel_type = ELF_R_TYPE(rel->r_info);
  const ELF::Word CRAZY_UNUSED rel_symbol = ELF_R_SYM(rel->r_info);

  const ELF::Addr reloc = static_cast<ELF::Addr>(rel->r_offset + load_bias_);

  RLOG("  rel reloc=%p offset=%p type=%d\n", reloc, rel->r_offset, rel_type);

  // Apply the relocation.
  ELF::Addr* CRAZY_UNUSED target = reinterpret_cast<ELF::Addr*>(reloc);
  switch (rel_type) {
#ifdef __arm__
    case R_ARM_JUMP_SLOT:
      RLOG("  R_ARM_JUMP_SLOT target=%p addr=%p\n", target, sym_addr);
      *target = sym_addr;
      break;

    case R_ARM_GLOB_DAT:
      RLOG("  R_ARM_GLOB_DAT target=%p addr=%p\n", target, sym_addr);
      *target = sym_addr;
      break;

    case R_ARM_ABS32:
      RLOG("  R_ARM_ABS32 target=%p (%p) addr=%p\n",
           target,
           *target,
           sym_addr);
      *target += sym_addr;
      break;

    case R_ARM_REL32:
      RLOG("  R_ARM_REL32 target=%p (%p) addr=%p offset=%p\n",
           target,
           *target,
           sym_addr,
           rel->r_offset);
      *target += sym_addr - rel->r_offset;
      break;

    case R_ARM_RELATIVE:
      RLOG("  R_ARM_RELATIVE target=%p (%p) bias=%p\n",
           target,
           *target,
           load_bias_);
      if (__builtin_expect(rel_symbol, 0)) {
        *error = "Invalid relative relocation with symbol";
        return false;
      }
      *target += load_bias_;
      break;

    case R_ARM_COPY:
      // NOTE: These relocations are forbidden in shared libraries.
      // The Android linker has special code to deal with this, which
      // is not needed here.
      RLOG("  R_ARM_COPY\n");
      *error = "Invalid R_ARM_COPY relocation in shared library";
      return false;
#endif  // __arm__

#ifdef __i386__
    case R_386_JMP_SLOT:
      *target = sym_addr;
      break;

    case R_386_GLOB_DAT:
      *target = sym_addr;
      break;

    case R_386_RELATIVE:
      if (rel_symbol) {
        *error = "Invalid relative relocation with symbol";
        return false;
      }
      *target += load_bias_;
      break;

    case R_386_32:
      *target += sym_addr;
      break;

    case R_386_PC32:
      *target += (sym_addr - reloc);
      break;
#endif  // __i386__

#ifdef __mips__
    case R_MIPS_REL32:
      if (resolved)
        *target += sym_addr;
      else
        *target += load_bias_;
      break;
#endif  // __mips__

    default:
      error->Format("Invalid relocation type (%d)", rel_type);
      return false;
  }

  return true;
}

bool ElfRelocations::ResolveSymbol(ELF::Word rel_type,
                                   ELF::Word rel_symbol,
                                   const ElfSymbols* symbols,
                                   SymbolResolver* resolver,
                                   ELF::Addr reloc,
                                   ELF::Addr* sym_addr,
                                   Error* error) {
  const char* sym_name = symbols->LookupNameById(rel_symbol);
  RLOG("    symbol name='%s'\n", sym_name);
  void* address = resolver->Lookup(sym_name);

  if (address) {
    // The symbol was found, so compute its address.
    RLOG("%s: symbol %s resolved to %p\n", __FUNCTION__, sym_name, address);
    *sym_addr = reinterpret_cast<ELF::Addr>(address);
    return true;
  }

  // The symbol was not found. Normally this is an error except
  // if this is a weak reference.
  if (!symbols->IsWeakById(rel_symbol)) {
    error->Format("Could not find symbol '%s'", sym_name);
    return false;
  }

  RLOG("%s: weak reference to unresolved symbol %s\n", __FUNCTION__, sym_name);

  // IHI0044C AAELF 4.5.1.1:
  // Libraries are not searched to resolve weak references.
  // It is not an error for a weak reference to remain
  // unsatisfied.
  //
  // During linking, the value of an undefined weak reference is:
  // - Zero if the relocation type is absolute
  // - The address of the place if the relocation is pc-relative
  // - The address of nominal base address if the relocation
  //   type is base-relative.
  RelocationType r = GetRelocationType(rel_type);
  if (r == RELOCATION_TYPE_ABSOLUTE || r == RELOCATION_TYPE_RELATIVE) {
    *sym_addr = 0;
    return true;
  }

  if (r == RELOCATION_TYPE_PC_RELATIVE) {
    *sym_addr = reloc;
    return true;
  }

  error->Format(
      "Invalid weak relocation type (%d) for unknown symbol '%s'",
      r,
      sym_name);
  return false;
}

bool ElfRelocations::ApplyRelReloc(const ELF::Rel* rel,
                                   const ElfSymbols* symbols,
                                   SymbolResolver* resolver,
                                   Error* error) {
  const ELF::Word rel_type = ELF_R_TYPE(rel->r_info);
  const ELF::Word rel_symbol = ELF_R_SYM(rel->r_info);

  ELF::Addr sym_addr = 0;
  ELF::Addr reloc = static_cast<ELF::Addr>(rel->r_offset + load_bias_);
  RLOG("  reloc=%p offset=%p type=%d symbol=%d\n",
       reloc,
       rel->r_offset,
       rel_type,
       rel_symbol);

  if (rel_type == 0)
    return true;

  bool resolved = false;

  // If this is a symbolic relocation, compute the symbol's address.
  if (__builtin_expect(rel_symbol != 0, 0)) {
    if (!ResolveSymbol(rel_type,
                       rel_symbol,
                       symbols,
                       resolver,
                       reloc,
                       &sym_addr,
                       error)) {
      return false;
    }
    resolved = true;
  }

  return ApplyResolvedRelReloc(rel, sym_addr, resolved, error);
}

bool ElfRelocations::ApplyRelRelocs(const ELF::Rel* rel,
                                    size_t rel_count,
                                    const ElfSymbols* symbols,
                                    SymbolResolver* resolver,
                                    Error* error) {
  RLOG("%s: rel=%p rel_count=%d\n", __FUNCTION__, rel, rel_count);

  if (!rel)
    return true;

  for (size_t rel_n = 0; rel_n < rel_count; rel++, rel_n++) {
    RLOG("  Relocation %d of %d:\n", rel_n + 1, rel_count);

    if (!ApplyRelReloc(rel, symbols, resolver, error))
      return false;
  }

  return true;
}

bool ElfRelocations::ApplyRelaReloc(const ELF::Rela* rela,
                                    const ElfSymbols* symbols,
                                    SymbolResolver* resolver,
                                    Error* error) {
  const ELF::Word rel_type = ELF_R_TYPE(rela->r_info);
  const ELF::Word rel_symbol = ELF_R_SYM(rela->r_info);

  ELF::Addr sym_addr = 0;
  ELF::Addr reloc = static_cast<ELF::Addr>(rela->r_offset + load_bias_);
  RLOG("  reloc=%p offset=%p type=%d symbol=%d\n",
       reloc,
       rela->r_offset,
       rel_type,
       rel_symbol);

  if (rel_type == 0)
    return true;

  bool resolved = false;

  // If this is a symbolic relocation, compute the symbol's address.
  if (__builtin_expect(rel_symbol != 0, 0)) {
    if (!ResolveSymbol(rel_type,
                       rel_symbol,
                       symbols,
                       resolver,
                       reloc,
                       &sym_addr,
                       error)) {
      return false;
    }
    resolved = true;
  }

  return ApplyResolvedRelaReloc(rela, sym_addr, resolved, error);
}

bool ElfRelocations::ApplyRelaRelocs(const ELF::Rela* rela,
                                     size_t rela_count,
                                     const ElfSymbols* symbols,
                                     SymbolResolver* resolver,
                                     Error* error) {
  RLOG("%s: rela=%p rela_count=%d\n", __FUNCTION__, rela, rela_count);

  if (!rela)
    return true;

  for (size_t rel_n = 0; rel_n < rela_count; rela++, rel_n++) {
    RLOG("  Relocation %d of %d:\n", rel_n + 1, rela_count);

    if (!ApplyRelaReloc(rela, symbols, resolver, error))
      return false;
  }

  return true;
}

#ifdef __mips__
bool ElfRelocations::RelocateMipsGot(const ElfSymbols* symbols,
                                     SymbolResolver* resolver,
                                     Error* error) {
  if (!plt_got_)
    return true;

  // Handle the local GOT entries.
  // This mimics what the system linker does.
  // Note from the system linker:
  // got[0]: lazy resolver function address.
  // got[1]: may be used for a GNU extension.
  // Set it to a recognizable address in case someone calls it
  // (should be _rtld_bind_start).
  ELF::Addr* got = plt_got_;
  got[0] = 0xdeadbeef;
  if (got[1] & 0x80000000)
    got[1] = 0xdeadbeef;

  for (ELF::Addr n = 2; n < mips_local_got_count_; ++n)
    got[n] += load_bias_;

  // Handle the global GOT entries.
  got += mips_local_got_count_;
  for (size_t idx = mips_gotsym_; idx < mips_symtab_count_; idx++, got++) {
    const char* sym_name = symbols->LookupNameById(idx);
    void* sym_addr = resolver->Lookup(sym_name);
    if (sym_addr) {
      // Found symbol, update GOT entry.
      *got = reinterpret_cast<ELF::Addr>(sym_addr);
      continue;
    }

    if (symbols->IsWeakById(idx)) {
      // Undefined symbols are only ok if this is a weak reference.
      // Update GOT entry to 0 though.
      *got = 0;
      continue;
    }

    error->Format("Cannot locate symbol %s", sym_name);
    return false;
  }

  return true;
}
#endif  // __mips__

void ElfRelocations::AdjustRelocation(ELF::Word rel_type,
                                      ELF::Addr src_reloc,
                                      size_t dst_delta,
                                      size_t map_delta) {
  ELF::Addr* dst_ptr = reinterpret_cast<ELF::Addr*>(src_reloc + dst_delta);

  switch (rel_type) {
#ifdef __arm__
    case R_ARM_RELATIVE:
      *dst_ptr += map_delta;
      break;
#endif  // __arm__

#ifdef __aarch64__
    case R_AARCH64_RELATIVE:
      *dst_ptr += map_delta;
      break;
#endif  // __aarch64__

#ifdef __i386__
    case R_386_RELATIVE:
      *dst_ptr += map_delta;
      break;
#endif

#ifdef __x86_64__
    case R_X86_64_RELATIVE:
      *dst_ptr += map_delta;
      break;
#endif

#ifdef __mips__
    case R_MIPS_REL32:
      *dst_ptr += map_delta;
      break;
#endif
    default:
      ;
  }
}

void ElfRelocations::AdjustAndroidRelocation(const ELF::Rela* relocation,
                                             size_t src_addr,
                                             size_t dst_addr,
                                             size_t map_addr,
                                             size_t size) {
  // Add this value to each source address to get the corresponding
  // destination address.
  const size_t dst_delta = dst_addr - src_addr;
  const size_t map_delta = map_addr - src_addr;

  const ELF::Word rel_type = ELF_R_TYPE(relocation->r_info);
  const ELF::Word rel_symbol = ELF_R_SYM(relocation->r_info);
  ELF::Addr src_reloc =
      static_cast<ELF::Addr>(relocation->r_offset + load_bias_);

  if (rel_type == 0 || rel_symbol != 0) {
    // Ignore empty and symbolic relocations
    return;
  }

  if (src_reloc < src_addr || src_reloc >= src_addr + size) {
    // Ignore entries that don't relocate addresses inside the source section.
    return;
  }

  AdjustRelocation(rel_type, src_reloc, dst_delta, map_delta);
}

// Args for ApplyAndroidRelocation handler function.
struct RelocateAndroidRelocationArgs {
  size_t src_addr;
  size_t dst_addr;
  size_t map_addr;
  size_t size;
};

// Static ForEachAndroidRelocation() handler.
bool ElfRelocations::RelocateAndroidRelocation(ElfRelocations* relocations,
                                               const ELF::Rela* relocation,
                                               void* opaque) {
  // Unpack args from opaque, to obtain addrs and size;
  RelocateAndroidRelocationArgs* args =
      reinterpret_cast<RelocateAndroidRelocationArgs*>(opaque);
  const size_t src_addr = args->src_addr;
  const size_t dst_addr = args->dst_addr;
  const size_t map_addr = args->map_addr;
  const size_t size = args->size;

  // Relocate the given relocation.  Because the r_addend field is ignored
  // in relocating RELA relocations we do not need to convert from REL to
  // RELA and supply alternative relocator functions; instead we can work
  // here directly on the RELA supplied by ForEachAndroidRelocation(), even
  // on REL architectures.
  relocations->AdjustAndroidRelocation(relocation,
                                       src_addr,
                                       dst_addr,
                                       map_addr,
                                       size);
  return true;
}

void ElfRelocations::RelocateAndroidRelocations(size_t src_addr,
                                                size_t dst_addr,
                                                size_t map_addr,
                                                size_t size) {
  if (!android_relocations_)
    return;

  assert(IsValidAndroidPackedRelocations(android_relocations_,
                                         android_relocations_size_));

  RelocateAndroidRelocationArgs args;
  args.src_addr = src_addr;
  args.dst_addr = dst_addr;
  args.map_addr = map_addr;
  args.size = size;
  ForEachAndroidRelocation(&RelocateAndroidRelocation, &args);
}

template<typename Rel>
void ElfRelocations::RelocateRelocations(size_t src_addr,
                                         size_t dst_addr,
                                         size_t map_addr,
                                         size_t size) {
  // Add this value to each source address to get the corresponding
  // destination address.
  const size_t dst_delta = dst_addr - src_addr;
  const size_t map_delta = map_addr - src_addr;

  // Ignore PLT relocations, which all target symbols (ignored here).
  const Rel* rel = reinterpret_cast<Rel*>(relocations_);
  const size_t relocations_count = relocations_size_ / sizeof(Rel);
  const Rel* rel_limit = rel + relocations_count;

  for (; rel < rel_limit; ++rel) {
    const ELF::Word rel_type = ELF_R_TYPE(rel->r_info);
    const ELF::Word rel_symbol = ELF_R_SYM(rel->r_info);
    ELF::Addr src_reloc = static_cast<ELF::Addr>(rel->r_offset + load_bias_);

    if (rel_type == 0 || rel_symbol != 0) {
      // Ignore empty and symbolic relocations
      continue;
    }

    if (src_reloc < src_addr || src_reloc >= src_addr + size) {
      // Ignore entries that don't relocate addresses inside the source section.
      continue;
    }

    AdjustRelocation(rel_type, src_reloc, dst_delta, map_delta);
  }
}

template void ElfRelocations::RelocateRelocations<ELF::Rel>(
    size_t src_addr, size_t dst_addr, size_t map_addr, size_t size);

template void ElfRelocations::RelocateRelocations<ELF::Rela>(
    size_t src_addr, size_t dst_addr, size_t map_addr, size_t size);

void ElfRelocations::CopyAndRelocate(size_t src_addr,
                                     size_t dst_addr,
                                     size_t map_addr,
                                     size_t size) {
  // First, a straight copy.
  ::memcpy(reinterpret_cast<void*>(dst_addr),
           reinterpret_cast<void*>(src_addr),
           size);

  // Relocate android relocations.
  RelocateAndroidRelocations(src_addr, dst_addr, map_addr, size);

  // Relocate relocations.
  if (relocations_type_ == DT_REL)
    RelocateRelocations<ELF::Rel>(src_addr, dst_addr, map_addr, size);

  if (relocations_type_ == DT_RELA)
    RelocateRelocations<ELF::Rela>(src_addr, dst_addr, map_addr, size);

#ifdef __mips__
  // Add this value to each source address to get the corresponding
  // destination address.
  const size_t dst_delta = dst_addr - src_addr;
  const size_t map_delta = map_addr - src_addr;

  // Only relocate local GOT entries.
  ELF::Addr* got = plt_got_;
  if (got) {
    for (ELF::Addr n = 2; n < mips_local_got_count_; ++n) {
      size_t got_addr = reinterpret_cast<size_t>(&got[n]);
      if (got_addr < src_addr || got_addr >= src_addr + size)
        continue;
      ELF::Addr* dst_ptr = reinterpret_cast<ELF::Addr*>(got_addr + dst_delta);
      *dst_ptr += map_delta;
    }
  }
#endif
}

}  // namespace crazy
