/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/sys_futex.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

/*
 * This is a simple futex implementation that is based on the
 * untrusted-code futex implementation from the NaCl IRT
 * (irt_futex.c), which in turn was based on futex_emulation.c from
 * nacl-glibc.
 *
 * There are some ways the performance of this implementation could be
 * improved:
 *
 *  * On Linux, use the kernel's futex() syscall with
 *    FUTEX_PRIVATE_FLAG.
 *
 *  * The current futex_wake() implementation does a linear search
 *    while holding a global lock, which could perform poorly if there
 *    are large numbers of waiting threads.
 *
 *    We could use a hash table rather than a single linked list for
 *    looking up wait addresses.  Furthermore, to reduce lock
 *    contention, we could use one lock per hash bucket rather than a
 *    single lock.
 */


static void ListAddNodeAtEnd(struct NaClListNode *new_node,
                             struct NaClListNode *head) {
  head->prev->next = new_node;
  new_node->prev = head->prev;
  new_node->next = head;
  head->prev = new_node;
}

static void ListRemoveNode(struct NaClListNode *node) {
  node->next->prev = node->prev;
  node->prev->next = node->next;
}

/*
 * Given a pointer to a NaClAppThread's futex_wait_list_node, this
 * returns a pointer to the NaClAppThread.
 */
static struct NaClAppThread *GetNaClAppThreadFromListNode(
    struct NaClListNode *node) {
  return (struct NaClAppThread *)
         ((uintptr_t) node -
          offsetof(struct NaClAppThread, futex_wait_list_node));
}

int32_t NaClSysFutexWaitAbs(struct NaClAppThread *natp, uint32_t addr,
                            uint32_t value, uint32_t abstime_ptr) {
  struct NaClApp *nap = natp->nap;
  struct nacl_abi_timespec abstime;
  uint32_t read_value;
  int32_t result;
  NaClSyncStatus sync_status;

  if (abstime_ptr != 0) {
    if (!NaClCopyInFromUser(nap, &abstime, abstime_ptr, sizeof(abstime))) {
      return -NACL_ABI_EFAULT;
    }
  }

  NaClXMutexLock(&nap->futex_wait_list_mu);

  /*
   * Note about lock ordering: NaClCopyInFromUser() can claim the
   * mutex nap->mu.  nap->mu may be claimed after
   * nap->futex_wait_list_mu but never before it.
   */
  if (!NaClCopyInFromUser(nap, &read_value, addr, sizeof(uint32_t))) {
    result = -NACL_ABI_EFAULT;
    goto cleanup;
  }
  if (read_value != value) {
    result = -NACL_ABI_EWOULDBLOCK;
    goto cleanup;
  }

  /* Add the current thread onto the futex wait list. */
  natp->futex_wait_addr = addr;
  ListAddNodeAtEnd(&natp->futex_wait_list_node, &nap->futex_wait_list_head);

  if (abstime_ptr == 0) {
    sync_status = NaClCondVarWait(
        &natp->futex_condvar, &nap->futex_wait_list_mu);
  } else {
    sync_status = NaClCondVarTimedWaitAbsolute(
        &natp->futex_condvar, &nap->futex_wait_list_mu, &abstime);
  }
  result = -NaClXlateNaClSyncStatus(sync_status);

  /*
   * In case a timeout or spurious wakeup occurs, remove this thread
   * from the wait queue.
   */
  if (natp->futex_wait_list_node.next != NULL) {
    ListRemoveNode(&natp->futex_wait_list_node);
  }
  /* Clear these fields to prevent their accidental use. */
  natp->futex_wait_list_node.next = NULL;
  natp->futex_wait_list_node.prev = NULL;
  natp->futex_wait_addr = 0;

cleanup:
  NaClXMutexUnlock(&nap->futex_wait_list_mu);
  return result;
}

int32_t NaClSysFutexWake(struct NaClAppThread *natp, uint32_t addr,
                         uint32_t nwake) {
  struct NaClApp *nap = natp->nap;
  struct NaClListNode *entry;
  uint32_t woken_count = 0;

  NaClXMutexLock(&nap->futex_wait_list_mu);

  /* We process waiting threads in FIFO order. */
  entry = nap->futex_wait_list_head.next;
  while (nwake > 0 && entry != &nap->futex_wait_list_head) {
    struct NaClListNode *next = entry->next;
    struct NaClAppThread *waiting_thread = GetNaClAppThreadFromListNode(entry);

    if (waiting_thread->futex_wait_addr == addr) {
      ListRemoveNode(entry);
      /*
       * Mark the thread as having been removed from the wait queue:
       * tell it not to try to remove itself from the queue.
       */
      entry->next = NULL;

      /* Also clear these fields to prevent their accidental use. */
      entry->prev = NULL;
      waiting_thread->futex_wait_addr = 0;

      NaClXCondVarSignal(&waiting_thread->futex_condvar);
      woken_count++;
      nwake--;
    }
    entry = next;
  }

  NaClXMutexUnlock(&nap->futex_wait_list_mu);

  return woken_count;
}
