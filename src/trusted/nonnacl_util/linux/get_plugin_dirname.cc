/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

namespace nacl {

// This code tries to determine the plugin directory which contains
// the sel_ldr and possibly other libraries
const char* SelLdrLauncher::GetPluginDirname() {
  Dl_info     info;
  char*       pathname = NULL;
  // NOTE: this static makes the functions NOT thread safe!
  static char bin_dir[2 * PATH_MAX + 1];
  // C++ really does not like to convert function pointer to regular pointers.
  // This is apparently the only way to do it without compiler warnings
  void* sym_addr = reinterpret_cast<void*>(
    reinterpret_cast<uintptr_t>(GetPluginDirname));

  // First, try looking a symbol up in the dynamic loader's records.
  if (0 != dladdr(sym_addr, &info)) {
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
