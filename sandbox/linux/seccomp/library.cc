// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define XOPEN_SOURCE 500
#include <algorithm>
#include <elf.h>
#include <errno.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <linux/unistd.h>
#include <set>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "allocator.h"
#include "debug.h"
#include "library.h"
#include "sandbox_impl.h"
#include "syscall.h"
#include "syscall_table.h"
#include "x86_decode.h"

#if defined(__x86_64__)
typedef Elf64_Phdr    Elf_Phdr;
typedef Elf64_Rela    Elf_Rel;

typedef Elf64_Half    Elf_Half;
typedef Elf64_Word    Elf_Word;
typedef Elf64_Sword   Elf_Sword;
typedef Elf64_Xword   Elf_Xword;
typedef Elf64_Sxword  Elf_Sxword;
typedef Elf64_Off     Elf_Off;
typedef Elf64_Section Elf_Section;
typedef Elf64_Versym  Elf_Versym;

#define ELF_ST_BIND   ELF64_ST_BIND
#define ELF_ST_TYPE   ELF64_ST_TYPE
#define ELF_ST_INFO   ELF64_ST_INFO
#define ELF_R_SYM     ELF64_R_SYM
#define ELF_R_TYPE    ELF64_R_TYPE
#define ELF_R_INFO    ELF64_R_INFO

#define ELF_REL_PLT   ".rela.plt"
#define ELF_JUMP_SLOT R_X86_64_JUMP_SLOT
#elif defined(__i386__)
typedef Elf32_Phdr    Elf_Phdr;
typedef Elf32_Rel     Elf_Rel;

typedef Elf32_Half    Elf_Half;
typedef Elf32_Word    Elf_Word;
typedef Elf32_Sword   Elf_Sword;
typedef Elf32_Xword   Elf_Xword;
typedef Elf32_Sxword  Elf_Sxword;
typedef Elf32_Off     Elf_Off;
typedef Elf32_Section Elf_Section;
typedef Elf32_Versym  Elf_Versym;

#define ELF_ST_BIND   ELF32_ST_BIND
#define ELF_ST_TYPE   ELF32_ST_TYPE
#define ELF_ST_INFO   ELF32_ST_INFO
#define ELF_R_SYM     ELF32_R_SYM
#define ELF_R_TYPE    ELF32_R_TYPE
#define ELF_R_INFO    ELF32_R_INFO

#define ELF_REL_PLT   ".rel.plt"
#define ELF_JUMP_SLOT R_386_JMP_SLOT
#else
#error Unsupported target platform
#endif

