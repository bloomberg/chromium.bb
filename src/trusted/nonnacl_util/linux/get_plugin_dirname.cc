/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/* TODO: add comment why this is needed */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <dlfcn.h>
#include <libgen.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

using std::string;

namespace nacl {

// First, try looking a symbol up in the dynamic loader's records.
static void PathFromSymbol(char* buffer, size_t len, void* sym) {
  Dl_info info;

  if (0 != dladdr(sym, &info)) {
    strncpy(buffer, info.dli_fname, len - 1);
  } else {
    buffer[0] = '\0';
  }
}

// Identify the path the plugin was loaded from.  For Linux we look through
// the address ranges in /proc/self/maps for the address of a static
// variable. If that fails, look in /proc/self/exe.
static void PathFromProc(char* buffer, size_t len) {
  FILE* fp;
  char* pathname;
  char line[1024];

  // assume failure
  buffer[0] = '\0';
  // Open the file, returning NULL for failure.
  if (NULL == (fp = fopen("/proc/self/maps", "r"))) {
    return;
  }
  // Read line by line.
  while (!feof(fp)) {
    int dummy_for_address;
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
      return;
    }

    if (NULL == fgets(line, sizeof(line), fp)) {
      return;
    }

    // If the address is in the range, break out of the loop.
    if (kAddressInSo >= low_addr && kAddressInSo < high_addr) {
      pathname = strchr(line, '/');
      strncpy(buffer, line, len - 1);
      break;
    }
  }
  // Close /proc/self/maps.
  fclose(fp);
}


// Path of the current executable
static void PathFromExe(char* buffer, size_t len) {
  int bin_dir_size = readlink("/proc/self/exe", buffer, len - 1);
  if (bin_dir_size < 0 || (size_t) bin_dir_size >= len) {
    buffer[0] = '\0';
  } else {
    buffer[len - 1] = '\0';
  }
}



// This code tries to determine the plugin directory which contains
// the sel_ldr and possibly other libraries
void SelLdrLauncher::GetPluginDirectory(char* buffer, size_t len) {
  // C++ really does not like to convert function pointer to regular pointers.
  // This is apparently the only way to do it without compiler warnings
  void* sym_addr = reinterpret_cast<void*>(
    reinterpret_cast<uintptr_t>(GetPluginDirectory));

  // We try 3 heuristics before giving up
  PathFromSymbol(buffer, len, sym_addr);
  if (0 == strlen(buffer)) {
    PathFromProc(buffer, len);
  }

  if (0 == strlen(buffer)) {
    PathFromExe(buffer, len);
  }

  if (0 == strlen(buffer)) {
    // TOOD(robertm): maybe emit error message - but these file seem to not
    //                use NaClLog
    return;
  }

  char* path_end = strrchr(buffer, '/');
  if (NULL != path_end) {
    *path_end = '\0';
  }
}


}  // namespace nacl
