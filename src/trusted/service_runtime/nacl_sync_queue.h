/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime workqueue.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SYNC_QUEUE_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SYNC_QUEUE_H_

#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/trusted/service_runtime/nacl_closure.h"

/**
 * This module implements a thread-safe queue.
 *
 * It internally uses nacl_sync_checked, so any failure returns from
 * synchronization objects will result in a fatal error.
 */

struct NaClSyncQueueItem;  /* opaque */

struct NaClSyncQueue {
  struct NaClMutex          mu;
  struct NaClCondVar        cv;  /* wake on not empty */
  struct NaClSyncQueueItem  *head;
  struct NaClSyncQueueItem  **insert_pt;
  int                       quit;
};

int NaClSyncQueueEmpty(struct NaClSyncQueue *nsqp);

/*
 * Placement new style constructor for a NaClSyncQueue object.
 */
int NaClSyncQueueCtor(struct NaClSyncQueue *nsqp) NACL_WUR;

void NaClSyncQueueDtor(struct NaClSyncQueue *nsqp);

void NaClSyncQueueInsert(struct NaClSyncQueue *nsqp,
                         void                 *item);

/*
 * Tell blocked threads to unblock.
 */
void NaClSyncQueueQuit(struct NaClSyncQueue *nsqp);

/*
 * Block waiting for a work queue item, or until NaClSyncQueueQuit is
 * invoked on the work queue, in which case NULL is returned.  Note
 * that the NaClSyncQueue may not be empty.  Before the dtor is
 * invoked on the queue, all enqueued work items must be freed.
 */
void *NaClSyncQueueDequeue(struct NaClSyncQueue *nsqp);

#endif
