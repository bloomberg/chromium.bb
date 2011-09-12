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
// the PPAPI main thread.  These calls are synchronized by the ppapi_proxy
// library at the plugin end.

#include "native_client/src/include/portability.h"

struct NaClSrpcChannel;
struct NaClThread;

namespace nacl {
class DescWrapper;
}  // namespace nacl

namespace ppapi_proxy {

class BrowserUpcall {
 public:
  // If successful, creates the listener thread in the browser that will handle
  // upcalls and returns the plugin end of the upcall channel.  The caller
  // takes ownership of this descriptor.  If either thread or socket creation
  // fails, this method returns NULL.
  // The thread's state is maintained by nacl_thread.  Upcall processing may
  // cause rpcs to be performed on main_channel (e.g., CallOnMainThread),
  // so the interface takes main_channel, but the upcall thread does not have
  // ownership.
  static nacl::DescWrapper* Start(NaClThread* nacl_thread,
                                  NaClSrpcChannel* main_channel);
};

}  // namespace ppapi_proxy
