// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// This is an early draft of background thread support.
// Until it is complete, we assume that all functions proxy functions
// (but CallOnMainThread) are called on the main thread.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Support for "upcalls" -- RPCs to the browser that are done from other than
// the main thread.  These calls are synchronized by the ppapi_proxy library
// at the plugin end.

#include "native_client/src/shared/ppapi_proxy/browser_upcall.h"

#include <new>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_core.h"
#include "srpcgen/upcall.h"

using nacl::DescWrapper;
using nacl::DescWrapperFactory;

namespace {

// Structure for passing information to the thread.  Shares ownership of
// the descriptor with the creating routine.  This allows passing ownership
// to the upcall thread.
struct UpcallInfo {
  nacl::scoped_ptr<DescWrapper> wrapper;
  NaClSrpcChannel* channel;
};

void WINAPI UpcallThread(void* arg) {
  // The memory for info was allocated on the creating (browser UI) thread,
  // but ownership was conferred to the upcall thread for deletion.
  nacl::scoped_ptr<UpcallInfo> info(reinterpret_cast<UpcallInfo*>(arg));
  NaClSrpcServerLoop(info->wrapper->desc(),
                     PpbUpcalls::srpc_methods,
                     info->channel);
}

}  // namespace

namespace ppapi_proxy {

DescWrapper* BrowserUpcall::Start(struct NaClThread* nacl_thread,
                                  NaClSrpcChannel* upcall_channel) {
  // Create a socket pair for the upcall server.
  DescWrapperFactory factory;
  DescWrapper* pair[2] = { NULL, NULL };
  if (factory.MakeSocketPair(pair)) {
    return NULL;
  }
  nacl::scoped_ptr<DescWrapper> browser_end(pair[0]);
  nacl::scoped_ptr<DescWrapper> plugin_end(pair[1]);
  // Create an info node to pass to the thread.
  nacl::scoped_ptr<UpcallInfo> info(new UpcallInfo);
  info->wrapper.reset(browser_end.get());
  info->channel = upcall_channel;
  // On success, info took ownership of browser_end.
  browser_end.release();
  // Create a thread and an SRPC "upcall" server.
  const int kThreadStackSize = 128 * 1024;
  if (!NaClThreadCreateJoinable(nacl_thread,
                                UpcallThread,
                                info.get(),
                                kThreadStackSize)) {
    return NULL;
  }
  // On successful thread creation, ownership of info passes to the thread.
  info.release();
  // On successful return, the caller gets the plugin_end of the socketpair.
  return plugin_end.release();
}

}  // namespace ppapi_proxy
