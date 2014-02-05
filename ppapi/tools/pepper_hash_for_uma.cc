// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a utility executable used for generating hashes for pepper
// interfaces for inclusion in tools/metrics/histograms/histograms.xml. Every
// interface-version pair must have a corresponding entry in the enum there.
//
// The hashing logic here must match the hashing logic at
// ppapi/proxy/interface_list.cc.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/hash.h"
#include "base/macros.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <interface_name>", argv[0]);
    return 1;
  }
  uint32 data = base::Hash(argv[1], strlen(argv[1]));

  // Strip off the signed bit because UMA doesn't support negative values,
  // but takes a signed int as input.
  int hash = static_cast<int>(data & 0x7fffffff);
  printf("%s: %d\n", argv[1], hash);
  return 0;
}
