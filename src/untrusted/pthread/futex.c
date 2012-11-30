/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>

#include "native_client/src/untrusted/pthread/futex.h"
#include "native_client/src/untrusted/pthread/pthread_internal.h"

/*
 * This is a simple futex implementation that is based on
 * futex_emulation.c from nacl-glibc.
 *
 * This implementation does not provide any of the following:
 *
 *  * bitsets (FUTEX_WAIT_BITSET or FUTEX_WAKE_BITSET)
 *  * the FUTEX_REQUEUE or FUTEX_CMP_REQUEUE operations
 *  * FUTEX_OP_*
 *  * robust lists, or futexes that are shareable between processes
 *
 * We do not attempt to use a hash table to map addresses to lists of
 * waiting threads, which would require one mutex per hash bucket.  We
 * just have a single list of waiting threads.  futex_wake() does a
 * linear search while holding a global lock, which could perform
 * poorly if there are large numbers of waiting threads.
 *
 * TODO(mseaborn): In the longer term, we want to implement futexes in
 * trusted code and provide futex syscalls.
 * See https://code.google.com/p/nativeclient/issues/detail?id=1244
 */


struct list_head {
  struct list_head *next;
  struct list_head *prev;
};

struct wait_list_node {
  struct list_head list;  /* Must be the first field in the struct */
  volatile int *addr;  /* Address this thread is waiting on */
  int condvar_desc;  /* Condvar for waking the thread */
  int condvar_desc_is_initialized;
};


static int g_mutex_desc = -1;

/* This is a doubly linked list with a sentinel node. */
static struct list_head g_wait_list = { &g_wait_list, &g_wait_list };

static __thread struct wait_list_node g_wait_list_node;


void __nc_futex_init(void) {
  /*
   * Initialize the global mutex that will be held while accessing
   * g_wait_list.  There is no __nc_futex_fini() because there is no
   * need to deallocate this mutex on exit.
   */
  assert(g_mutex_desc == -1);
  int rc = __nc_irt_mutex.mutex_create(&g_mutex_desc);
  assert(rc == 0);
}

void __nc_futex_thread_exit(void) {
  /*
   * Deallocate the condvar that is allocated on demand by
   * futex_wait().  We do not need to claim g_mutex_desc here because
   * the list node is not on the list of waiting threads (g_wait_list)
   * -- a thread that is exiting cannot be waiting.
   */
  struct wait_list_node *self = &g_wait_list_node;
  if (self->condvar_desc_is_initialized) {
    int rc = __nc_irt_cond.cond_destroy(self->condvar_desc);
    assert(rc == 0);
    self->condvar_desc_is_initialized = 0;
  }
}

static void list_add_node_at_end(struct list_head *new_node,
                                 struct list_head *head) {
  head->prev->next = new_node;
  new_node->prev = head->prev;
  new_node->next = head;
  head->prev = new_node;
}

static void list_delete_node(struct list_head *node) {
  node->next->prev = node->prev;
  node->prev->next = node->next;
}

/*
 * If |*addr| still contains |value|, this waits to be woken up by a
 * __nc_futex_wake(addr,...) call from another thread; otherwise, it
 * returns EWOULDBLOCK immediately.  If woken by another thread, this
 * returns 0.  If |abstime| is non-NULL and the time specified by
 * abstime passes, this returns ETIMEDOUT.
 *
 * Note that this differs from Linux's FUTEX_WAIT in that it takes an
 * absolute time value (relative to the Unix epoch) rather than a
 * relative time duration.  This is because this is implemented in
 * terms of NaCl's cond_timed_wait_abs() IRT call/syscall.
 */
int __nc_futex_wait(volatile int *addr, int value,
                    const struct timespec *abstime) {
  int retcode;

  int rc = __nc_irt_mutex.mutex_lock(g_mutex_desc);
  assert(rc == 0);

  if (*addr != value) {
    retcode = EWOULDBLOCK;
  } else {
    struct wait_list_node *self = &g_wait_list_node;

    if (!self->condvar_desc_is_initialized) {
      retcode = __nc_irt_cond.cond_create(&self->condvar_desc);
      if (retcode != 0)
        goto ret_unlock;
      self->condvar_desc_is_initialized = 1;
    }

    self->addr = addr;

    list_add_node_at_end(&self->list, &g_wait_list);

    if (abstime != NULL) {
      retcode = __nc_irt_cond.cond_timed_wait_abs(
          self->condvar_desc, g_mutex_desc, abstime);
    } else {
      retcode = __nc_irt_cond.cond_wait(
          self->condvar_desc, g_mutex_desc);
    }
    /*
     * In case a timeout or spurious wakeup occurs, remove this thread
     * from the wait queue.
     */
    if (self->list.next != NULL)
      list_delete_node(&self->list);
  }

ret_unlock:
  rc = __nc_irt_mutex.mutex_unlock(g_mutex_desc);
  assert(rc == 0);
  return retcode;
}

/*
 * This wakes up threads that are waiting on |addr| using
 * __nc_futex_wait().  |nwake| is the maximum number of threads that
 * will be woken up.  The number of threads that were woken is
 * returned in |*count|.
 */
int __nc_futex_wake(volatile int *addr, int nwake, int *count) {
  int rc = __nc_irt_mutex.mutex_lock(g_mutex_desc);
  assert(rc == 0);

  *count = 0;

  /* We process waiting threads in FIFO order. */
  struct list_head *entry = g_wait_list.next;
  while (nwake > 0 && entry != &g_wait_list) {
    struct list_head *next = entry->next;
    /*
     * The following cast assumes that 'struct wait_list_node' extends
     * 'struct list_head'.
     */
    assert(offsetof(struct wait_list_node, list) == 0);
    struct wait_list_node *waiting_thread = (struct wait_list_node *) entry;
    if (waiting_thread->addr == addr) {
      list_delete_node(entry);
      /*
       * Mark the thread as having been removed from the wait queue,
       * so that it does not try to remove itself.
       */
      entry->next = NULL;
      int rc = __nc_irt_cond.cond_signal(waiting_thread->condvar_desc);
      assert(rc == 0);
      ++*count;
      --nwake;
    }
    entry = next;
  }

  rc = __nc_irt_mutex.mutex_unlock(g_mutex_desc);
  assert(rc == 0);

  return 0;
}
