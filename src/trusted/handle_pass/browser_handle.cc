/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// C/C++ library for handle passing in the Windows Chrome sandbox
// browser plugin interface.

#include <windows.h>
#include <map>
#include "native_client/src/trusted/handle_pass/handle_lookup.h"
#include "native_client/src/trusted/handle_pass/browser_handle.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/desc/nrd_xfer_effector.h"

static const HANDLE kInvalidHandle = reinterpret_cast<HANDLE>(-1);

// All APIs are guarded by the single mutex.
static struct NaClMutex pid_handle_map_mu = { NULL };

// The map.
static std::map<DWORD, HANDLE>* pid_handle_map = NULL;

// The NaClDesc for the bound socket used to accept connections from
// sel_ldr instances and the corresponding socket address.
// NOTE: we are not closing these descriptors when no instances remain alive.
static struct NaClDesc* handle_descs[2] = { NULL, NULL };

int NaClHandlePassBrowserInit() {
  return NaClMutexCtor(&pid_handle_map_mu);
}

int NaClHandlePassBrowserCtor() {
  int retval = 1;

  NaClMutexLock(&pid_handle_map_mu);
  if (NULL == pid_handle_map) {
    pid_handle_map = new(std::nothrow) std::map<DWORD, HANDLE>;
    if (NULL == pid_handle_map) {
      retval = 0;
    }
    HANDLE handle = GetCurrentProcess();
    DWORD pid = GetCurrentProcessId();
    (*pid_handle_map)[pid] = handle;
  }
  NaClMutexUnlock(&pid_handle_map_mu);
  NaClHandlePassSetLookupMode(HANDLE_PASS_BROKER_PROCESS);
  return retval;
}

HANDLE NaClHandlePassBrowserLookupHandle(DWORD pid) {
  HANDLE retval = NULL;
  NaClMutexLock(&pid_handle_map_mu);
  if (pid_handle_map->find(pid) == pid_handle_map->end()) {
    // IMC compares the result to NULL
    retval = NULL;
  } else {
    retval = (*pid_handle_map)[pid];
  }
  NaClMutexUnlock(&pid_handle_map_mu);
  return retval;
}

static NaClSrpcError Lookup(NaClSrpcChannel* channel,
                            NaClSrpcArg** in_args,
                            NaClSrpcArg** out_args) {
  NaClMutexLock(&pid_handle_map_mu);
  // The PID of the process wanting to send a descriptor.
  int sender_pid = in_args[0]->u.ival;
  // The PID of the process to be sent a descriptor.
  int recipient_pid = in_args[1]->u.ival;
  // The HANDLE in the sender process.  This is the duplicate of the
  // HANDLE contained in the mapping for recipient_pid.
  HANDLE recipient_handle;
  if (pid_handle_map->find(sender_pid) == pid_handle_map->end() ||
      pid_handle_map->find(recipient_pid) == pid_handle_map->end() ||
      FALSE == DuplicateHandle(GetCurrentProcess(),
                               (*pid_handle_map)[recipient_pid],
                               (*pid_handle_map)[sender_pid],
                               &recipient_handle,
                               0,
                               FALSE,
                               DUPLICATE_SAME_ACCESS)) {
    // IMC compares the result to NULL
    out_args[0]->u.ival = NULL;
  } else {
    out_args[0]->u.ival = reinterpret_cast<int>(recipient_handle);
  }
  NaClMutexUnlock(&pid_handle_map_mu);
  return NACL_SRPC_RESULT_OK;
}

static NaClSrpcError Shutdown(NaClSrpcChannel* channel,
                              NaClSrpcArg** in_args,
                              NaClSrpcArg** out_args) {
  return NACL_SRPC_RESULT_BREAK;
}

static void WINAPI HandleServer(void* dummy) {
  // Set up an effector.
  struct NaClDesc* pair[2];
  struct NaClNrdXferEffector effector;
  struct NaClDescEffector* effp;
  struct NaClDesc* lookup_desc;
  struct NaClSrpcHandlerDesc handlers[] = {
      { "lookup:ii:i", Lookup },
      { "shutdown::", Shutdown },
      { (char const *) NULL, (NaClSrpcMethod) 0, },};

  // Create a bound socket for use by the effector.
  if (0 != NaClCommonDescMakeBoundSock(pair)) {
    goto no_state;
  }
  // Create an effector to use to receive the connected socket.
  if (!NaClNrdXferEffectorCtor(&effector, pair[0])) {
    goto bound_sock_created;
  }
  effp = (struct NaClDescEffector*) &effector;
  // Accept on the bound socket.
  if (0 != (handle_descs[0]->vtbl->AcceptConn)(handle_descs[0], effp)) {
    goto effector_constructed;
  }
  // Get the connected socket from the effector.
  lookup_desc = NaClNrdXferEffectorTakeDesc(&effector);
  // Create an SRPC client and start the message loop.
  NaClSrpcServerLoop(lookup_desc, handlers, NULL);
  // Success.  Clean up.

 effector_constructed:
  effp->vtbl->Dtor(effp);
 bound_sock_created:
  NaClDescUnref(pair[1]);
  NaClDescUnref(pair[0]);
 no_state:
  NaClThreadExit();
}

struct NaClDesc* NaClHandlePassBrowserGetSocketAddress() {
  struct NaClDesc* socket_address = NULL;
  struct NaClThread thread;

  NaClMutexLock(&pid_handle_map_mu);
  // If the socket address is already set, bump the ref count and return it.
  if (NULL == handle_descs[1]) {
    // Otherwise, create the bound socket and socket address.
    if (0 != NaClCommonDescMakeBoundSock(handle_descs)) {
      goto no_cleanup;
    }
  }
  socket_address = NaClDescRef(handle_descs[1]);
  // Create the acceptor/server thread.
  NaClThreadCtor(&thread, HandleServer, NULL, 65536);
  // And return to the caller.
 no_cleanup:
  NaClMutexUnlock(&pid_handle_map_mu);
  return socket_address;
}

void NaClHandlePassBrowserRememberHandle(DWORD pid, HANDLE handle) {
  NaClMutexLock(&pid_handle_map_mu);
  (*pid_handle_map)[pid] = handle;
  NaClMutexUnlock(&pid_handle_map_mu);
}

void NaClHandlePassBrowserDtor() {
  // Nothing for now.
}
