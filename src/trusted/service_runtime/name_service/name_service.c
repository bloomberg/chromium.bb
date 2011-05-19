/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_string.h"

#include "native_client/src/trusted/service_runtime/name_service/name_service.h"


#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"

#include "native_client/src/trusted/service_runtime/include/sys/nacl_name_service.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

#include "native_client/src/trusted/simple_service/nacl_simple_service.h"
#include "native_client/src/trusted/simple_service/nacl_simple_ltd_service.h"
#include "native_client/src/trusted/threading/nacl_thread_interface.h"


/*
 * Name service is a linear linked list.  We could use a hash
 * container eventually, but performance is not a goal for this simple
 * bootstrap name service.  Static entry and factory-based generation
 * are mutually exclusive; the |factory| function is used iff |entry|
 * is NULL.  Client code is expected to cache lookup results.
 */
struct NaClNameServiceEntry {
  struct NaClNameServiceEntry *next;
  char const                  *name;
  struct NaClDesc             *entry;  /* static entry, or, ... */

  NaClNameServiceFactoryFn_t  factory;
  void                        *state;
};

struct NaClSrpcHandlerDesc const kNaClNameServiceHandlers[];
/* fwd */

int NaClNameServiceCtor(struct NaClNameService *self) {
  int     retval = 0;  /* fail */

  NaClLog(4, "Entered NaClNameServiceCtor\n");
  if (!NaClSimpleLtdServiceCtor(&self->base,
                                kNaClNameServiceHandlers,
                                NACL_NAME_SERVICE_CONNECTION_MAX,
                                NaClThreadInterfaceThreadFactory,
                                (void *) NULL)) {
    NaClLog(4, "NaClSimpleLtdServiceCtor failed\n");
    goto done;
  }
  if (!NaClMutexCtor(&self->mu)) {
    NaClLog(4, "NaClMutexCtor failed\n");
    goto abort_mu;
  }
  NACL_VTBL(NaClRefCount, self) = (struct NaClRefCountVtbl *)
      &kNaClNameServiceVtbl;
  /* success return path */
  self->head = (struct NaClNameServiceEntry *) NULL;
  retval = 1;
  goto done;

  /* cleanup unwind */
 abort_mu:  /* mutex ctor failed */
  (*NACL_VTBL(NaClRefCount, self)->Dtor)((struct NaClRefCount *) self);
 done:
  return retval;
}

void NaClNameServiceDtor(struct NaClRefCount *vself) {
  struct NaClNameService      *self = (struct NaClNameService *) vself;

  struct NaClNameServiceEntry *p;
  struct NaClNameServiceEntry *next;

  for (p = self->head; NULL != p; p = next) {
    next = p->next;
    if (NULL != p->entry) {
      NaClRefCountUnref((struct NaClRefCount *) p->entry);
    } else {
      /*
       * Tell the factory fn that this particular use can be GC'd.
       */
      (void) (*p->factory)(p->state, p->name, (struct NaClDesc **) NULL);
    }
    free(p);
  }
  NaClMutexDtor(&self->mu);
  NACL_VTBL(NaClRefCount, self) = (struct NaClRefCountVtbl *)
      &kNaClSimpleLtdServiceVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)((struct NaClRefCount *) self);
}

static struct NaClNameServiceEntry **NameServiceSearch(
    struct NaClNameServiceEntry **hd,
    char const                  *name) {
  while (NULL != *hd && 0 != strcmp((*hd)->name, name)) {
    hd = &(*hd)->next;
  }
  return hd;
}


