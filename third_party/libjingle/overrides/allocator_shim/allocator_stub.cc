// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <new>

#include "talk/base/basictypes.h"


typedef void* (*AllocateFunction)(std::size_t);
typedef void (*DellocateFunction)(void*);

#ifdef LIBPEERCONNECTION_IMPLEMENTATION
#error "Only compile the allocator stub with the client implementation"
#endif

#ifdef WIN32
#define ALLOC_IMPORT __declspec(dllimport)
#else
#define ALLOC_IMPORT
#endif

// The stub implementations that forward new / delete calls to the allocator
// in the current binary (usually tcmalloc).

static void* Allocate(std::size_t n) {
  return operator new(n);
}

static void Dellocate(void* p) {
  return operator delete(p);
}

ALLOC_IMPORT
bool SetProxyAllocator(AllocateFunction a, DellocateFunction d);

// Initialize the proxy by supplying it with a pointer to our
// allocator/deallocator routines.
// This unfortunately costs a single static initializer.  The alternatives
// are:
// * A circular reference from libjingle back to chrome (or any other binary)
// * Reworking libjingle interfaces to not use stl types in classinterfaces.
// 	 This is currently not feasible.
// * Hack internal peerconnectionfactory implementation details such as
//   PeerConnectionFactory::Initialize to initialize the proxy.  While possible,
//   this would be very prone to regressions due to any changes in the libjingle
//   library.
// * Make modifications to libjingle to support initializing the proxy when
//   needed.  I'm (tommi) currently working on this.
static const bool proxy_allocator_initialized =
    SetProxyAllocator(&Allocate, &Dellocate);

// Include the implementation of peerconnectionfactory.cc here.
// This is done to ensure that the stub initialization code and
// peerconnectionfactory belong to the same object file.  That way the
// linker cannot discard the necessary stub initialization code that goes with
// the factory, even though there aren't any explicit dependencies.
// See libjingle.gyp for how we exclude peerconnectionfactory.cc from the
// target in this case.
#include "talk/app/webrtc/peerconnectionfactory.cc"
