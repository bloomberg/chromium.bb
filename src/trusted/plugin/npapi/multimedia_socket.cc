/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/npapi/multimedia_socket.h"

#include <string.h>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/platform/nacl_time.h"

#include "native_client/src/trusted/desc/nacl_desc_imc.h"

#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/npapi/plugin_npapi.h"
#include "native_client/src/trusted/plugin/npapi/video.h"
#include "native_client/src/trusted/plugin/service_runtime.h"
#include "native_client/src/trusted/plugin/shared_memory.h"
#include "native_client/src/trusted/plugin/srpc_client.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace {

uintptr_t kNaClMultimediaBridgeIdent;

int const kMaxUpcallThreadWaitSec = 5;
int const kNanoXinMicroX = 1000;

// NB: InitializeIdentifiers is not thread-safe.
void InitializeIdentifiers(plugin::BrowserInterface* browser_interface) {
  static bool identifiers_initialized = false;

  if (!identifiers_initialized) {
    kNaClMultimediaBridgeIdent =
        browser_interface->StringToIdentifier("nacl_multimedia_bridge");
    identifiers_initialized = true;
  }
}

}  // namespace

namespace plugin {

MultimediaSocket::MultimediaSocket(BrowserInterface* browser_interface,
                                   ServiceRuntime* sri)
    : browser_interface_(browser_interface),
      service_runtime_(sri),
      upcall_thread_should_exit_(false),
      upcall_thread_id_(0) {
  NaClMutexCtor(&mu_);
  NaClCondVarCtor(&cv_);
  upcall_thread_state_ = UPCALL_THREAD_NOT_STARTED;
  InitializeIdentifiers(browser_interface_);  // inlineable; tail call.
}

MultimediaSocket::~MultimediaSocket() {
  enum UpcallThreadState ts;

  PLUGIN_PRINTF(("MultimediaSocket::~MultimediaSocket: entered\n"));
  NaClXMutexLock(&mu_);
  if (NULL == getenv("NACLTEST_DISABLE_SHUTDOWN")) {
    upcall_thread_should_exit_ = true;
    PLUGIN_PRINTF((" set flag to tell upcall thread to exit.\n"));
  } else {
    PLUGIN_PRINTF((" NOT telling upcall thread to exit.\n"));
  }
  PLUGIN_PRINTF((" upcall_thread_state_ %d\n", upcall_thread_state_));
  if (UPCALL_THREAD_NOT_STARTED != (ts = upcall_thread_state_)) {
    while (UPCALL_THREAD_EXITED != upcall_thread_state_) {
      PLUGIN_PRINTF(("MultimediaSocket::~MultimediaSocket:"
                     " waiting for upcall thread to exit\n"));
      NaClXCondVarWait(&cv_, &mu_);
    }
  }
  NaClXMutexUnlock(&mu_);
  if (UPCALL_THREAD_NOT_STARTED != ts) {
    NaClThreadDtor(&upcall_thread_);
  }
  NaClCondVarDtor(&cv_);
  NaClMutexDtor(&mu_);
  PLUGIN_PRINTF(("MultimediaSocket::~MultimediaSocket: done.\n"));
}

void MultimediaSocket::UpcallThreadExiting() {
  NaClXMutexLock(&mu_);
  upcall_thread_state_ = UPCALL_THREAD_EXITED;
  NaClXCondVarBroadcast(&cv_);
  NaClXMutexUnlock(&mu_);
}

static void handleUpcall(NaClSrpcRpc* rpc,
                         NaClSrpcArg** ins,
                         NaClSrpcArg** outs,
                         NaClSrpcClosure* done) {
  UNREFERENCED_PARAMETER(ins);
  UNREFERENCED_PARAMETER(outs);
  rpc->result = NACL_SRPC_RESULT_BREAK;
  if (rpc->channel) {
    VideoScopedGlobalLock video_lock;
    VideoCallbackData* video_cb_data;
    Plugin* portable_plugin;
    MultimediaSocket* msp;

    PLUGIN_PRINTF(("Upcall: channel %p\n",
                   static_cast<void*>(rpc->channel)));
    PLUGIN_PRINTF(("Upcall: server_instance_data %p\n",
                   static_cast<void*>(rpc->channel->server_instance_data)));
    video_cb_data = reinterpret_cast<VideoCallbackData*>
        (rpc->channel->server_instance_data);
    portable_plugin = video_cb_data->portable_plugin;
    if (NULL != portable_plugin) {
      VideoMap* video = static_cast<PluginNpapi*>(portable_plugin)->video();
      if (video) {
        video->RequestRedraw();
      }
      rpc->result = NACL_SRPC_RESULT_OK;
    } else if (NULL != getenv("NACLTEST_DISABLE_SHUTDOWN")) {
      PLUGIN_PRINTF(("Upcall: SrpcPlugin dtor invoked VideoMap dtor,"
                     "but pretending that the channel is okay for testing\n"));
      rpc->result = NACL_SRPC_RESULT_OK;
    } else {
      PLUGIN_PRINTF(("Upcall: plugin was NULL\n"));
    }
    msp = video_cb_data->msp;
    if (NULL != msp) {
      if (msp->UpcallThreadShouldExit()) {
        rpc->result = NACL_SRPC_RESULT_BREAK;
      }
    }
  }
  if (NACL_SRPC_RESULT_OK == rpc->result) {
    PLUGIN_PRINTF(("Upcall: success\n"));
  } else if (NACL_SRPC_RESULT_BREAK == rpc->result) {
    PLUGIN_PRINTF(("Upcall: break detected, thread exiting\n"));
  } else {
    PLUGIN_PRINTF(("Upcall: failure\n"));
  }
  done->Run(done);
}

bool MultimediaSocket::UpcallThreadShouldExit() {
  bool should_exit;
  NaClMutexLock(&mu_);
  should_exit = upcall_thread_should_exit_;
  NaClMutexUnlock(&mu_);
  return should_exit;
}

void MultimediaSocket::set_upcall_thread_id(uint32_t tid) {
  NaClMutexLock(&mu_);
  upcall_thread_id_ = tid;
  NaClCondVarSignal(&cv_);
  NaClMutexUnlock(&mu_);
}

static void WINAPI UpcallThread(void* arg) {
  VideoCallbackData* cbdata;
  NaClSrpcHandlerDesc handlers[] = {
    { "upcall::", handleUpcall },
    { NULL, NULL }
  };

  cbdata = reinterpret_cast<VideoCallbackData*>(arg);
  PLUGIN_PRINTF(("MultimediaSocket::UpcallThread(%p)\n", arg));
  PLUGIN_PRINTF(("MultimediaSocket::cbdata->portable_plugin %p\n",
                 static_cast<void*>(cbdata->portable_plugin)));
  // Set up the DescWrapper* the server will be placed on.
  if (NULL == cbdata->handle) {
    PLUGIN_PRINTF(("MultimediaSocket::UpcallThread(%p) FAILED\n", arg));
    return;
  }
  cbdata->msp->set_upcall_thread_id(NaClThreadId());
  // Run the SRPC server.
  NaClSrpcServerLoop(cbdata->handle->desc(), handlers, cbdata);
  // release the cbdata
  cbdata->msp->UpcallThreadExiting();
  VideoGlobalLock();
  VideoMap::ReleaseCallbackData(cbdata);
  VideoGlobalUnlock();
  PLUGIN_PRINTF(("MultimediaSocket::UpcallThread: End\n"));
}

// Support for initializing the NativeClient multimedia system.
bool MultimediaSocket::InitializeModuleMultimedia(
      Plugin* plugin, PortableHandle* connected_socket) {
  PLUGIN_PRINTF(("MultimediaSocket::InitializeModuleMultimedia(%p)\n",
                 static_cast<void*>(this)));

  VideoMap* video = static_cast<PluginNpapi*>(plugin)->video();
  ScriptableHandle* video_shared_memory = video->VideoSharedMemorySetup();

  // If there is no display shared memory region, don't initialize.
  // TODO(nfullagar,sehr): make sure this makes sense with NPAPI call order.
  if (NULL == video_shared_memory) {
    PLUGIN_PRINTF(("MultimediaSocket::InitializeModuleMultimedia:"
                   "No video_shared_memory.\n"));
    // trivial success case.  NB: if NaCl module and HTML disagrees,
    // then the HTML wins.
    return true;
  }
  // Determine whether the NaCl module has reported having a method called
  // "nacl_multimedia_bridge".
  if (!connected_socket->HasMethod(kNaClMultimediaBridgeIdent,
                                   METHOD_CALL)) {
    PLUGIN_PRINTF(("No nacl_multimedia_bridge method was found.\n"));
    return false;
  }
  // Create a socket pair.
  nacl::DescWrapper* desc[2];
  if (0 != plugin->wrapper_factory()->MakeSocketPair(desc)) {
    PLUGIN_PRINTF(("MakeSocketPair failed!\n"));
    return false;
  }
  // Start a thread to handle the upcalls.
  VideoCallbackData* cbdata;
  cbdata = video->InitCallbackData(desc[0], plugin, this);
  PLUGIN_PRINTF(("MultimediaSocket::InitializeModuleMultimedia:"
                 " launching thread\n"));
  uint32_t tid;
  NaClXMutexLock(&mu_);
  if (upcall_thread_state_ != UPCALL_THREAD_NOT_STARTED) {
    PLUGIN_PRINTF(("Internal error: upcall thread already running\n"));
    NaClXMutexUnlock(&mu_);
    return false;
  }
  if (!NaClThreadCtor(&upcall_thread_, UpcallThread, cbdata, 128 << 10)) {
    NaClXMutexUnlock(&mu_);
    // not constructed, so no dtor will be needed
    video->ForceDeleteCallbackData(cbdata);
    return false;
  }
  upcall_thread_state_ = UPCALL_THREAD_RUNNING;

  while (0 == (tid = upcall_thread_id_)) {
    NaClXCondVarWait(&cv_, &mu_);
  }
  NaClXMutexUnlock(&mu_);

  char buf[512];
  SNPRINTF(buf, sizeof buf,
           "MultimediaSocket upcall thread %x (%d)",
           tid,
           tid);
  service_runtime_->Log(4, buf);

  SrpcParams params("oo", "");

  PortableHandle* internal_video_shared_memory = video_shared_memory->handle();
  params.ins()[0]->tag = NACL_SRPC_ARG_TYPE_HANDLE;
  params.ins()[0]->u.hval = internal_video_shared_memory->desc();
  params.ins()[1]->tag = NACL_SRPC_ARG_TYPE_HANDLE;
  params.ins()[1]->u.hval = desc[1]->desc();

  PLUGIN_PRINTF(("MultimediaSocket::InitializeModuleMultimedia:"
                 " params %p\n", static_cast<void*>(&params)));

  bool rpc_result = connected_socket->Invoke(kNaClMultimediaBridgeIdent,
                                             METHOD_CALL,
                                             &params);
  PLUGIN_PRINTF(("MultmediaSocket::InitializeModuleMultimedia:"
                 " returned %d\n", static_cast<int>(rpc_result)));
  // TODO(sehr,mseaborn): use scoped_ptr for management of DescWrappers.
  delete desc[1];

  return rpc_result;
}

}  // namespace plugin
