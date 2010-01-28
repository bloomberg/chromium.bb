/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/win/lock_impl.h"

NaCl::LockImpl::LockImpl() {
  mutex_ = CreateMutex(NULL, FALSE, NULL);
}

NaCl::LockImpl::~LockImpl() {
  CloseHandle(mutex_);
}

bool NaCl::LockImpl::Try() {
  DWORD dwWaitResult;
  dwWaitResult = WaitForSingleObject(mutex_, 0);
  switch (dwWaitResult) {
    case WAIT_OBJECT_0:
      return true;
    case WAIT_TIMEOUT:
      return false;
  }
  return false;
}

void NaCl::LockImpl::Lock() {
  WaitForSingleObject(mutex_, INFINITE);
}

void NaCl::LockImpl::Unlock() {
  ReleaseMutex(mutex_);
}
