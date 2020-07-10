// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/base/debug/alias.h"

#include "build/build_config.h"

namespace pdfium {
namespace base {
namespace debug {

#if defined(COMPILER_MSVC)
#pragma optimize("", off)
#elif defined(__clang__)
#pragma clang optimize off
#endif

void Alias(const void* var) {
}

#if defined(COMPILER_MSVC)
#pragma optimize("", on)
#elif defined(__clang__)
#pragma clang optimize on
#endif

}  // namespace debug
}  // namespace base
}  // namespace pdfium
