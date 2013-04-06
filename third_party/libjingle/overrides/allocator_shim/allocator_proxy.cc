// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <new>

#include "talk/base/basictypes.h"

typedef void* (*AllocateFunction)(std::size_t);
typedef void (*DellocateFunction)(void*);

#ifndef LIBPEERCONNECTION_IMPLEMENTATION
#error "Only compile the allocator proxy with the shared_library implementation"
#endif

#ifdef WIN32
#define ALLOC_EXPORT __declspec(dllexport)
#else
#define ALLOC_EXPORT __attribute__((visibility("default")))
#endif

static AllocateFunction g_alloc = NULL;
static DellocateFunction g_dealloc = NULL;

// This function will be called by the client code to initialize the allocator
// routines (see allocator_stub.cc).
ALLOC_EXPORT
bool SetProxyAllocator(AllocateFunction alloc, DellocateFunction dealloc) {
  g_alloc = alloc;
  g_dealloc = dealloc;
  return true;
}

// Override the global new/delete routines and proxy them over to the allocator
// routines handed to us via SetProxyAllocator.

void* operator new(std::size_t n) throw() {
  return g_alloc(n);
}

void operator delete(void* p) throw() {
  g_dealloc(p);
}
