/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/mmap_test_check.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "gtest/gtest.h"

#define MAX_INPUT_LINE_LENGTH   4096

void CheckMapping(uintptr_t addr, size_t size, int protect, int map_type) {
  const char *maps_path = "/proc/self/maps";
  FILE *maps_file;
  char buf[MAX_INPUT_LINE_LENGTH];
  uint64_t start;
  uint64_t end;
  int64_t inode;
  char flags[4];
  int prot = 0;
  int type = 0;
  int num;

  // Open /proc/self/maps to read process memory mapping
  maps_file = fopen(maps_path, "r");
  ASSERT_TRUE(maps_file);

  for (;;) {
    // Read each line from /proc/self/maps file
    if (!fgets(buf, sizeof(buf), maps_file)) {
      break;
    }
    // Parse the line
    if (3 < sscanf(buf, "%"SCNx64"-%"SCNx64" %4s %*d %*x:%*x %"SCNd64" %n",
                   &start, &end, flags, &inode, &num)) {
      if (start == addr) {
        if (flags[0] == 'r')
          prot |= PROT_READ;
        if (flags[1] == 'w')
          prot |= PROT_WRITE;
        if (flags[2] == 'x')
          prot |= PROT_EXEC;

        if (flags[3] == 'p')
          type |= MAP_PRIVATE;
        else if (flags[3] == 's')
          type |= MAP_SHARED;

        break;
      }
    }
  }

  // This will fail if we did not find the mapping
  ASSERT_EQ(start, addr);
  ASSERT_EQ(end - start, size);
  ASSERT_EQ(prot, protect);
  ASSERT_EQ(type, map_type);

  ASSERT_EQ(fclose(maps_file), 0);
}