int NaClNameServiceCreateDescEntry(
    struct NaClNameService  *nnsp,
    char const              *name,
    struct NaClDesc         *new_desc) {
  int                         retval = NACL_NAME_SERVICE_INSUFFICIENT_RESOURCES;
  struct NaClNameServiceEntry *name_entry = NULL;
  struct NaClNameServiceEntry *found = NULL;
  char                        *dup_name = STRDUP(name);

  /*
   * common case is insertion success, so we pre-allocate memory
   * (strdup, malloc) to avoid doing memory allocations while holding
   * the name service lock.
   */
  if (NULL == dup_name) {
    goto dup_failed;
  }
  name_entry = (struct NaClNameServiceEntry *) malloc(sizeof *name_entry);
  if (NULL == name_entry) {
    goto entry_alloc_failed;
  }

  NaClXMutexLock(&nnsp->mu);
  found = *NameServiceSearch(&nnsp->head, name);
  if (NULL != found) {
    retval = NACL_NAME_SERVICE_DUPLICATE_NAME;
    goto unlock_and_cleanup;
  }
  name_entry->next = nnsp->head;
  name_entry->name = dup_name;
  dup_name = (char *) NULL;
  name_entry->entry = NaClDescRef(new_desc);
  name_entry->factory = (NaClNameServiceFactoryFn_t) NULL;
  name_entry->state = (void *) NULL;
  nnsp->head = name_entry;
  name_entry = NULL;
  retval = NACL_NAME_SERVICE_SUCCESS;

 unlock_and_cleanup:
  NaClXMutexUnlock(&nnsp->mu);
  free(name_entry);
 entry_alloc_failed:
  free(dup_name);
 dup_failed:
  return retval;
}

int NaClNameServiceCreateFactoryEntry(
    struct NaClNameService      *nnsp,
    char const                  *name,
    NaClNameServiceFactoryFn_t  factory_fn,
    void                        *factory_state) {
  int                         retval = NACL_NAME_SERVICE_INSUFFICIENT_RESOURCES;
  struct NaClNameServiceEntry *name_entry = NULL;
  struct NaClNameServiceEntry *found = NULL;
  char                        *dup_name = STRDUP(name);

  /*
   * common case is insertion success, so we pre-allocate memory
   * (strdup, malloc) to avoid doing memory allocation while holding
   * the name service lock.
   */
  if (NULL == dup_name) {
    goto dup_failed;
  }
  name_entry = (struct NaClNameServiceEntry *) malloc(sizeof *name_entry);
  if (NULL == name_entry) {
    goto entry_alloc_failed;
  }

  NaClXMutexLock(&nnsp->mu);
  found = *NameServiceSearch(&nnsp->head, name);
  if (NULL != found) {
    retval = NACL_NAME_SERVICE_DUPLICATE_NAME;
    goto unlock_and_cleanup;
  }
  name_entry->next = nnsp->head;
  name_entry->name = dup_name;
  dup_name = (char *) NULL;
  name_entry->entry = (struct NaClDesc *) NULL;
  name_entry->factory = factory_fn;
  name_entry->state = factory_state;
  nnsp->head = name_entry;
  name_entry = NULL;
  retval = NACL_NAME_SERVICE_SUCCESS;

 unlock_and_cleanup:
  NaClXMutexUnlock(&nnsp->mu);
  free(name_entry);
 entry_alloc_failed:
  free(dup_name);
 dup_failed:
  return retval;
}

int NaClNameServiceResolveName(struct NaClNameService  *nnsp,
                               char const              *name,
                               struct NaClDesc         **out) {
  struct NaClNameServiceEntry *nnsep;
  int                         status = NACL_NAME_SERVICE_NAME_NOT_FOUND;

  NaClXMutexLock(&nnsp->mu);
  nnsep = *NameServiceSearch(&nnsp->head, name);
  if (NULL != nnsep) {
    if (NULL != nnsep->entry) {
      *out = NaClDescRef(nnsep->entry);
      status = NACL_NAME_SERVICE_SUCCESS;
    } else {
      status = (*nnsep->factory)(nnsep->state, name, out);
    }
  }
  nnsep = NULL;
  NaClXMutexUnlock(&nnsp->mu);
  return status;
}

int NaClNameServiceDeleteName(struct NaClNameService *nnsp,
                              char const             *name) {
  struct NaClNameServiceEntry **nnsepp;
  struct NaClNameServiceEntry *to_free = NULL;
  int                         status = NACL_NAME_SERVICE_NAME_NOT_FOUND;

  NaClXMutexLock(&nnsp->mu);
  nnsepp = NameServiceSearch(&nnsp->head, name);
  if (NULL != *nnsepp) {
    to_free = *nnsepp;
    *nnsepp = to_free->next;
    status = NACL_NAME_SERVICE_SUCCESS;
  }
  NaClXMutexUnlock(&nnsp->mu);

  /* do the free operations w/o holding the lock */
  if (NULL != to_free) {
    NaClDescSafeUnref(to_free->entry);
    if (NULL != to_free->factory) {
      (void) (*to_free->factory)(to_free->state,
                                 to_free->name,
                                 (struct NaClDesc **) NULL);
    }
    free((void *) to_free->name);
    free(to_free);
  }
  return status;
}

