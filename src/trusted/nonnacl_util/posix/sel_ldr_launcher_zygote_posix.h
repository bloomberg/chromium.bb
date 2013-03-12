/* -*- c++ -*- */
/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_POSIX_SEL_LDR_LAUNCHER_ZYGOTE_POSIX_H_
#define NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_POSIX_SEL_LDR_LAUNCHER_ZYGOTE_POSIX_H_

#include <map>
#include <vector>

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher_zygote_base.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_raii.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

/* SRPC signatures */
#define NACL_ZYGOTE_LAUNCH_PROCESS  "launch_process:C:hii"
#define NACL_ZYGOTE_RELEASE_CHANNEL "release_by_id:i:i"

namespace nacl {
class ZygotePosix : public ZygoteBase {
 public:
  // The Zygote ctor is simple, POD-style initialization.  More
  // complex system-level initialization is done by the bool Init()
  // method, which may spawn subprocesses etc.
  ZygotePosix();
  virtual ~ZygotePosix();

  // Startup initialization: spawn the zygote subprocess, which will
  // handle RPC calls to provide the sel_ldr spawning service.  May
  // fail.
  virtual bool Init();

  // Methods used in client (parent) process to request action by the
  // zygote process:

  // Serialize input arguments, send to zygote process, which uses
  // another SelLdrLauncherStandalone object to fork and exec the
  // desired process, then sends *out_channel in the RPC reply to
  // return to the caller for establishing bootstrap_socket.  *out_pid
  // is needed by SelLdrLauncherStandalone::KillChildProcess, which is
  // invoked from the dtor.
  bool SpawnNaClSubprocess(const std::vector<nacl::string>& prefix,
                           const std::vector<nacl::string>& sel_ldr_argv,
                           const std::vector<nacl::string>& app_argv,
                           NaClHandle* out_channel,
                           int* out_channel_id,
                           int* out_pid);

  // Since the descriptor in *out_channel is in-flight after
  // SpawnNaClSubprocess returns and due to kernel bugs on OSX we
  // should not close the last reference to a descriptor outside of
  // communication channels until after the descriptor has been
  // externalized to the recipient process, we have to do a 2-phase
  // protocol to acknowledge receipt.  Fortunately,
  // SpawnNaClSubprocess is expensive enough that the extra RPC should
  // not be noticeable.  It's an ugly implementation gotcha to have to
  // expose, but this is not to user code.  The initial implementation
  // should have full 2-phase commit/release, but as a TODO(bsy): we
  // should remove the requirement for invoking ReleaseChannelById for
  // Linux and get rid of it entirely.
  bool ReleaseChannelById(int channel_id);

  // Methods used in zygote process.  These could be private by making
  // the SRPC handler functions friends.

  bool HandleSpawn(const std::vector<nacl::string>& prefix,
                   const std::vector<nacl::string>& sel_ldr_argv,
                   const std::vector<nacl::string>& app_argv,
                   struct NaClDesc** out_channel,
                   int* out_channel_id,
                   int* out_pid);

  bool HandleReleaseChannel(int channel_id);

 private:
  scoped_ptr<DescWrapperFactory> factory_;

  scoped_ptr<DescWrapper> comm_channel_;
  // end of socketpair, for server side (child) use

  // The mutex mu_ ensures exclusive access to client_channel_ on the
  // client side, and similarly ensures exclusive access to
  // channel_map_ and channel_ix_ on the server side.
  struct NaClMutex mu_;
  struct NaClSrpcChannel client_channel_;
  std::map<int32_t, struct NaClDesc *> channel_map_;
  int32_t channel_ix_;

  void ZygoteServiceLoop();  // child calls this to go into SRPC service loop
};
}

#endif
