/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <io.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/win/lock_impl.h"

static const char kMutexCreateFailed[] =
    "Unrecoverable failure on CreateMutex.\n";
static const char kMutexLockFailed[] =
    "Unrecoverable failure locking a Mutex (ret:%lu) (GetLastError:%lu).\n";

NaCl::LockImpl::LockImpl() {
  mutex_ = CreateMutex(NULL, FALSE, NULL);
  abandoned_ = false;
  /*
   * Since this is probably resource exhaustion, and/or this could be the
   * log's lock, then logging is risky so we write directly to stderr.
   */
  if (NULL == mutex_) {
    _write(2, kMutexCreateFailed, sizeof kMutexCreateFailed - 1);
    NaClAbort();
  }
}

NaCl::LockImpl::~LockImpl() {
  CloseHandle(mutex_);
}

bool NaCl::LockImpl::Try() {
  DWORD dwWaitResult;

  /* Mutex previously abandoned, so always fail. */
  if (abandoned_) return false;

  dwWaitResult = WaitForSingleObject(mutex_, 0);
  switch (dwWaitResult) {
    case WAIT_OBJECT_0:
      return true;
    case WAIT_TIMEOUT:
      return false;
    case WAIT_ABANDONED:
      /*
       * This thread now owns the lock, but we mark it as abandoned
       * to force other accesses to fail or block, and pretend to fail.
       */
      abandoned_ = true;
      return false;
  }

  /*
   * It is possible to fail on Windows, so we make all other cases return
   * false to ensure the lock is never successful in these cases.
   */
  return false;
}

void NaCl::LockImpl::Lock() {
  DWORD dwWaitResult;

  /*
   * We check if abandoned because we may unknowningly own the
   * lock due to an abandoned try.  On Windows the subsequent
   * lock would actually succeed since Windows mutexs are
   * recursive.
   */
  if (abandoned_) Sleep(INFINITE);

  dwWaitResult = WaitForSingleObject(mutex_, INFINITE);
  switch (dwWaitResult) {
    /* Wait was successful, return */
    case WAIT_OBJECT_0:
      return;

    case WAIT_ABANDONED:
      /*
       * This thread now owns the lock, but we pretend to block
       * and mark it as abandoned to force other access to block.
       */
      abandoned_ = true;
      Sleep(INFINITE);
  }

  /*
   * It is possible to fail on Windows, so we need to abort to prevent a
   * security issue.  Since this could be the log's lock, and/or the system
   * is seriously unstable, we write directly.
   * GetLastError returns a 32b DWORD and dwWaitResult is one too.
   * -2147483648 is the widest value that can be printed (so add 11 chars for
   * each.
   */
  char buf[sizeof kMutexLockFailed + 11 * 2];
  sprintf_s(buf, sizeof(buf), kMutexLockFailed,
            dwWaitResult, GetLastError());
  _write(2, buf, lstrlenA(buf));
  NaClAbort();
}

void NaCl::LockImpl::Unlock() {
  /*
   * We do not check the return code, since the failure case is unlocking
   * an unlocked mutex which has an undefined bahavoir in POSIX.
   */
  ReleaseMutex(mutex_);
}
