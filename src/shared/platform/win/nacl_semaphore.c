/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NaCl semaphore implementation (Windows)
 */

#include <windows.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/win/nacl_semaphore.h"


/* Generic test for success on any status value (non-negative numbers
 * indicate success).
 */
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

/* semaphore object query structure */

typedef struct _SEMAINFO {
  unsigned int count;   /* current semaphore count */
  unsigned int limit;   /* max semaphore count */
} SEMAINFO, *PSEMAINFO;

/* only one query type for semaphores */
#define SEMAQUERYINFOCLASS  0
#define WINAPI __stdcall

/* NT Function call. */
NTSTATUS WINAPI NtQuerySemaphore(HANDLE       Handle,
                                 unsigned int InfoClass,
                                 PSEMAINFO    SemaInfo,
                                 unsigned int InfoSize,
                                 unsigned int *RetLen);



int NaClSemCtor(struct NaClSemaphore *sem, int32_t value) {
  sem->sem_handle = CreateSemaphore(NULL, value, SEM_VALUE_MAX, NULL);
  if (NULL == sem->sem_handle) {
    return 0;
  }
  sem->interrupt_event = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (NULL == sem->interrupt_event) {
    CloseHandle(sem->sem_handle);
    return 0;
  }
  return 1;
}

void NaClSemDtor(struct NaClSemaphore *sem) {
  CloseHandle(sem->sem_handle);
  CloseHandle(sem->interrupt_event);
}

NaClSyncStatus NaClSemWait(struct NaClSemaphore *sem) {
  DWORD rv;
  NaClSyncStatus status;
  HANDLE handles[2];
  handles[0] = sem->sem_handle;
  handles[1] = sem->interrupt_event;

  rv = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
  switch (rv) {
    case WAIT_OBJECT_0:
      status = NACL_SYNC_OK;
      break;
    case WAIT_OBJECT_0 + 1:
      status = NACL_SYNC_SEM_INTERRUPTED;
      break;
    default:
      status = NACL_SYNC_INTERNAL_ERROR;
  }
  return status;
}

NaClSyncStatus NaClSemTryWait(struct NaClSemaphore *sem) {
  DWORD rv;
  rv = WaitForSingleObject(sem->sem_handle, 0);
  return (rv != WAIT_OBJECT_0);
}

NaClSyncStatus NaClSemPost(struct NaClSemaphore *sem) {
  if (ReleaseSemaphore(sem->sem_handle, 1, NULL)) {
    return NACL_SYNC_OK;
  }
  if (ERROR_TOO_MANY_POSTS == GetLastError()) {
    return NACL_SYNC_SEM_RANGE_ERROR;
  }
  return NACL_SYNC_INTERNAL_ERROR;
}

int32_t NaClSemGetValue(struct NaClSemaphore *sem) {
  /*
   * TODO(greogyrd): please uncomment these decls when they're needed / when
   * the code below is re-enabled.  These were commented-out to eliminate
   * windows compiler warnings.
  SEMAINFO    sem_info;
  UINT        ret_length;
  NTSTATUS    status;
   */
  int32_t     count = -1;
/* TODO(gregoryd): cannot use NtQuerySemaphore without linking to ntdll.lib
  status = NtQuerySemaphore(
    sem->sem_handle,
    SEMAQUERYINFOCLASS,
    &sem_info,
    sizeof sem_info,
    &ret_length
    );

  if (!NT_SUCCESS(status)) {
    count = -1;
  } else {
    count = sem_info.count;
  }
*/
  return count;
}

void NaClSemIntr(struct NaClSemaphore *sem) {
  SetEvent(sem->interrupt_event);
}

void NaClSemReset(struct NaClSemaphore *sem) {
  ResetEvent(sem->interrupt_event);
}
