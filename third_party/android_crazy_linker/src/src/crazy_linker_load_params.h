// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_LOAD_PARAMS_H
#define CRAZY_LINKER_LOAD_PARAMS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "crazy_linker_util.h"

namespace crazy {

// A structure used to hold parameters related to loading an ELF library
// into the current process' address space.
//
// |library_path| is either the full library path.
// |library_offset| is the page-aligned offset where the library starts in
// its input file (typically > 0 when reading from Android APKs).
// |wanted_address| is either 0, or the address where the library should
// be loaded.
struct LoadParams {
  String library_path;
  off_t library_offset = 0;
  uintptr_t wanted_address = 0;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_LOAD_PARAMS_H
