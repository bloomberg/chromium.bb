// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_GLOBALS_H
#define CRAZY_LINKER_GLOBALS_H

#include <pthread.h>

#include "crazy_linker_library_list.h"
#include "crazy_linker_pointer_set.h"
#include "crazy_linker_rdebug.h"
#include "crazy_linker_search_path_list.h"

// All crazy linker globals are declared in this header.

namespace crazy {

class Globals {
 public:
  Globals();
  ~Globals();

  void Lock() { pthread_mutex_lock(&lock_); }

  void Unlock() { pthread_mutex_unlock(&lock_); }

  static Globals* Get();

  static LibraryList* GetLibraries() { return &Get()->libraries_; }

  static SearchPathList* GetSearchPaths() { return &Get()->search_paths_; }

  static RDebug* GetRDebug() { return &Get()->rdebug_; }

  static int* GetSDKBuildVersion() { return &sdk_build_version_; }

  // Set of valid handles returned by the dlopen() wrapper. This is
  // required to deal with rare cases where the wrapper is passed
  // a handle that was opened with the system linker by mistake.
  static PointerSet* GetValidHandles() { return &Get()->valid_handles_; }

 private:
  pthread_mutex_t lock_;
  LibraryList libraries_;
  SearchPathList search_paths_;
  RDebug rdebug_;
  PointerSet valid_handles_;
  static int sdk_build_version_;
};

// Helper class to access the globals with scoped locking.
class ScopedGlobalLock {
 public:
  ScopedGlobalLock() { Globals::Get()->Lock(); }

  ~ScopedGlobalLock() { Globals::Get()->Unlock(); }
};

}  // namespace crazy

#endif  // CRAZY_LINKER_GLOBALS_H
