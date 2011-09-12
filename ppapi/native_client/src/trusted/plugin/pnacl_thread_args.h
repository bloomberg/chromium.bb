// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_THREAD_ARGS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_THREAD_ARGS_H_

#include "native_client/src/trusted/plugin/plugin_error.h"

#include "ppapi/cpp/completion_callback.h"

namespace plugin {

class BrowserInterface;
class NaClSubprocess;
class PnaclCoordinator;

// Base structure for storing pnacl helper thread arguments.
struct PnaclThreadArgs {
  PnaclThreadArgs(NaClSubprocess* subprocess_,
                  BrowserInterface* browser_,
                  pp::CompletionCallback finish_cb_)
    : should_die(false),
      subprocess(subprocess_),
      browser(browser_),
      finish_cb(finish_cb_) {
  }

  // Bool to signal to the thread that it should end whenever possible.
  bool should_die;

  // SRPC Nexe subprocess that does the work.
  NaClSubprocess* subprocess;
  // Browser Interface for SRPC setup.
  BrowserInterface* browser;

  // Callback to run when task is completed or an error has occurred.
  pp::CompletionCallback finish_cb;

  ErrorInfo error_info;
};

//----------------------------------------------------------------------
// Helper thread arguments.

// TODO(jvoung): Move these to the compile / link files when we separate
// those bits from pnacl_coordinator.

// Arguments needed to run LLVM in a separate thread, to go from
// bitcode -> object file. This prevents LLVM from blocking the main thread.
struct DoTranslateArgs : PnaclThreadArgs {
  DoTranslateArgs(NaClSubprocess* subprocess_,
                  BrowserInterface* browser_,
                  pp::CompletionCallback finish_cb_,
                  nacl::DescWrapper* pexe_fd_)
    : PnaclThreadArgs(subprocess_, browser_, finish_cb_),
      pexe_fd(pexe_fd_),
      obj_fd(kNaClSrpcInvalidImcDesc),
      obj_len(-1) {
  }

  // Borrowed references which must outlive the thread.
  nacl::DescWrapper* pexe_fd;

  // Output.
  NaClSrpcImcDescType obj_fd;
  int32_t obj_len;
};

// Arguments needed to run LD in a separate thread, to go from
// object file -> nexe.
struct DoLinkArgs : PnaclThreadArgs {
  DoLinkArgs(NaClSubprocess* subprocess_,
             BrowserInterface* browser_,
             pp::CompletionCallback finish_cb_,
             PnaclCoordinator* coordinator_,
             nacl::DescWrapper* obj_fd_,
             int32_t obj_len_)
    : PnaclThreadArgs(subprocess_, browser_, finish_cb_),
      coordinator(coordinator_),
      obj_fd(obj_fd_),
      obj_len(obj_len_),
      nexe_fd(kNaClSrpcInvalidImcDesc) {
  }
  PnaclCoordinator* coordinator;  // Punch hole in abstraction.

  // Borrowed references which must outlive the thread.
  nacl::DescWrapper* obj_fd;
  int32_t obj_len;

  // Output.
  NaClSrpcImcDescType nexe_fd;
};


}  // namespace plugin;
#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_THREAD_ARGS_H_
