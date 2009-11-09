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
 * Chrome native WebWorker support.
 */
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * There is a lot of partial commonality between this and sel_main.c
 * TODO(sehr): refactor to eliminate the commonality.
 */

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/gio/gio.h"
#include "native_client/src/trusted/platform_qualify/nacl_os_qualify.h"

#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/expiration.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/web_worker_stub.h"


int verbosity = 0;

/*
 * Logging hack: open a descriptor to redirect the logs to in order
 * to make libsel.lib work correctly.  It is not thread safe.
 */
static int out_desc = -1;

/*
 * Connect creates a NaClDesc from performing a connect to the bound socket
 * passed in.  This is used to set up the SRPC channels to the worker.
 */
static struct NaClDesc *Connect(struct NaClDesc* bound_desc) {
  struct NaClNrdXferEffector eff;
  struct NaClDescEffector *effp;
  struct NaClDesc *connected_desc = NULL;

  if (NaClNrdXferEffectorCtor(&eff, bound_desc)) {
    effp = (struct NaClDescEffector *) &eff;
    if (0 == (bound_desc->vtbl->ConnectAddr)(bound_desc, effp)) {
      connected_desc = NaClNrdXferEffectorTakeDesc(&eff);
    }
  }

  return connected_desc;
}

/*
 * Start a native webworker from the module in buffer.
 */
int NaClStartNativeWebWorker(char *buffer,
                             size_t buf_len,
                             struct NaClApp **nap,
                             struct NaClSrpcChannel **untrusted_channel) {
  struct NaClApp*       state;

  struct GioMemoryFile  gf;

  int                   ret_code;
  static char*          argv[] = { "NaClMain", 0 };
  static char const*    envp[] = { 0 };
  struct NaClDesc*      command_desc;
  struct NaClDesc*      untrusted_desc;

  /* Initialize channel and nap results */
  *nap = NULL;
  *untrusted_channel = NULL;

  /* do expiration check first */
  if (NaClHasExpired()) {
    fprintf(stderr, "This version of Native Client has expired.\n");
    fprintf(stderr, "Please visit: http://code.google.com/p/nativeclient/\n");
    return -1;
  }

  ret_code = 1;
  /* we send the worker output to "dev null" */
  out_desc = open(PORTABLE_DEV_NULL, O_WRONLY);
  if (-1 == out_desc) {
    return -1;
  }

  /*
   * Set a flag making it look like this is embedded in the browser.
   * TODO(sehr): eliminate this flag.
   */
  NaClSrpcFileDescriptor = 5;

  /*
   * AllModulesInit/Fini are used for per-process initialization, and hence
   * what we are doing now does not allow multiple instances per process.
   * TODO(sehr): revisit this when we want to allow processes be reused.
   */
  NaClAllModulesInit();

  if (0 == GioMemoryFileCtor(&gf, buffer, buf_len)) {
    return 1;
  }

  state = (struct NaClApp *) malloc(sizeof(struct NaClApp));
  if (NULL == state) {
    return 1;
  }
  if (!NaClAppCtor(state)) {
    goto done_file_dtor;
  }

  state->restrict_to_main_thread = 1;

  *nap = state;

  /*
   * Ensure this operating system platform is supported.
   */
  if (!NaClOsIsSupported()) {
    goto done;
  }
  /*
   * Load the NaCl module from the memory file.
   */
  if (LOAD_OK != NaClAppLoadFile((struct Gio *) &gf,
                                 *nap,
                                 NACL_ABI_MISMATCH_OPTION_ABORT)) {
    goto done;
  }
  /*
   * Close the memory pseudo-file used for passing in the .nexe.
   */
  if ((*((struct Gio *) &gf)->vtbl->Close)((struct Gio *) &gf) == -1) {
    goto done;
  }
  (*((struct Gio *) &gf)->vtbl->Dtor)((struct Gio *) &gf);

  /*
   * Finish setting up the NaCl App.  This includes dup'ing
   * descriptors 0-2 and making them available to the NaCl App.
   */
  if (LOAD_OK != NaClAppPrepareToLaunch(*nap, 0, out_desc, out_desc)) {
    goto done;
  }

  /*
   * We create a bound socket and socket address pair.
   */
  NaClCreateServiceSocket(*nap);

  NaClXMutexLock(&(*nap)->mu);
  (*nap)->module_load_status = LOAD_OK;
  NaClXCondVarBroadcast(&(*nap)->cv);
  NaClXMutexUnlock(&(*nap)->mu);

  /*
   * only *nap->ehdrs.e_entry is usable, no symbol table is
   * available.
   */
  if (!NaClCreateMainThread(*nap, 1, argv, envp)) {
    goto done;
  }

  /*
   * Since we did not create the secure command channel, the first connection
   * is to the untrusted command channel, which is also not used for workers.
   */
  if (NULL == (command_desc = Connect((*nap)->service_address))) {
    goto done_thread_created;
  }

  /*
   * The second connection is to the service channel.
   */
  if (NULL == (untrusted_desc = Connect((*nap)->service_address))) {
    goto done_command_connected;
  }
  *untrusted_channel = (NaClSrpcChannel *) malloc(sizeof(NaClSrpcChannel));
  if (NULL == *untrusted_channel) {
    goto done_untrusted_connected;
  }
  if (!NaClSrpcClientCtor(*untrusted_channel, untrusted_desc)) {
    goto done_srpc_allocated;
  }

  return 0;

 done_srpc_allocated:
  free(*untrusted_channel);
  *untrusted_channel = NULL;

 done_untrusted_connected:
  NaClDescUnref(untrusted_desc);

 done_command_connected:
  NaClDescUnref(command_desc);

 done_thread_created:
    /* TODO(sehr): shut down main thread cleanly. */

 done:
  fflush(stdout);

  NaClAppDtor(state);
  free(state);
  *nap = NULL;

 done_file_dtor:
  NaClAllModulesFini();

  return ret_code;
}

