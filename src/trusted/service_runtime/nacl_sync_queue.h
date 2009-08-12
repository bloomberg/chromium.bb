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
