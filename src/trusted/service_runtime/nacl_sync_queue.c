/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime synchronized queue.
 */

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

#include "native_client/src/trusted/service_runtime/nacl_sync_queue.h"

struct NaClSyncQueueItem {
  struct NaClSyncQueueItem  *next;
  void                      *item;  /* arbitrary object */
};


int NaClSyncQueueEmptyMu(struct NaClSyncQueue *nsqp) {
  return NULL == nsqp->head;
}


int NaClSyncQueueEmpty(struct NaClSyncQueue *nsqp) {
  int empty;

  NaClXMutexLock(&nsqp->mu);
  empty = NaClSyncQueueEmptyMu(nsqp);
  NaClXMutexUnlock(&nsqp->mu);
  return empty;
}


int NaClSyncQueueCtor(struct NaClSyncQueue *nsqp) {
  if (!NaClMutexCtor(&nsqp->mu)) {
    return 0;
  }
  if (!NaClCondVarCtor(&nsqp->cv)) {
    NaClMutexDtor(&nsqp->mu);
    return 0;
  }
  nsqp->head = NULL;
  nsqp->insert_pt = &nsqp->head;
  nsqp->quit = 0;

  /* post-condition: NaClSyncQueueEmpty(nsqp) != 0 */
  return 1;
}


void NaClSyncQueueDtor(struct NaClSyncQueue *nsqp) {
  /*
   * NB: sanity check is not complete, since after dropping the lock
   * and prior to deallocating the mutex, another (confused) thread
   * could come along and insert an object.  Since such a confused
   * thread might try to insert an object into the queue even after
   * the mutex and condvar is dtor'd, we're pretty much stuck.
   */
  CHECK(NaClSyncQueueEmpty(nsqp));
  NaClMutexDtor(&nsqp->mu);
  NaClCondVarDtor(&nsqp->cv);
}


void NaClSyncQueueInsert(struct NaClSyncQueue *nsqp, void *item) {
  struct NaClSyncQueueItem  *nsqip;

  NaClLog(3, "NaClSyncQueueInsert(0x%08"NACL_PRIxPTR",0x%08"NACL_PRIxPTR")\n",
          (uintptr_t) nsqp,
          (uintptr_t) item);

  nsqip = malloc(sizeof *nsqip);
  if (NULL == nsqip) {
    NaClLog(LOG_FATAL, "Out of memory for NaClSyncQueue item\n");
  }
  nsqip->next = NULL;
  nsqip->item = item;
  NaClXMutexLock(&nsqp->mu);
  *nsqp->insert_pt = nsqip;
  nsqp->insert_pt = &nsqip->next;
  NaClXCondVarSignal(&nsqp->cv);  /* non-empty or quit*/
  NaClXMutexUnlock(&nsqp->mu);

}


void NaClSyncQueueQuit(struct NaClSyncQueue *nsqp) {
  NaClXMutexLock(&nsqp->mu);
  nsqp->quit = 1;
  NaClXCondVarSignal(&nsqp->cv);
  NaClXMutexUnlock(&nsqp->mu);
}


void *NaClSyncQueueDequeue(struct NaClSyncQueue *nsqp) {
  struct NaClSyncQueueItem  *qitem;
  void                      *item;

  NaClLog(3, "NaClSyncQueueDequeue: waiting on queue 0x%08"NACL_PRIxPTR"\n",
          (uintptr_t) nsqp);
  NaClXMutexLock(&nsqp->mu);
  while (NaClSyncQueueEmptyMu(nsqp) && !nsqp->quit) {
    NaClLog(3, "NaClSyncQueueDequeue: waiting\n");
    NaClCondVarWait(&nsqp->cv, &nsqp->mu);
  }

  if (nsqp->quit) {
    item = NULL;
  } else {
    CHECK(!NaClSyncQueueEmptyMu(nsqp));

    qitem = nsqp->head;
    nsqp->head = qitem->next;
    if (NULL == nsqp->head) {
      nsqp->insert_pt = &nsqp->head;
    }
    item = qitem->item;
    free(qitem);
  }

  NaClXMutexUnlock(&nsqp->mu);

  NaClLog(3, "NaClSyncQueueDequeue: returning item 0x%08"NACL_PRIxPTR"\n",
          (uintptr_t) item);
  return item;
}
