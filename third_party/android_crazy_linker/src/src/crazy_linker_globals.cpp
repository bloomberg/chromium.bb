// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_globals.h"

#include <new>

#include <pthread.h>

namespace crazy {

namespace {

// Implement lazy-initialized static variable without a C++ constructor.
// Note that this is leaky, i.e. the instance is never destroyed, but
// this was also the case with the previous heap-based implementation.
pthread_once_t s_once = PTHREAD_ONCE_INIT;

union Storage {
  char dummy;
  Globals globals;

  Storage() {}
  ~Storage() {}
};

Storage s_storage;

void InitGlobals() {
  new (&s_storage.globals) Globals();
}

}  // namespace

Globals::Globals() {
  // TODO(digit): Remove the need for a recursive mutex (which is often the
  // symptom of something wrong in threaded code). This needs refactoring
  // the deferred task management though.
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&lock_, &attr);
  search_paths_.ResetFromEnv("LD_LIBRARY_PATH");
}

Globals::~Globals() {
  pthread_mutex_destroy(&lock_);
}

void Globals::Lock() {
  pthread_mutex_lock(&lock_);
}

void Globals::Unlock() {
  pthread_mutex_unlock(&lock_);
}

// static
Globals* Globals::Get() {
  pthread_once(&s_once, InitGlobals);
  return &s_storage.globals;
}

// static
int Globals::sdk_build_version = 0;

}  // namespace crazy