size_t NaClNameServiceEnumerate(struct NaClNameService  *nnsp,
                                char                    *dest,
                                size_t                  nbytes) {
  struct NaClNameServiceEntry *p;
  size_t                      written;
  size_t                      to_write;
  size_t                      name_len;

  NaClXMutexLock(&nnsp->mu);

  for (p = nnsp->head, written = 0;
       NULL != p && written < nbytes;
       p = p->next, written += to_write) {
    name_len = strlen(p->name) + 1;  /* cannot overflow */
    to_write = (name_len < nbytes - written) ? name_len
        : nbytes - written;
    /* written + to_write <= nbytes */
    strncpy(dest, p->name, to_write);
    dest += to_write;
  }

  NaClXMutexUnlock(&nnsp->mu);
  return written;
}

static void NaClNameServiceNameInsertRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClNameService  *nnsp =
      (struct NaClNameService *) rpc->channel->server_instance_data;
  char                    *name = in_args[0]->arrays.str;
  struct NaClDesc         *desc = in_args[1]->u.hval;

  out_args[0]->u.ival = (*NACL_VTBL(NaClNameService, nnsp)->CreateDescEntry)(
      nnsp, name, desc);
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
}

static void NaClNameServiceNameLookupRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClNameService  *nnsp =
      (struct NaClNameService *) rpc->channel->server_instance_data;
  char                    *name = in_args[0]->arrays.str;
  int                     status;
  struct NaClDesc         *desc;

  status = (*NACL_VTBL(NaClNameService, nnsp)->ResolveName)(
      nnsp, name, &desc);
  out_args[0]->u.ival = status;
  out_args[1]->u.hval = (NACL_NAME_SERVICE_SUCCESS == status)
      ? desc
      : (struct NaClDesc *) NaClDescInvalidMake();
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
}

static void NaClNameServiceNameDeleteRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClNameService  *nnsp =
      (struct NaClNameService *) rpc->channel->server_instance_data;
  char                    *name = in_args[0]->arrays.str;

  out_args[0]->u.ival = (*NACL_VTBL(NaClNameService, nnsp)->DeleteName)(
      nnsp, name);
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
}

static void NaClNameServiceListRpc(
    struct NaClSrpcRpc      *rpc,
    struct NaClSrpcArg      **in_args,
    struct NaClSrpcArg      **out_args,
    struct NaClSrpcClosure  *done_cls) {
  struct NaClNameService  *nnsp =
      (struct NaClNameService *) rpc->channel->server_instance_data;
  size_t                  nbytes = out_args[0]->u.count;
  char                    *dest = out_args[0]->arrays.carr;

  UNREFERENCED_PARAMETER(in_args);
  out_args[0]->u.count = (uint32_t) (*NACL_VTBL(NaClNameService, nnsp)->
                                     Enumerate)(nnsp, dest, nbytes);
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
}

struct NaClSrpcHandlerDesc const kNaClNameServiceHandlers[] = {
  { "list::C", NaClNameServiceListRpc, },
  { "insert:sh:i", NaClNameServiceNameInsertRpc, },
  { "lookup:s:ih", NaClNameServiceNameLookupRpc, },
  { "delete:s:i", NaClNameServiceNameDeleteRpc, },
  { (char const *) NULL, (NaClSrpcMethod) NULL, },
};

void NaClNameServiceLaunch(struct NaClNameService *self) {

  NaClLog(4, "NaClNameServiceThread: starting service\n");
  NaClSimpleServiceStartServiceThread((struct NaClSimpleService *) self);
}

struct NaClNameServiceVtbl kNaClNameServiceVtbl = {
  {
    /* really NaClSimpleLtdServiceVtbl contents */
    {
      NaClNameServiceDtor,
    },
    NaClSimpleServiceConnectionFactory,
    NaClSimpleLtdServiceAcceptConnection,
    NaClSimpleServiceAcceptAndSpawnHandler,
    NaClSimpleLtdServiceRpcHandler,
  },
  NaClNameServiceCreateDescEntry,
  NaClNameServiceCreateFactoryEntry,
  NaClNameServiceResolveName,
  NaClNameServiceDeleteName,
  NaClNameServiceEnumerate,
};
