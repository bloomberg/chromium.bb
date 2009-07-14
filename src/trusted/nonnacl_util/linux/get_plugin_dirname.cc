/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/* TODO: add comment why this is needed */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <libgen.h>
#include <linux/limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

namespace nacl {


const char* SelLdrLauncher::GetPluginDirname() {
  Dl_info     info;
  char*       pathname = NULL;
  static char bin_dir[2 * PATH_MAX + 1];
  // C++ really does not like to convert function pointer to regular pointers.
  // This is apparently the only way to do it without compiler warnings
  void* sym_addr = reinterpret_cast<void*>(
    reinterpret_cast<uintptr_t>(GetPluginDirname));

  // First, try looking a symbol up in the dynamic loader's records.
  if (0 == dladdr(sym_addr, &info)) {
    printf("dladdr failed\n");
  } else {
    strncpy(bin_dir, info.dli_fname, sizeof(bin_dir));
    pathname = bin_dir;
  }
  // What follows is probably major overkill if the dladdr scheme works.
  if (NULL == pathname) {
    // Identify the path the plugin was loaded from.  For Linux we look through
    // the address ranges in /proc/self/maps for the address of a static
    // variable. If that fails, look in /proc/self/exe.
    FILE* fp;

    // Open the file, returning NULL for failure.
    if (NULL == (fp = fopen("/proc/self/maps", "r"))) {
      return NULL;
    }
    // Read line by line.
    while (!feof(fp)) {
      static int dummy_for_address;
      const uintptr_t kAddressInSo = (uintptr_t) &dummy_for_address;
      uintptr_t low_addr;
      uintptr_t high_addr;
      char perm[5];
      uintptr_t offset;
      unsigned dev_maj;
      unsigned dev_min;
      unsigned inode;

      if (7 != fscanf(fp, "%"PRIxPTR"-%"PRIxPTR" %s %"PRIxPTR" %x:%x %x",
                      &low_addr, &high_addr, perm, &offset, &dev_maj,
                      &dev_min, &inode)) {
        return NULL;
      }
      if (NULL == fgets(bin_dir, sizeof(bin_dir), fp)) {
        return NULL;
      }
      // If the address is in the range, break out of the loop.
      if (kAddressInSo >= low_addr && kAddressInSo < high_addr) {
        pathname = strchr(bin_dir, '/');
        break;
      }
    }
    // Close /proc/self/maps.
    fclose(fp);
  }
  // If we didn't find a matching pathname, try the path of the current
  // executable.
  if (NULL == pathname) {
    int bin_dir_size = readlink("/proc/self/exe", bin_dir, PATH_MAX);
    if (bin_dir_size < 0 || bin_dir_size > PATH_MAX) {
      return NULL;
    }
    bin_dir[bin_dir_size] = '\0';
    pathname = bin_dir;
  }
  // Return just the directory path.
  return dirname(pathname);
}


}  // namespace nacl
