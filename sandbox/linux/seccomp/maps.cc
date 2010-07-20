// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <linux/unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "library.h"
#include "maps.h"
#include "sandbox_impl.h"

namespace playground {

Maps::Maps(int proc_self_maps) :
    proc_self_maps_(proc_self_maps),
    begin_iter_(this, true, false),
    end_iter_(this, false, true),
    vsyscall_(0) {
  Sandbox::SysCalls sys;
  if (proc_self_maps_ >= 0 &&
      !sys.lseek(proc_self_maps_, 0, SEEK_SET)) {
    char buf[256] = { 0 };
    int len = 0, rc = 1;
    bool long_line = false;
    do {
      if (rc > 0) {
        rc = Sandbox::read(sys, proc_self_maps_, buf + len,
                           sizeof(buf) - len - 1);
        if (rc > 0) {
          len += rc;
        }
      }
      char *ptr = buf;
      if (!long_line) {
        long_line = true;
        unsigned long start = strtoul(ptr, &ptr, 16);
        unsigned long stop = strtoul(ptr + 1, &ptr, 16);
        while (*ptr == ' ' || *ptr == '\t') ++ptr;
        char *perm_ptr = ptr;
        while (*ptr && *ptr != ' ' && *ptr != '\t') ++ptr;
        string perm(perm_ptr, ptr - perm_ptr);
        unsigned long offset = strtoul(ptr, &ptr, 16);
        while (*ptr == ' ' || *ptr == '\t') ++ptr;
        char *id_ptr = ptr;
        while (*ptr && *ptr != ' ' && *ptr != '\t') ++ptr;
        while (*ptr == ' ' || *ptr == '\t') ++ptr;
        while (*ptr && *ptr != ' ' && *ptr != '\t') ++ptr;
        string id(id_ptr, ptr - id_ptr);
        while (*ptr == ' ' || *ptr == '\t') ++ptr;
        char *library_ptr = ptr;
        while (*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '\n') ++ptr;
        string library(library_ptr, ptr - library_ptr);
        bool isVDSO = false;
        if (library == "[vdso]") {
          // /proc/self/maps has a misleading file offset in the [vdso] entry.
          // Override it with a sane value.
          offset = 0;
          isVDSO = true;
        } else if (library == "[vsyscall]") {
          vsyscall_ = reinterpret_cast<char *>(start);
        } else if (library.empty() || library[0] == '[') {
          goto skip_entry;
        }
        int prot = 0;
        if (perm.find('r') != string::npos) {
          prot |= PROT_READ;
        }
        if (perm.find('w') != string::npos) {
          prot |= PROT_WRITE;
        }
        if (perm.find('x') != string::npos) {
          prot |= PROT_EXEC;
        }
        if ((prot & (PROT_EXEC | PROT_READ)) == 0) {
          goto skip_entry;
        }
        Library* lib = &libs_[id + ' ' + library];
        lib->setLibraryInfo(this);
        lib->addMemoryRange(reinterpret_cast<void *>(start),
                            reinterpret_cast<void *>(stop),
                            Elf_Addr(offset),
                            prot, isVDSO);
      }
   skip_entry:
      for (;;) {
        if (!*ptr || *ptr++ == '\n') {
          long_line = false;
          memmove(buf, ptr, len - (ptr - buf));
          memset(buf + len - (ptr - buf), 0, ptr - buf);
          len -= (ptr - buf);
          break;
        }
      }
    } while (len || long_line);
  }
}

Maps::Iterator::Iterator(Maps* maps, bool at_beginning, bool at_end)
    : maps_(maps),
      at_beginning_(at_beginning),
      at_end_(at_end) {
}

Maps::LibraryMap::iterator& Maps::Iterator::getIterator() const {
  if (at_beginning_) {
    iter_ = maps_->libs_.begin();
  } else if (at_end_) {
    iter_ = maps_->libs_.end();
  }
  return iter_;
}

Maps::Iterator Maps::Iterator::begin() {
  return maps_->begin_iter_;
}

Maps::Iterator Maps::Iterator::end() {
  return maps_->end_iter_;
}

Maps::Iterator& Maps::Iterator::operator++() {
  getIterator().operator++();
  at_beginning_ = false;
  return *this;
}

Maps::Iterator Maps::Iterator::operator++(int i) {
  getIterator().operator++(i);
  at_beginning_ = false;
  return *this;
}

Library* Maps::Iterator::operator*() const {
  return &getIterator().operator*().second;
}

bool Maps::Iterator::operator==(const Maps::Iterator& iter) const {
  return getIterator().operator==(iter.getIterator());
}

bool Maps::Iterator::operator!=(const Maps::Iterator& iter) const {
  return !operator==(iter);
}

Maps::string Maps::Iterator::name() const {
  return getIterator()->first;
}

// Test whether a line ends with "[stack]"; used for identifying the
// stack entry of /proc/self/maps.
static bool isStackLine(char* buf, char* end) {
  char* ptr = buf;
  for ( ; *ptr != '\n' && ptr < end; ++ptr)
    ;
  if (ptr < end && ptr - 7 > buf) {
    return (memcmp(ptr - 7, "[stack]", 7) == 0);
  }
  return false;
}

char* Maps::allocNearAddr(char* addr_target, size_t size, int prot) const {
  // We try to allocate memory within 1.5GB of a target address. This means,
  // we will be able to perform relative 32bit jumps from the target address.
  const unsigned long kMaxDistance = 1536 << 20;
  // In most of the code below, we just care about the numeric value of
  // the address.
  const long addr = reinterpret_cast<long>(addr_target);
  size = (size + 4095) & ~4095;
  Sandbox::SysCalls sys;
  if (sys.lseek(proc_self_maps_, 0, SEEK_SET)) {
    return NULL;
  }

  // Iterate through lines of /proc/self/maps to consider each mapped
  // region one at a time, looking for a gap between regions to allocate.
  char buf[256] = { 0 };
  int len = 0, rc = 1;
  bool long_line = false;
  unsigned long gap_start = 0x10000;
  void* new_addr;
  do {
    if (rc > 0) {
      do {
        rc = Sandbox::read(sys, proc_self_maps_, buf + len,
                           sizeof(buf) - len - 1);
        if (rc > 0) {
          len += rc;
        }
      } while (rc > 0 && len < (int)sizeof(buf) - 1);
    }
    char *ptr = buf;
    if (!long_line) {
      long_line = true;
      // Maps lines have the form "<start address>-<end address> ... <name>".
      unsigned long gap_end = strtoul(ptr, &ptr, 16);
      unsigned long map_end = strtoul(ptr + 1, &ptr, 16);

      // gap_start to gap_end now covers the region of empty space before
      // the current line.  Now we try to see if there's a place within the
      // gap we can use.

      if (gap_end - gap_start >= size) {
        // Is the gap before our target address?
        if (addr - static_cast<long>(gap_end) >= 0) {
          if (addr - (gap_end - size) < kMaxDistance) {
            unsigned long position;
            if (isStackLine(ptr, buf + len)) {
              // If we're adjacent to the stack, try to stay away from
              // the GROWS_DOWN region.  Pick the farthest away region that
              // is still within the gap.

              if (static_cast<unsigned long>(addr) < kMaxDistance ||  // Underflow protection.
                  static_cast<unsigned long>(addr) - kMaxDistance < gap_start) {
                position = gap_start;
              } else {
                position = (addr - kMaxDistance) & ~4095;
                if (position < gap_start) {
                  position = gap_start;
                }
              }
            } else {
              // Otherwise, take the end of the region.
              position = gap_end - size;
            }
            new_addr = reinterpret_cast<char *>(sys.MMAP
                           (reinterpret_cast<void *>(position), size, prot,
                            MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0));
            if (new_addr != MAP_FAILED) {
              goto done;
            }
          }
        } else if (gap_start + size - addr < kMaxDistance) {
          // Gap is after the address.  Above checks that we can wrap around
          // through 0 to a space we'd use.
          new_addr = reinterpret_cast<char *>(sys.MMAP
                         (reinterpret_cast<void *>(gap_start), size, prot,
                          MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1 ,0));
          if (new_addr != MAP_FAILED) {
            goto done;
          }
        }
      }
      gap_start = map_end;
    }
    for (;;) {
      if (!*ptr || *ptr++ == '\n') {
        long_line = false;
        memmove(buf, ptr, len - (ptr - buf));
        memset(buf + len - (ptr - buf), 0, ptr - buf);
        len -= (ptr - buf);
        break;
      }
    }
  } while (len || long_line);
  new_addr = NULL;
done:
  return reinterpret_cast<char*>(new_addr);
}

} // namespace
