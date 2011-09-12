// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

// Functions for dynamically loading the trusted plugin when running unit
// tests.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_DYLIB_UNITTEST_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_DYLIB_UNITTEST_H_

#if NACL_WINDOWS
#include <windows.h>
typedef HINSTANCE DylibHandle;
typedef void (*SymbolHandle)();
#else
#include <dlfcn.h>
#include <inttypes.h>
typedef void* DylibHandle;
// uintptr_t is used here because ISO C++ won't allow casting from a void*
// (the return type of dlsym()) to a pointer-to-function.  Instead,
// GetSymbolHandle() returns a uintptr_t which can then be cast into a pointer-
// to-function.  This depends on uintptr_t being the same size (or larger than)
// void*.
typedef uintptr_t SymbolHandle;
#endif

// Load the dynamic library at |lib_path|.  Returns NULL on error.
DylibHandle DylibOpen(const char* lib_path);

// Close the dynamic library and free all the system resources associated with
// it.  Returns |true| on success.
bool DylibClose(DylibHandle dl_handle);

// Return a handle to the symbol named |name| in the library represented by
// |dl_handle|.  Returns NULL ff the symbol does not exist, or some other error
// occurs.
SymbolHandle GetSymbolHandle(DylibHandle dl_handle, const char* name);

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_DYLIB_UNITTEST_H_