namespace playground {

char* Library::__kernel_vsyscall;
char* Library::__kernel_sigreturn;
char* Library::__kernel_rt_sigreturn;

Library::~Library() {
  if (image_size_) {
    // We no longer need access to a full mapping of the underlying library
    // file. Move the temporarily extended mapping back to where we originally
    // found. Make sure to preserve any changes that we might have made since.
    Sandbox::SysCalls sys;
    sys.mprotect(image_, 4096, PROT_READ | PROT_WRITE | PROT_EXEC);
    if (memcmp(image_, memory_ranges_.rbegin()->second.start, 4096)) {
      // Only copy data, if we made any changes in this data. Otherwise there
      // is no need to create another modified COW mapping.
      memcpy(image_, memory_ranges_.rbegin()->second.start, 4096);
    }
    sys.mprotect(image_, 4096, PROT_READ | PROT_EXEC);
    sys.mremap(image_, image_size_, 4096, MREMAP_MAYMOVE | MREMAP_FIXED,
               memory_ranges_.rbegin()->second.start);
  }
}

char* Library::getBytes(char* dst, const char* src, ssize_t len) {
  // Some kernels don't allow accessing the VDSO from write()
  if (isVDSO_ &&
      src >= memory_ranges_.begin()->second.start &&
      src <= memory_ranges_.begin()->second.stop) {
    ssize_t max =
      reinterpret_cast<char *>(memory_ranges_.begin()->second.stop) - src;
    if (len > max) {
      len = max;
    }
    memcpy(dst, src, len);
    return dst;
  }

  // Read up to "len" bytes from "src" and copy them to "dst". Short
  // copies are possible, if we are at the end of a mapping. Returns
  // NULL, if the operation failed completely.
  static int helper_socket[2];
  Sandbox::SysCalls sys;
  if (!helper_socket[0] && !helper_socket[1]) {
    // Copy data through a socketpair, as this allows us to access it
    // without incurring a segmentation fault.
    sys.socketpair(AF_UNIX, SOCK_STREAM, 0, helper_socket);
  }
  char* ptr = dst;
  int   inc = 4096;
  while (len > 0) {
    ssize_t l = inc == 1 ? inc : 4096 - (reinterpret_cast<long>(src) & 0xFFF);
    if (l > len) {
      l = len;
    }
    l = NOINTR_SYS(sys.write(helper_socket[0], src, l));
    if (l == -1) {
      if (sys.my_errno == EFAULT) {
        if (inc == 1) {
          if (ptr == dst) {
            return NULL;
          }
          break;
        }
        inc = 1;
        continue;
      } else {
        return NULL;
      }
    }
    l = sys.read(helper_socket[1], ptr, l);
    if (l <= 0) {
      return NULL;
    }
    ptr += l;
    src += l;
    len -= l;
  }
  return dst;
}

char *Library::get(Elf_Addr offset, char *buf, size_t len) {
  if (!valid_) {
    memset(buf, 0, len);
    return NULL;
  }
  RangeMap::const_iterator iter = memory_ranges_.lower_bound(offset);
  if (iter == memory_ranges_.end()) {
    memset(buf, 0, len);
    return NULL;
  }
  offset -= iter->first;
  long size = reinterpret_cast<char *>(iter->second.stop) -
              reinterpret_cast<char *>(iter->second.start);
  if (offset > size - len) {
    memset(buf, 0, len);
    return NULL;
  }
  char *src = reinterpret_cast<char *>(iter->second.start) + offset;
  memset(buf, 0, len);
  if (!getBytes(buf, src, len)) {
    return NULL;
  }
  return buf;
}

Library::string Library::get(Elf_Addr offset) {
  if (!valid_) {
    return "";
  }
  RangeMap::const_iterator iter = memory_ranges_.lower_bound(offset);
  if (iter == memory_ranges_.end()) {
    return "";
  }
  offset -= iter->first;
  const char *start = reinterpret_cast<char *>(iter->second.start) + offset;
  const char *stop  = reinterpret_cast<char *>(iter->second.stop) + offset;
  char buf[4096]    = { 0 };
  getBytes(buf, start, stop - start >= (int)sizeof(buf) ?
           sizeof(buf) - 1 : stop - start);
  start             = buf;
  stop              = buf;
  while (*stop) {
    ++stop;
  }
  string s = stop > start ? string(start, stop - start) : "";
  return s;
}

char *Library::getOriginal(Elf_Addr offset, char *buf, size_t len) {
  if (!valid_) {
    memset(buf, 0, len);
    return NULL;
  }
  Sandbox::SysCalls sys;
  if (!image_ && !isVDSO_ && !memory_ranges_.empty() &&
      memory_ranges_.rbegin()->first == 0) {
    // Extend the mapping of the very first page of the underlying library
    // file. This way, we can read the original file contents of the entire
    // library.
    // We have to be careful, because doing so temporarily removes the first
    // 4096 bytes of the library from memory. And we don't want to accidentally
    // unmap code that we are executing. So, only use functions that can be
    // inlined.
    void* start = memory_ranges_.rbegin()->second.start;
    image_size_ = memory_ranges_.begin()->first +
      (reinterpret_cast<char *>(memory_ranges_.begin()->second.stop) -
       reinterpret_cast<char *>(memory_ranges_.begin()->second.start));
    if (image_size_ < 8192) {
      // It is possible to create a library that is only a single page in
      // size. In that case, we have to make sure that we artificially map
      // one extra page past the end of it, as our code relies on mremap()
      // actually moving the mapping.
      image_size_ = 8192;
    }
    image_ = reinterpret_cast<char *>(sys.mremap(start, 4096, image_size_,
                                                 MREMAP_MAYMOVE));
    if (image_size_ == 8192 && image_ == start) {
      // We really mean it, when we say we want the memory to be moved.
      image_ = reinterpret_cast<char *>(sys.mremap(start, 4096, image_size_,
                                                   MREMAP_MAYMOVE));
      sys.munmap(reinterpret_cast<char *>(start) + 4096, 4096);
    }
    if (image_ == MAP_FAILED) {
      image_ = NULL;
    } else {
      sys.MMAP(start, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
      for (int i = 4096 / sizeof(long); --i;
           reinterpret_cast<long *>(start)[i] =
             reinterpret_cast<long *>(image_)[i]);
    }
  }

  if (image_) {
    if (offset + len > image_size_) {
      // It is quite likely that we initially did not map the entire file as
      // we did not know how large it is. So, if necessary, try to extend the
      // mapping.
      size_t new_size = (offset + len + 4095) & ~4095;
      char* tmp =
        reinterpret_cast<char *>(sys.mremap(image_, image_size_, new_size,
                                            MREMAP_MAYMOVE));
      if (tmp != MAP_FAILED) {
        image_      = tmp;
        image_size_ = new_size;
      }
    }
    if (buf && offset + len <= image_size_) {
      return reinterpret_cast<char *>(memcpy(buf, image_ + offset, len));
    }
    return NULL;
  }
  return buf ? get(offset, buf, len) : NULL;
}

Library::string Library::getOriginal(Elf_Addr offset) {
  if (!valid_) {
    return "";
  }
  // Make sure we actually have a mapping that we can access. If the string
  // is located at the end of the image, we might not yet have extended the
  // mapping sufficiently.
  if (!image_ || image_size_ <= offset) {
    getOriginal(offset, NULL, 1);
  }

  if (image_) {
    if (offset < image_size_) {
      char* start = image_ + offset;
      char* stop  = start;
      while (stop < image_ + image_size_ && *stop) {
        ++stop;
        if (stop >= image_ + image_size_) {
          getOriginal(stop - image_, NULL, 1);
        }
      }
      return string(start, stop - start);
    }
    return "";
  }
  return get(offset);
}

const Elf_Ehdr* Library::getEhdr() {
  if (!valid_) {
    return NULL;
  }
  return &ehdr_;
}

const Elf_Shdr* Library::getSection(const string& section) {
  if (!valid_) {
    return NULL;
  }
  SectionTable::const_iterator iter = section_table_.find(section);
  if (iter == section_table_.end()) {
    return NULL;
  }
  return &iter->second.second;
}

int Library::getSectionIndex(const string& section) {
  if (!valid_) {
    return -1;
  }
  SectionTable::const_iterator iter = section_table_.find(section);
  if (iter == section_table_.end()) {
    return -1;
  }
  return iter->second.first;
}

void Library::makeWritable(bool state) const {
  for (RangeMap::const_iterator iter = memory_ranges_.begin();
       iter != memory_ranges_.end(); ++iter) {
    const Range& range = iter->second;
    long length = reinterpret_cast<char *>(range.stop) -
                  reinterpret_cast<char *>(range.start);
    Sandbox::SysCalls sys;
    sys.mprotect(range.start, length,
                 range.prot | (state ? PROT_WRITE : 0));
  }
}

bool Library::isSafeInsn(unsigned short insn) {
  // Check if the instruction has no unexpected side-effects. If so, it can
  // be safely relocated from the function that we are patching into the
  // out-of-line scratch space that we are setting up. This is often necessary
  // to make room for the JMP into the scratch space.
  return ((insn & 0x7) < 0x6 && (insn & 0xF0) < 0x40
          /* ADD, OR, ADC, SBB, AND, SUB, XOR, CMP */) ||
         #if defined(__x86_64__)
         insn == 0x63 /* MOVSXD */ ||
         #endif
         (insn >= 0x80 && insn <= 0x8E /* ADD, OR, ADC,
         SBB, AND, SUB, XOR, CMP, TEST, XCHG, MOV, LEA */) ||
         (insn == 0x90) || /* NOP */
         (insn >= 0xA0 && insn <= 0xA9) /* MOV, TEST */ ||
         (insn >= 0xB0 && insn <= 0xBF /* MOV */) ||
         (insn >= 0xC0 && insn <= 0xC1) || /* Bit Shift */
         (insn >= 0xD0 && insn <= 0xD3) || /* Bit Shift */
         (insn >= 0xC6 && insn <= 0xC7 /* MOV */) ||
         (insn == 0xF7) /* TEST, NOT, NEG, MUL, IMUL, DIV, IDIV */;
}

char* Library::getScratchSpace(const Maps* maps, char* near, int needed,
                               char** extraSpace, int* extraLength) {
  if (needed > *extraLength ||
      labs(*extraSpace - reinterpret_cast<char *>(near)) > (1536 << 20)) {
    if (*extraSpace) {
      // Start a new scratch page and mark any previous page as write-protected
      Sandbox::SysCalls sys;
      sys.mprotect(*extraSpace, 4096, PROT_READ|PROT_EXEC);
    }
    // Our new scratch space is initially executable and writable.
    *extraLength = 4096;
    *extraSpace = maps->allocNearAddr(near, *extraLength,
                                      PROT_READ|PROT_WRITE|PROT_EXEC);
  }
  if (*extraSpace) {
    *extraLength -= needed;
    return *extraSpace + *extraLength;
  }
  Sandbox::die("Insufficient space to intercept system call");
}

void Library::patchSystemCallsInFunction(const Maps* maps, char *start,
                                         char *end, char** extraSpace,
                                         int* extraLength) {
  std::set<char *, std::less<char *>, SystemAllocator<char *> > branch_targets;
  for (char *ptr = start; ptr < end; ) {
    unsigned short insn = next_inst((const char **)&ptr, __WORDSIZE == 64);
    char *target;
    if ((insn >= 0x70 && insn <= 0x7F) /* Jcc */ || insn == 0xEB /* JMP */) {
      target = ptr + (reinterpret_cast<signed char *>(ptr))[-1];
    } else if (insn == 0xE8 /* CALL */ || insn == 0xE9 /* JMP */ ||
               (insn >= 0x0F80 && insn <= 0x0F8F) /* Jcc */) {
      target = ptr + (reinterpret_cast<int *>(ptr))[-1];
    } else {
      continue;
    }
    branch_targets.insert(target);
  }
  struct Code {
    char*          addr;
    int            len;
    unsigned short insn;
    bool           is_ip_relative;
  } code[5] = { { 0 } };
  int codeIdx = 0;
  char* ptr = start;
  while (ptr < end) {
    // Keep a ring-buffer of the last few instruction in order to find the
    // correct place to patch the code.
    char *mod_rm;
    code[codeIdx].addr = ptr;
    code[codeIdx].insn = next_inst((const char **)&ptr, __WORDSIZE == 64,
                                   0, 0, &mod_rm, 0, 0);
    code[codeIdx].len = ptr - code[codeIdx].addr;
    code[codeIdx].is_ip_relative =
      #if defined(__x86_64__)
        mod_rm && (*mod_rm & 0xC7) == 0x5;
      #else
        false;
      #endif

    // Whenever we find a system call, we patch it with a jump to out-of-line
    // code that redirects to our system call wrapper.
    bool is_syscall = true;
    #if defined(__x86_64__)
    bool is_indirect_call = false;
    if (code[codeIdx].insn == 0x0F05 /* SYSCALL */ ||
        // In addition, on x86-64, we need to redirect all CALLs between the
        // VDSO and the VSyscalls page. We want these to jump to our own
        // modified copy of the VSyscalls. As we know that the VSyscalls are
        // always more than 2GB away from the VDSO, the compiler has to
        // generate some form of indirect jumps. We can find all indirect
        // CALLs and redirect them to a separate scratch area, where we can
        // inspect the destination address. If it indeed points to the
        // VSyscall area, we then adjust the destination address accordingly.
        (is_indirect_call =
         (isVDSO_ && vsys_offset_ && code[codeIdx].insn == 0xFF &&
          !code[codeIdx].is_ip_relative &&
          mod_rm && (*mod_rm & 0x38) == 0x10 /* CALL (indirect) */))) {
      is_syscall = !is_indirect_call;
    #elif defined(__i386__)
    bool is_gs_call = false;
    if (code[codeIdx].len  == 7 &&
        code[codeIdx].insn == 0xFF &&
        code[codeIdx].addr[2] == '\x15' /* CALL (indirect) */ &&
        code[codeIdx].addr[0] == '\x65' /* %gs prefix */) {
      char* target;
      asm volatile("mov %%gs:(%1), %0\n"
                   : "=a"(target)
                   : "c"(*reinterpret_cast<int *>(code[codeIdx].addr+3)));
      if (target == __kernel_vsyscall) {
        is_gs_call = true;
        // TODO(markus): also handle the other vsyscalls
      }
    }
    if (is_gs_call ||
        (code[codeIdx].insn == 0xCD &&
         code[codeIdx].addr[1] == '\x80' /* INT $0x80 */)) {
    #else
    #error Unsupported target platform
    #endif
      // Found a system call. Search backwards to figure out how to redirect
      // the code. We will need to overwrite a couple of instructions and,
      // of course, move these instructions somewhere else.
      int startIdx = codeIdx;
      int endIdx = codeIdx;
      int length = code[codeIdx].len;
      for (int idx = codeIdx;
           (idx = (idx + (sizeof(code) / sizeof(struct Code)) - 1) %
                  (sizeof(code) / sizeof(struct Code))) != codeIdx; ) {
        std::set<char *>::const_iterator iter =
            std::upper_bound(branch_targets.begin(), branch_targets.end(),
                             code[idx].addr);
        if (iter != branch_targets.end() && *iter < ptr) {
          // Found a branch pointing to somewhere past our instruction. This
          // instruction cannot be moved safely. Leave it in place.
          break;
        }
        if (code[idx].addr && !code[idx].is_ip_relative &&
            isSafeInsn(code[idx].insn)) {
          // These are all benign instructions with no side-effects and no
          // dependency on the program counter. We should be able to safely
          // relocate them.
          startIdx = idx;
          length   = ptr - code[startIdx].addr;
        } else {
          break;
        }
      }
      // Search forward past the system call, too. Sometimes, we can only
      // find relocatable instructions following the system call.
      #if defined(__i386__)
   findEndIdx:
      #endif
      char *next = ptr;
      for (int i = codeIdx;
           next < end &&
           (i = (i + 1) % (sizeof(code) / sizeof(struct Code))) != startIdx;
           ) {
        std::set<char *>::const_iterator iter =
            std::lower_bound(branch_targets.begin(), branch_targets.end(),
                             next);
        if (iter != branch_targets.end() && *iter == next) {
          // Found branch target pointing to our instruction
          break;
        }
        char *tmp_rm;
        code[i].addr = next;
        code[i].insn = next_inst((const char **)&next, __WORDSIZE == 64,
                                 0, 0, &tmp_rm, 0, 0);
        code[i].len = next - code[i].addr;
        code[i].is_ip_relative = tmp_rm && (*tmp_rm & 0xC7) == 0x5;
        if (!code[i].is_ip_relative && isSafeInsn(code[i].insn)) {
          endIdx = i;
          length = next - code[startIdx].addr;
        } else {
          break;
        }
      }
      // We now know, how many instructions neighboring the system call we
      // can safely overwrite. On x86-32 we need six bytes, and on x86-64
      // We need five bytes to insert a JMPQ and a 32bit address. We then
      // jump to a code fragment that safely forwards to our system call
      // wrapper.
      // On x86-64, this is complicated by the fact that the API allows up
      // to 128 bytes of red-zones below the current stack pointer. So, we
      // cannot write to the stack until we have adjusted the stack
      // pointer.
      // On both x86-32 and x86-64 we take care to leave the stack unchanged
      // while we are executing the preamble and postamble. This allows us
      // to treat instructions that reference %esp/%rsp as safe for
      // relocation.
      // In particular, this means that on x86-32 we cannot use CALL, but
      // have to use a PUSH/RET combination to change the instruction pointer.
      // On x86-64, we can instead use a 32bit JMPQ.
      //
      // .. .. .. .. ; any leading instructions copied from original code
      // 48 81 EC 80 00 00 00        SUB  $0x80, %rsp
      // 50                          PUSH %rax
      // 48 8D 05 .. .. .. ..        LEA  ...(%rip), %rax
      // 50                          PUSH %rax
      // 48 B8 .. .. .. ..           MOV  $syscallWrapper, %rax
      // .. .. .. ..
      // 50                          PUSH %rax
      // 48 8D 05 06 00 00 00        LEA  6(%rip), %rax
      // 48 87 44 24 10              XCHG %rax, 16(%rsp)
      // C3                          RETQ
      // 48 81 C4 80 00 00 00        ADD  $0x80, %rsp
      // .. .. .. .. ; any trailing instructions copied from original code
      // E9 .. .. .. ..              JMPQ ...
      //
      // Total: 52 bytes + any bytes that were copied
      //
      // On x86-32, the stack is available and we can do:
      //
      // TODO(markus): Try to maintain frame pointers on x86-32
      //
      // .. .. .. .. ; any leading instructions copied from original code
      // 68 .. .. .. ..              PUSH return_addr
      // 68 .. .. .. ..              PUSH $syscallWrapper
      // C3                          RET
      // .. .. .. .. ; any trailing instructions copied from original code
      // 68 .. .. .. ..              PUSH return_addr
      // C3                          RET
      //
      // Total: 17 bytes + any bytes that were copied
      //
      // For indirect jumps from the VDSO to the VSyscall page, we instead
      // replace the following code (this is only necessary on x86-64). This
      // time, we don't have to worry about red zones:
      //
      // .. .. .. .. ; any leading instructions copied from original code
      // E8 00 00 00 00              CALL .
      // 48 83 04 24 ..              ADDQ $.., (%rsp)
      // FF .. .. .. .. ..           PUSH ..  ; from original CALL instruction
      // 48 81 3C 24 00 00 00 FF     CMPQ $0xFFFFFFFFFF000000, 0(%rsp)
      // 72 10                       JB   . + 16
      // 81 2C 24 .. .. .. ..        SUBL ..., 0(%rsp)
      // C7 44 24 04 00 00 00 00     MOVL $0, 4(%rsp)
      // C3                          RETQ
      // 48 87 04 24                 XCHG %rax,(%rsp)
      // 48 89 44 24 08              MOV  %rax,0x8(%rsp)
      // 58                          POP  %rax
      // C3                          RETQ
      // .. .. .. .. ; any trailing instructions copied from original code
      // E9 .. .. .. ..              JMPQ ...
      //
      // Total: 52 bytes + any bytes that were copied

      if (length < (__WORDSIZE == 32 ? 6 : 5)) {
        // There are a very small number of instruction sequences that we
        // cannot easily intercept, and that have been observed in real world
        // examples. Handle them here:
        #if defined(__i386__)
        int diff;
        if (!memcmp(code[codeIdx].addr, "\xCD\x80\xEB", 3) &&
            (diff = *reinterpret_cast<signed char *>(
                 code[codeIdx].addr + 3)) < 0 && diff >= -6) {
          // We have seen...
          //   for (;;) {
          //      _exit(0);
          //   }
          // ..get compiled to:
          //   B8 01 00 00 00      MOV  $__NR_exit, %eax
          //   66 90               XCHG %ax, %ax
          //   31 DB             0:XOR  %ebx, %ebx
          //   CD 80               INT  $0x80
          //   EB FA               JMP  0b
          // The JMP is really superfluous as the system call never returns.
          // And there are in fact no returning system calls that need to be
          // unconditionally repeated in an infinite loop.
          // If we replace the JMP with NOPs, the system call can successfully
          // be intercepted.
          *reinterpret_cast<unsigned short *>(code[codeIdx].addr + 2) = 0x9090;
          goto findEndIdx;
        }
        #elif defined(__x86_64__)
        std::set<char *>::const_iterator iter;
        #endif
        // If we cannot figure out any other way to intercept this system call,
        // we replace it with a call to INT0. This causes a SEGV which we then
        // handle in the signal handler. That's a lot slower than rewriting the
        // instruction with a jump, but it should only happen very rarely.
        if (is_syscall) {
          memcpy(code[codeIdx].addr, "\xCD", 2);
          if (code[codeIdx].len > 2) {
            memset(code[codeIdx].addr + 2, 0x90, code[codeIdx].len - 2);
          }
          goto replaced;
        }
        #if defined(__x86_64__)
        // On x86-64, we occasionally see code like this in the VDSO:
        //   48 8B 05 CF FE FF FF  MOV   -0x131(%rip),%rax
        //   FF 50 20              CALLQ *0x20(%rax)
        // By default, we would not replace the MOV instruction, as it is
        // IP relative. But if the following instruction is also IP relative,
        // we are left with only three bytes which is not enough to insert a
        // jump.
        // We recognize this particular situation, and as long as the CALLQ
        // is not a branch target, we decide to still relocate the entire
        // sequence. We just have to make sure that we then patch up the
        // IP relative addressing.
        else if (is_indirect_call && startIdx == codeIdx &&
                 code[startIdx = (startIdx + (sizeof(code) /
                                              sizeof(struct Code)) - 1) %
                      (sizeof(code) / sizeof(struct Code))].addr &&
                 ptr - code[startIdx].addr >= 5 &&
                 code[startIdx].is_ip_relative &&
                 isSafeInsn(code[startIdx].insn) &&
                 ((iter = std::upper_bound(branch_targets.begin(),
                                           branch_targets.end(),
                                           code[startIdx].addr)) ==
                  branch_targets.end() || *iter >= ptr)) {
          // We changed startIdx to include the IP relative instruction.
          // When copying this preamble, we make sure to patch up the
          // offset.
        }
        #endif
        else {
          Sandbox::die("Cannot intercept system call");
        }
      }
      int needed = (__WORDSIZE == 32 ? 6 : 5) - code[codeIdx].len;
      int first = codeIdx;
      while (needed > 0 && first != startIdx) {
        first = (first + (sizeof(code) / sizeof(struct Code)) - 1) %
                (sizeof(code) / sizeof(struct Code));
        needed -= code[first].len;
      }
      int second = codeIdx;
      while (needed > 0) {
        second = (second + 1) % (sizeof(code) / sizeof(struct Code));
        needed -= code[second].len;
      }
      int preamble = code[codeIdx].addr - code[first].addr;
      int postamble = code[second].addr + code[second].len -
                      code[codeIdx].addr - code[codeIdx].len;

      // The following is all the code that construct the various bits of
      // assembly code.
      #if defined(__x86_64__)
      if (is_indirect_call) {
        needed = 52 + preamble + code[codeIdx].len + postamble;
      } else {
        needed = 52 + preamble + postamble;
      }
      #elif defined(__i386__)
      needed = 17 + preamble + postamble;
      #else
      #error Unsupported target platform
      #endif

      // Allocate scratch space and copy the preamble of code that was moved
      // from the function that we are patching.
      char* dest = getScratchSpace(maps, code[first].addr, needed,
                                   extraSpace, extraLength);
      memcpy(dest, code[first].addr, preamble);

      // For jumps from the VDSO to the VSyscalls we sometimes allow exactly
      // one IP relative instruction in the preamble.
      if (code[first].is_ip_relative) {
        *reinterpret_cast<int *>(dest + (code[codeIdx].addr -
                                         code[first].addr) - 4)
          -= dest - code[first].addr;
      }

      // For indirect calls, we need to copy the actual CALL instruction and
      // turn it into a PUSH instruction.
      #if defined(__x86_64__)
      if (is_indirect_call) {
        memcpy(dest + preamble, "\xE8\x00\x00\x00\x00\x48\x83\x04\x24", 9);
        dest[preamble + 9] = code[codeIdx].len + 42;
        memcpy(dest + preamble + 10, code[codeIdx].addr, code[codeIdx].len);

        // Convert CALL -> PUSH
        dest[preamble + 10 + (mod_rm - code[codeIdx].addr)] |= 0x20;
        preamble += 10 + code[codeIdx].len;
      }
      #endif

      // Copy the static body of the assembly code.
      memcpy(dest + preamble,
           #if defined(__x86_64__)
           is_indirect_call ?
           "\x48\x81\x3C\x24\x00\x00\x00\xFF\x72\x10\x81\x2C\x24\x00\x00\x00"
           "\x00\xC7\x44\x24\x04\x00\x00\x00\x00\xC3\x48\x87\x04\x24\x48\x89"
           "\x44\x24\x08\x58\xC3" :
           "\x48\x81\xEC\x80\x00\x00\x00\x50\x48\x8D\x05\x00\x00\x00\x00\x50"
           "\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\x50\x48\x8D\x05\x06\x00"
           "\x00\x00\x48\x87\x44\x24\x10\xC3\x48\x81\xC4\x80\x00\x00",
           is_indirect_call ? 37 : 47
           #elif defined(__i386__)
           "\x68\x00\x00\x00\x00\x68\x00\x00\x00\x00\xC3", 11
           #else
           #error Unsupported target platform
           #endif
           );

      // Copy the postamble that was moved from the function that we are
      // patching.
      memcpy(dest + preamble +
             #if defined(__x86_64__)
             (is_indirect_call ? 37 : 47),
             #elif defined(__i386__)
             11,
             #else
             #error Unsupported target platform
             #endif
             code[codeIdx].addr + code[codeIdx].len,
             postamble);

      // Patch up the various computed values
      #if defined(__x86_64__)
      int post = preamble + (is_indirect_call ? 37 : 47) + postamble;
      dest[post] = '\xE9';
      *reinterpret_cast<int *>(dest + post + 1) =
          (code[second].addr + code[second].len) - (dest + post + 5);
      if (is_indirect_call) {
        *reinterpret_cast<int *>(dest + preamble + 13) = vsys_offset_;
      } else {
        *reinterpret_cast<int *>(dest + preamble + 11) =
            (code[second].addr + code[second].len) - (dest + preamble + 15);
        *reinterpret_cast<void **>(dest + preamble + 18) =
            reinterpret_cast<void *>(&syscallWrapper);
      }
      #elif defined(__i386__)
      *(dest + preamble + 11 + postamble) = '\x68'; // PUSH
      *reinterpret_cast<char **>(dest + preamble + 12 + postamble) =
          code[second].addr + code[second].len;
      *(dest + preamble + 16 + postamble) = '\xC3'; // RET
      *reinterpret_cast<char **>(dest + preamble + 1) =
          dest + preamble + 11;
      *reinterpret_cast<void (**)()>(dest + preamble + 6) = syscallWrapper;
      #else
      #error Unsupported target platform
      #endif

      // Pad unused space in the original function with NOPs
      memset(code[first].addr, 0x90 /* NOP */,
             code[second].addr + code[second].len - code[first].addr);

      // Replace the system call with an unconditional jump to our new code.
      #if defined(__x86_64__)
      *code[first].addr = '\xE9';   // JMPQ
      *reinterpret_cast<int *>(code[first].addr + 1) =
          dest - (code[first].addr + 5);
      #elif defined(__i386__)
      code[first].addr[0] = '\x68'; // PUSH
      *reinterpret_cast<char **>(code[first].addr + 1) = dest;
      code[first].addr[5] = '\xC3'; // RET
      #else
      #error Unsupported target platform
      #endif
    }
   replaced:
    codeIdx = (codeIdx + 1) % (sizeof(code) / sizeof(struct Code));
  }
}

void Library::patchVDSO(char** extraSpace, int* extraLength){
  #if defined(__i386__)
  Sandbox::SysCalls sys;
  if (!__kernel_vsyscall ||
      sys.mprotect(reinterpret_cast<void *>(
                     reinterpret_cast<long>(__kernel_vsyscall) & ~0xFFF),
                   4096, PROT_READ|PROT_WRITE|PROT_EXEC)) {
    return;
  }

  // x86-32 has a small number of well-defined functions in the VDSO library.
  // These functions do not easily lend themselves to be rewritten by the
  // automatic code. Instead, we explicitly find new definitions for them.
  //
  // We don't bother with optimizing the syscall instruction instead always
  // use INT $0x80, no matter whether the hardware supports more modern
  // calling conventions.
  //
  // TODO(markus): Investigate whether it is worthwhile to optimize this
  // code path and use the platform-specific entry code.
  if (__kernel_vsyscall) {
    // Replace the kernel entry point with:
    //
    // E9 .. .. .. ..    JMP syscallWrapper
    *__kernel_vsyscall = '\xE9';
    *reinterpret_cast<long *>(__kernel_vsyscall + 1) =
        reinterpret_cast<char *>(&syscallWrapper) -
        reinterpret_cast<char *>(__kernel_vsyscall + 5);
  }
  if (__kernel_sigreturn) {
    // Replace the sigreturn() system call with a jump to code that does:
    //
    // 58                POP %eax
    // B8 77 00 00 00    MOV $0x77, %eax
    // E8 .. .. .. ..    CALL syscallWrapper
    char* dest = getScratchSpace(maps_, __kernel_sigreturn, 11, extraSpace,
                                 extraLength);
    memcpy(dest, "\x58\xB8\x77\x00\x00\x00\xE8", 7);
    *reinterpret_cast<long *>(dest + 7) =
        reinterpret_cast<char *>(&syscallWrapper) - dest - 11;;
    *__kernel_sigreturn = '\xE9';
    *reinterpret_cast<long *>(__kernel_sigreturn + 1) =
        dest - reinterpret_cast<char *>(__kernel_sigreturn) - 5;
  }
  if (__kernel_rt_sigreturn) {
    // Replace the rt_sigreturn() system call with a jump to code that does:
    //
    // B8 AD 00 00 00    MOV $0xAD, %eax
    // E8 .. .. .. ..    CALL syscallWrapper
    char* dest = getScratchSpace(maps_, __kernel_rt_sigreturn, 10, extraSpace,
                                 extraLength);
    memcpy(dest, "\xB8\xAD\x00\x00\x00\xE8", 6);
    *reinterpret_cast<long *>(dest + 6) =
        reinterpret_cast<char *>(&syscallWrapper) - dest - 10;
    *__kernel_rt_sigreturn = '\xE9';
    *reinterpret_cast<long *>(__kernel_rt_sigreturn + 1) =
        dest - reinterpret_cast<char *>(__kernel_rt_sigreturn) - 5;
  }
  #endif
}

int Library::patchVSystemCalls() {
  #if defined(__x86_64__)
  // VSyscalls live in a shared 4kB page at the top of the address space. This
  // page cannot be unmapped nor remapped. We have to create a copy within
  // 2GB of the page, and rewrite all IP-relative accesses to shared variables.
  // As the top of the address space is not accessible by mmap(), this means
  // that we need to wrap around addresses to the bottom 2GB of the address
  // space.
  // Only x86-64 has VSyscalls.
  if (maps_->vsyscall()) {
    char* copy = maps_->allocNearAddr(maps_->vsyscall(), 0x1000,
                                      PROT_READ|PROT_WRITE|PROT_EXEC);
    char* extraSpace = copy;
    int extraLength = 0x1000;
    memcpy(copy, maps_->vsyscall(), 0x1000);
    long adjust = (long)maps_->vsyscall() - (long)copy;
    for (int vsys = 0; vsys < 0x1000; vsys += 0x400) {
      char* start = copy + vsys;
      char* end   = start + 0x400;

      // There can only be up to four VSyscalls starting at an offset of
      // n*0x1000, each. VSyscalls are invoked by functions in the VDSO
      // and provide fast implementations of a time source. We don't exactly
      // know where the code and where the data is in the VSyscalls page.
      // So, we disassemble the code for each function and find all branch
      // targets within the function in order to find the last address of
      // function.
      for (char *last = start, *vars = end, *ptr = start; ptr < end; ) {
     new_function:
        char* mod_rm;
        unsigned short insn = next_inst((const char **)&ptr, true, 0, 0,
                                        &mod_rm, 0, 0);
        if (mod_rm && (*mod_rm & 0xC7) == 0x5) {
          // Instruction has IP relative addressing mode. Adjust to reference
          // the variables in the original VSyscall segment.
          long offset = *reinterpret_cast<int *>(mod_rm + 1);
          char* var = ptr + offset;
          if (var >= ptr && var < vars) {
            // Variables are stored somewhere past all the functions. Remember
            // the first variable in the VSyscall slot, so that we stop
            // scanning for instructions once we reach that address.
            vars = var;
          }
          offset += adjust;
          if ((offset >> 32) && (offset >> 32) != -1) {
            Sandbox::die("Cannot patch [vsystemcall]");
          }
          *reinterpret_cast<int *>(mod_rm + 1) = offset;
        }

        // Check for jump targets to higher addresses (but within our own
        // VSyscall slot). They extend the possible end-address of this
        // function.
        char *target = 0;
        if ((insn >= 0x70 && insn <= 0x7F) /* Jcc */ ||
            insn == 0xEB /* JMP */) {
          target = ptr + (reinterpret_cast<signed char *>(ptr))[-1];
        } else if (insn == 0xE8 /* CALL */ || insn == 0xE9 /* JMP */ ||
                   (insn >= 0x0F80 && insn <= 0x0F8F) /* Jcc */) {
          target = ptr + (reinterpret_cast<int *>(ptr))[-1];
        }

        // The function end is found, once the loop reaches the last valid
        // address in the VSyscall slot, or once it finds a RET instruction
        // that is not followed by any jump targets. Unconditional jumps that
        // point backwards are treated the same as a RET instruction.
        if (insn == 0xC3 /* RET */ ||
            (target < ptr &&
             (insn == 0xEB /* JMP */ || insn == 0xE9 /* JMP */))) {
          if (last >= ptr) {
            continue;
          } else {
            // The function can optionally be followed by more functions in
            // the same VSyscall slot. Allow for alignment to a 16 byte
            // boundary. If we then find more non-zero bytes, and if this is
            // not the known start of the variables, assume a new function
            // started.
            for (; ptr < vars; ++ptr) {
              if ((long)ptr & 0xF) {
                if (*ptr && *ptr != '\x90' /* NOP */) {
                  goto new_function;
                }
                *ptr = '\x90'; // NOP
              } else {
                if (*ptr && *ptr != '\x90' /* NOP */) {
                  goto new_function;
                }
                break;
              }
            }

            // Translate all SYSCALLs to jumps into our system call handler.
            patchSystemCallsInFunction(NULL, start, ptr,
                                       &extraSpace, &extraLength);
            break;
          }
        }

        // Adjust assumed end address for this function, if a valid jump
        // target has been found that originates from the current instruction.
        if (target > last && target < start + 0x100) {
          last = target;
        }
      }
    }

    // We are done. Write-protect our code and make it executable.
    Sandbox::SysCalls sys;
    sys.mprotect(copy, 0x1000, PROT_READ|PROT_EXEC);
    return maps_->vsyscall() - copy;
  }
  #endif
  return 0;
}

void Library::patchSystemCalls() {
  if (!valid_) {
    return;
  }
  int extraLength = 0;
  char* extraSpace = NULL;
  if (isVDSO_) {
    // patchVDSO() calls patchSystemCallsInFunction() which needs vsys_offset_
    // iff processing the VDSO library. So, make sure we call
    // patchVSystemCalls() first.
    vsys_offset_ = patchVSystemCalls();
    #if defined(__i386__)
    patchVDSO(&extraSpace, &extraLength);
    return;
    #endif
  }
  SectionTable::const_iterator iter;
  if ((iter = section_table_.find(".text")) == section_table_.end()) {
    return;
  }
  const Elf_Shdr& shdr = iter->second.second;
  char* start = reinterpret_cast<char *>(shdr.sh_addr + asr_offset_);
  char* stop = start + shdr.sh_size;
  char* func = start;
  int nopcount = 0;
  bool has_syscall = false;
  for (char *ptr = start; ptr < stop; ptr++) {
    #if defined(__x86_64__)
    if ((*ptr == '\x0F' && ptr[1] == '\x05' /* SYSCALL */) ||
        (isVDSO_ && *ptr == '\xFF')) {
    #elif defined(__i386__)
    if ((*ptr   == '\xCD' && ptr[1] == '\x80' /* INT $0x80 */) ||
        (*ptr   == '\x65' && ptr[1] == '\xFF' &&
         ptr[2] == '\x15' /* CALL %gs:.. */)) {
    #else
    #error Unsupported target platform
    #endif
      ptr++;
      has_syscall = true;
      nopcount    = 0;
    } else if (*ptr == '\x90' /* NOP */) {
      nopcount++;
    } else if (!(reinterpret_cast<long>(ptr) & 0xF)) {
      if (nopcount > 2) {
        // This is very likely the beginning of a new function. Functions
        // are aligned on 16 byte boundaries and the preceding function is
        // padded out with NOPs.
        //
        // For performance reasons, we quickly scan the entire text segment
        // for potential SYSCALLs, and then patch the code in increments of
        // individual functions.
        if (has_syscall) {
          has_syscall = false;
          // Our quick scan of the function found a potential system call.
          // Do a more thorough scan, now.
          patchSystemCallsInFunction(maps_, func, ptr, &extraSpace,
                                     &extraLength);
        }
        func = ptr;
      }
      nopcount = 0;
    } else {
      nopcount = 0;
    }
  }
  if (has_syscall) {
    // Patch any remaining system calls that were in the last function before
    // the loop terminated.
    patchSystemCallsInFunction(maps_, func, stop, &extraSpace, &extraLength);
  }

  // Mark our scratch space as write-protected and executable.
  if (extraSpace) {
    Sandbox::SysCalls sys;
    sys.mprotect(extraSpace, 4096, PROT_READ|PROT_EXEC);
  }
}

bool Library::parseElf() {
  valid_ = true;

  // Verify ELF header
  Elf_Shdr str_shdr;
  if (!getOriginal(0, &ehdr_) ||
      ehdr_.e_ehsize < sizeof(Elf_Ehdr) ||
      ehdr_.e_phentsize < sizeof(Elf_Phdr) ||
      ehdr_.e_shentsize < sizeof(Elf_Shdr) ||
      !getOriginal(ehdr_.e_shoff + ehdr_.e_shstrndx * ehdr_.e_shentsize,
                   &str_shdr)) {
    // Not all memory mappings are necessarily ELF files. Skip memory
    // mappings that we cannot identify.
 error:
    valid_ = false;
    return false;
  }

  // Parse section table and find all sections in this ELF file
  for (int i = 0; i < ehdr_.e_shnum; i++) {
    Elf_Shdr shdr;
    if (!getOriginal(ehdr_.e_shoff + i*ehdr_.e_shentsize, &shdr)) {
      continue;
    }
    section_table_.insert(
       std::make_pair(getOriginal(str_shdr.sh_offset + shdr.sh_name),
                      std::make_pair(i, shdr)));
  }

  // Compute the offset of entries in the .text segment
  const Elf_Shdr* text = getSection(".text");
  if (text == NULL) {
    // On x86-32, the VDSO is unusual in as much as it does not have a single
    // ".text" section. Instead, it has one section per function. Each
    // section name starts with ".text". We just need to pick an arbitrary
    // one in order to find the asr_offset_ -- which would typically be zero
    // for the VDSO.
    for (SectionTable::const_iterator iter = section_table_.begin();
         iter != section_table_.end(); ++iter) {
      if (!strncmp(iter->first.c_str(), ".text", 5)) {
        text = &iter->second.second;
        break;
      }
    }
  }

  // Now that we know where the .text segment is located, we can compute the
  // asr_offset_.
  if (text) {
    RangeMap::const_iterator iter =
        memory_ranges_.lower_bound(text->sh_offset);
    if (iter != memory_ranges_.end()) {
      asr_offset_ = reinterpret_cast<char *>(iter->second.start) -
          (text->sh_addr - (text->sh_offset - iter->first));
    } else {
      goto error;
    }
  } else {
    goto error;
  }

  return !isVDSO_ || parseSymbols();
}

bool Library::parseSymbols() {
  if (!valid_) {
    return false;
  }

  Elf_Shdr str_shdr;
  getOriginal(ehdr_.e_shoff + ehdr_.e_shstrndx * ehdr_.e_shentsize, &str_shdr);

  // Find PLT and symbol tables
  const Elf_Shdr* plt = getSection(ELF_REL_PLT);
  const Elf_Shdr* symtab = getSection(".dynsym");
  Elf_Shdr strtab = { 0 };
  if (symtab) {
    if (symtab->sh_link >= ehdr_.e_shnum ||
        !getOriginal(ehdr_.e_shoff + symtab->sh_link * ehdr_.e_shentsize,
                     &strtab)) {
      Debug::message("Cannot find valid symbol table\n");
      valid_ = false;
      return false;
    }
  }

  if (plt && symtab) {
    // Parse PLT table and add its entries
    for (int i = plt->sh_size/sizeof(Elf_Rel); --i >= 0; ) {
      Elf_Rel rel;
      if (!getOriginal(plt->sh_offset + i * sizeof(Elf_Rel), &rel) ||
          ELF_R_SYM(rel.r_info)*sizeof(Elf_Sym) >= symtab->sh_size) {
        Debug::message("Encountered invalid plt entry\n");
        valid_ = false;
        return false;
      }

      if (ELF_R_TYPE(rel.r_info) != ELF_JUMP_SLOT) {
        continue;
      }
      Elf_Sym sym;
      if (!getOriginal(symtab->sh_offset +
                       ELF_R_SYM(rel.r_info)*sizeof(Elf_Sym), &sym) ||
          sym.st_shndx >= ehdr_.e_shnum) {
        Debug::message("Encountered invalid symbol for plt entry\n");
        valid_ = false;
        return false;
      }
      string name = getOriginal(strtab.sh_offset + sym.st_name);
      if (name.empty()) {
        continue;
      }
      plt_entries_.insert(std::make_pair(name, rel.r_offset));
    }
  }

  if (symtab) {
    // Parse symbol table and add its entries
    for (Elf_Addr addr = 0; addr < symtab->sh_size; addr += sizeof(Elf_Sym)) {
      Elf_Sym sym;
      if (!getOriginal(symtab->sh_offset + addr, &sym) ||
          (sym.st_shndx >= ehdr_.e_shnum &&
           sym.st_shndx < SHN_LORESERVE)) {
        Debug::message("Encountered invalid symbol\n");
        valid_ = false;
        return false;
      }
      string name = getOriginal(strtab.sh_offset + sym.st_name);
      if (name.empty()) {
        continue;
      }
      symbols_.insert(std::make_pair(name, sym));
    }
  }

  SymbolTable::const_iterator iter = symbols_.find("__kernel_vsyscall");
  if (iter != symbols_.end() && iter->second.st_value) {
    __kernel_vsyscall = asr_offset_ + iter->second.st_value;
  }
  iter = symbols_.find("__kernel_sigreturn");
  if (iter != symbols_.end() && iter->second.st_value) {
    __kernel_sigreturn = asr_offset_ + iter->second.st_value;
  }
  iter = symbols_.find("__kernel_rt_sigreturn");
  if (iter != symbols_.end() && iter->second.st_value) {
    __kernel_rt_sigreturn = asr_offset_ + iter->second.st_value;
  }

  return true;
}

} // namespace
