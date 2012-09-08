// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_INCLUDE_MEMORY_H_
#define NATIVE_CLIENT_SRC_INCLUDE_MEMORY_H_

// Platform-specific includes for tr1::memory.  The file is in a different
// location on Windows than on *nix platforms.

#ifdef __cplusplus

#if NACL_LINUX || NACL_OSX || __native_client__
#include <tr1/memory>
#elif NACL_WINDOWS
#include <memory>
#else
#error "tr1::memory header file not available for this platform."
#endif

#endif  // __cplusplus

#endif  // NATIVE_CLIENT_SRC_INCLUDE_MEMORY_H_