/*
 * Causes a native web worker to be shut down.
 */
int NaClTerminateNativeWebWorker(struct NaClApp **nap,
                                 struct NaClSrpcChannel **untrusted_channel) {
  int retval;

  /*
   * Make sure things started correctly.
   */
  if (NULL == *nap || NULL == *untrusted_channel) {
    return 1;
  }

  retval = NaClWaitForMainThreadToExit(*nap);

  /*
   * Flush the module output stream.
   */
  fflush(stdout);
  close(out_desc);

  /*
   * Destroy and free the untrusted channel.
   */
  if (NULL != *untrusted_channel) {
    NaClSrpcDtor(*untrusted_channel);
    free(*untrusted_channel);
    *untrusted_channel = NULL;
  }
  /*
   * Destroy and free the application memory structure.
   */
  NaClAppDtor(*nap);
  free(*nap);
  *nap = NULL;

  NaClAllModulesFini();

  return retval;
}

/*
 * Performs a postMessage to the worker.
 */
int NaClPostMessageToNativeWebWorker(char *buffer,
                                     size_t buf_len,
                                     struct NaClApp **nap,
                                     struct NaClSrpcChannel **untrusted_ch) {
  /*
   * TODO(sehr): track down why there is a buf_len arg when buffer
   * ought to be an UTF-8 NUL-terminated string.
   */
  UNREFERENCED_PARAMETER(buf_len);

  /*
   * Make sure things started correctly.
   */
  if (NULL == *nap || NULL == *untrusted_ch) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }

  return NaClSrpcInvokeByName(*untrusted_ch, "postMessage", buffer);
}

/*
 * Creates a channel that can be used to receive upcalls (postMessage from
 * a worker).
 */
