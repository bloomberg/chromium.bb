// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ---
// On some platforms abort() is implemented in a way that Chrome's crash
// reporter treats it as a normal exit. See issue:
// http://code.google.com/p/chromium/issues/detail?id=118665
// So we replace abort with a
// segmentation fault, that crash reporter can always detect.

#ifndef BASE_ABORT_H_
#define BASE_ABORT_H_

#if defined(TCMALLOC_USE_SYSTEM_ABORT)
#include <stdlib.h>

namespace tcmalloc {
inline void Abort() {
  abort();
}
} // namespace tcmalloc

#else
namespace tcmalloc {
inline void Abort() {
  // Make a segmentation fault to force abort.
  *reinterpret_cast<int*>(NULL) = 0x2001;
}
} // namespace tcmalloc

#endif

#endif  // BASE_ABORT_H_