int NaClCreateUpcallChannel(struct NaClDesc* desc[2]) {
  NaClHandle nh[2];

  struct NaClDescXferableDataDesc *nacl_desc;
  /* The module's end of the pair */

  struct NaClDescXferableDataDesc *chrome_desc;
  /* The browser's end of the pair */

  /*
   * Initialize the descriptors to invalid.
   */
  desc[0] = NULL;
  desc[1] = NULL;

  /*
   * Create a socket pair.
   */
  if (0 != NaClSocketPair(nh)) {
    return 0;
  }

  /*
   * Create an IMC descriptor for the NaCl module's end of the socket pair.
   */
  nacl_desc = (struct NaClDescXferableDataDesc *) malloc(sizeof *nacl_desc);
  if (NULL == nacl_desc) {
    return 0;
  }
  if (!NaClDescXferableDataDescCtor(nacl_desc, nh[0])) {
    free(nacl_desc);
    return 0;
  }

  /*
   * Create an IMC descriptor for the browser worker's end of the socket pair.
   */
  chrome_desc = (struct NaClDescXferableDataDesc *) malloc(sizeof *chrome_desc);
  if (NULL == chrome_desc) {
    NaClDescUnref((struct NaClDesc*) nacl_desc);
    return 0;
  }
  if (!NaClDescXferableDataDescCtor(chrome_desc, nh[1])) {
    NaClDescUnref((struct NaClDesc*) nacl_desc);
    free(chrome_desc);
    return 0;
  }

  /*
   * Set the returned descriptors.
   */
  desc[0] = (struct NaClDesc*) nacl_desc;
  desc[1] = (struct NaClDesc*) chrome_desc;

  return 1;
}

/*
 * Destroys the upcall channel.
 */
void NaClDestroyUpcallChannel(struct NaClDesc* desc[2]) {
  NaClDescUnref(desc[0]);
  NaClDescUnref(desc[1]);
}

/*
 * Upcall handling is done by the use of SRPC handlers for postMessage
 * and postException (TBD).  The use of SRPC/IMC may be temporary as we are
 * currently using sockets to talk between threads within a process, certainly
 * not the most efficient implementation.
 */
struct NativeWorkerListenerData {
  NativeWorkerPostMessageFunc func;
  struct WebWorkerClient* client;
};

/*
 * This SRPC method is used to handle postMessage invocations from the
 * worker (native) code.
 */
static NaClSrpcError postMessageFromNativeWorker(NaClSrpcChannel *channel,
                                                 NaClSrpcArg **ins,
                                                 NaClSrpcArg **outs) {
  const char* str = ins[0]->u.sval;
  struct NativeWorkerListenerData *data;
  NativeWorkerPostMessageFunc func;
  struct WebWorkerClient* client;

  UNREFERENCED_PARAMETER(outs);
  data = (struct NativeWorkerListenerData *) channel->server_instance_data;
  func = data->func;
  client = data->client;
  (*func)(str, client);
  return NACL_SRPC_RESULT_OK;
}

static const NaClSrpcHandlerDesc handlers[] = {
  { "postMessage:s:", postMessageFromNativeWorker },
  { 0, 0 }
};

/*
 * Runs an SRPC server loop on the specified channel.  The post_message_func
 * is invoked whenever the "postMessage" RPC is received.
 */
int NaClSrpcListenerLoop(struct NaClDesc *chrome_desc,
                         NativeWorkerPostMessageFunc func,
                         WWClientPointer client) {
  struct NativeWorkerListenerData *data = NULL;
  int retval = 0;

  data = (struct NativeWorkerListenerData *) malloc(sizeof(*data));
  if (NULL == data) {
    goto cleanup;
  }
  data->func = func;
  data->client = client;
  /* Loop processing RPCs. */
  if (!NaClSrpcServerLoop(chrome_desc, handlers, (void *)data)) {
    goto cleanup;
  }
  retval = 1;
 cleanup:
  free(data);
  return retval;
}

/*
 * Part of the initialization of a worker.  Sends the descriptor to the
 * worker library to indicate where to send postMessage RPCs.
 */
int NaClSrpcSendUpcallDesc(struct NaClSrpcChannel *channel,
                           struct NaClDesc *nacl_desc) {
  return NaClSrpcInvokeByName(channel, "setUpcallDesc", nacl_desc);
}
