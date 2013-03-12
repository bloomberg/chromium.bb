/* -*- c++ -*- */
/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Zygote process for POSIX environments.  The Zygote process is
// forked from the main process early on, so that the number of
// (shared) file descriptors is small or effectively non-existent.
// The parent process and the zygote share a communication channel
// over which the parent may request the zygote to launch subprocesses
// -- the zygote takes care of closing unnecessary descriptors, etc,
// so that the launched process's execution environment is reasonably
// sane for running sel_ldr as a subprcoess.


#include <fcntl.h>

#include "native_client/src/trusted/nonnacl_util/posix/sel_ldr_launcher_zygote_posix.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/serialization/serialization.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/nacl_base/nacl_refcount.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

namespace nacl {

class SelLdrLauncherStandaloneReal : public SelLdrLauncherStandalone {
 public:
  SelLdrLauncherStandaloneReal() {}
  NaClHandle channel() const { return channel_; }
  void DisassociateChildProcess() {
    child_process_ = NACL_INVALID_HANDLE;
  }
};

ZygotePosix::ZygotePosix()
    : channel_ix_(0) {}

bool ZygotePosix::Init() {
  CHECK(factory_ == NULL);
  factory_.reset(new DescWrapperFactory);
  NaClHandle pair[2];
  if (NaClSocketPair(pair) == -1) {
    return false;
  }
  int pid;
  pid = fork();
  if (-1 == pid) {
    NaClLog(LOG_INFO, "ZygotePosix::Init: fork failed\n");
    (void) NaClClose(pair[0]);
    (void) NaClClose(pair[1]);
    return false;
  }
  if (0 == pid) {
    // child
    (void) NaClClose(pair[1]);
    comm_channel_.reset(factory_->MakeImcSock(pair[0]));
    int rc = fcntl(pair[0], F_SETFD, FD_CLOEXEC);
    CHECK(rc == 0);
    NaClXMutexCtor(&mu_);
    NaClLog(LOG_INFO, "ZygotePosix::Init: child entering service loop\n");
    ZygoteServiceLoop();
    // NOTREACHED
    return false;
  } else {
    // parent
    NaClLog(4, "ZygotePosix::Init: parent closing pair[0]\n");
    (void) NaClClose(pair[0]);
    comm_channel_.reset(factory_->MakeImcSock(pair[1]));
    NaClLog(4, "ZygotePosix::Init: NaClSrpcClientCtor\n");
    if (!NaClSrpcClientCtor(&client_channel_, comm_channel_->desc())) {
      NaClLog(LOG_INFO, "ZygotePosix::Init: NaClSrpcClientCtor failed\n");
      (void) NaClClose(pair[1]);
      return false;
    }
    NaClLog(4, "ZygotePosix::Init: NaClXMutexCtor\n");
    NaClXMutexCtor(&mu_);
    NaClLog(4, "ZygotePosix::Init: done\n");
    return true;
  }
}

ZygotePosix::~ZygotePosix() {
  NaClSrpcDtor(&client_channel_);
  // comm_channel_ automatically destroyed at end.
  NaClMutexDtor(&mu_);
}


namespace {

void ZygoteLaunchHandler(
    struct NaClSrpcRpc *rpc,
    struct NaClSrpcArg **in_args,
    struct NaClSrpcArg **out_args,
    struct NaClSrpcClosure *done_cls) {
  ZygotePosix* instance =
      reinterpret_cast<ZygotePosix*>(rpc->channel->server_instance_data);
  // deserialize in_args[0]->u.count bytes at in_args[0]->arrays.carr
  // to obtain the vectors of strings.

  SerializationBuffer deserializer(
      reinterpret_cast<uint8_t*>(in_args[0]->arrays.carr),
      static_cast<size_t>(in_args[0]->u.count));
  std::vector<nacl::string> prefix;
  std::vector<nacl::string> sel_ldr_argv;
  std::vector<nacl::string> app_argv;

  if (!deserializer.Deserialize(&prefix)) {
    goto bad;
  }
  if (!deserializer.Deserialize(&sel_ldr_argv)) {
    goto bad;
  }
  if (!deserializer.Deserialize(&app_argv)) {
    goto bad;
  }

  NaClDesc *handle;
  int handle_id;
  int pid;

  if (!instance->HandleSpawn(prefix, sel_ldr_argv, app_argv,
                             &handle, &handle_id, &pid)) {
    goto bad;
  }

  out_args[0]->u.hval = handle;
  out_args[1]->u.ival = handle_id;
  out_args[2]->u.ival = pid;

  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
  // Cannot invoke NaClDescUnref yet on OSX; must wait for
  // ZygoteReleaseChannel, HandleReleaseChannel
  //
  // NaClDescUnref(handle);
  return;

 bad:
  rpc->result = NACL_SRPC_RESULT_OK;
  out_args[0]->u.hval = kNaClSrpcInvalidImcDesc;
  out_args[1]->u.ival = -1;  // invalid handle_id
  out_args[2]->u.ival = 0;
  // Use 0 instead of -1 for invalid pid; we avoid having a buggy
  // program do kill(-1, SIGKILL).  By using 0, a buggy program will
  // just kill every process in its own process group, which will
  // (hopefully) make the bug quite apparent, without taking all of
  // the user's processes down.
  (*done_cls->Run)(done_cls);
}

void ZygoteReleaseChannel(
    struct NaClSrpcRpc *rpc,
    struct NaClSrpcArg **in_args,
    struct NaClSrpcArg **out_args,
    struct NaClSrpcClosure *done_cls) {
  ZygotePosix* instance =
      reinterpret_cast<ZygotePosix*>(rpc->channel->server_instance_data);
  out_args[0]->u.ival = instance->HandleReleaseChannel(in_args[0]->u.ival);
  rpc->result = NACL_SRPC_RESULT_OK;
  (*done_cls->Run)(done_cls);
}

}  // namespace (anonymous)

void ZygotePosix::ZygoteServiceLoop() {
  struct NaClSrpcHandlerDesc const kZygoteServices[] = {
    { NACL_ZYGOTE_LAUNCH_PROCESS, ZygoteLaunchHandler, },
    { NACL_ZYGOTE_RELEASE_CHANNEL, ZygoteReleaseChannel, },
    { (char const *) NULL, (NaClSrpcMethod) NULL, },
  };

  NaClSrpcServerLoop(comm_channel_->desc(),
                     kZygoteServices,
                     this);
  NaClLog(4,
          "ZygotePosix::ZygoteServiceLoop(): NaClSrpcServerLoop exited;"
          " assuming embedder/Zygote-user (sel_universal) has exited.\n");
  _exit(0);
}

bool ZygotePosix::SpawnNaClSubprocess(
    const std::vector<nacl::string>& prefix,
    const std::vector<nacl::string>& sel_ldr_argv,
    const std::vector<nacl::string>& app_argv,
    NaClHandle* out_channel,
    int* out_channel_id,
    int* out_pid) {
  SerializationBuffer buf;
  if (!buf.Serialize(prefix) ||
      !buf.Serialize(sel_ldr_argv) ||
      !buf.Serialize(app_argv)) {
    return false;
  }

  NaClLog(4, "Entered ZygotePosix::SpawnNaClSubprocess\n");
  uint32_t num_bytes;
  if (buf.num_bytes() > static_cast<size_t>(~static_cast<uint32_t>(0))) {
    NaClLog(4, "ZygotePosix::SpawnNaClSubprocess: arglist too large\n");
    return false;
  }
  num_bytes = static_cast<uint32_t>(buf.num_bytes());

  NaClDesc *desc = NULL;
  int channel_id = 0;
  int pid = 0;

  {
    NaClSrpcError rpc_result;
    MutexLocker take(&mu_);
    rpc_result = NaClSrpcInvokeBySignature(
        &client_channel_,
        NACL_ZYGOTE_LAUNCH_PROCESS,
        num_bytes,
        reinterpret_cast<char const *>(buf.data()),
        &desc,
        &channel_id,
        &pid);
    if (rpc_result != NACL_SRPC_RESULT_OK) {
      NaClLog(4,
              ("ZygotePosix::SpawnNaClSubprocess: NaClSrpcInvokeBySignature"
               " failed, error %d\n"),
              rpc_result);
      return false;
    }
  }

  if (desc == NULL || NACL_VTBL(NaClDesc, desc)->typeTag == NACL_DESC_INVALID) {
    // An error occurred zygote-side.
    return false;
  }

  CHECK(NACL_VTBL(NaClDesc, desc)->typeTag ==
        NACL_DESC_TRANSFERABLE_DATA_SOCKET);

  *out_channel = reinterpret_cast<struct NaClDescXferableDataDesc *>(desc)->
      base.h;
  *out_channel_id = channel_id;
  *out_pid = pid;
  // THIS IS AN UGLY HACK.  AVOID DOING THIS EVER AGAIN ELSEWHERE.
  //
  // We get a struct NaClDesc that is a NaClDescXferableDataDesc and
  // we reach in to extract the handle -- the bootstrap channel_ isn't
  // really an IMC desc -- and so we cannot run the standard Unref
  // which leads to a close of the handle without setting the handle
  // to NACL_INVALID_HANDLE.
  //
  // A cleaner hack is to create a NaClDesc subclass which wraps an
  // opaque handle.  This subclass would implement no usable methods
  // -- everything would be an *Unimplemented function -- with a
  // special static member accessor that allows the extraction of the
  // underlying descriptor by trusted code.  Since we don't foresee
  // ever needing this functionality elsewhere, we are just going to
  // use the UGLY HACK and document this alternative.
  reinterpret_cast<struct NaClDescXferableDataDesc *>(desc)->base.h =
      NACL_INVALID_HANDLE;
  NaClDescUnref(desc);

  return true;
}

bool ZygotePosix::ReleaseChannelById(int channel_id) {
  MutexLocker take(&mu_);
  NaClSrpcError rpc_result;
  int result;
  rpc_result = NaClSrpcInvokeBySignature(&client_channel_,
                                         NACL_ZYGOTE_RELEASE_CHANNEL,
                                         channel_id,
                                         &result);
  if (rpc_result != NACL_SRPC_RESULT_OK) {
    return false;
  }
  NaClLog(4, "ZygotePosix::ReleaseChannelById(%d) -> %d\n", channel_id, result);
  return result != 0;
}

bool ZygotePosix::HandleSpawn(const std::vector<nacl::string>& prefix,
                              const std::vector<nacl::string>& sel_ldr_argv,
                              const std::vector<nacl::string>& app_argv,
                              struct NaClDesc** out_channel,
                              int32_t* out_channel_id,
                              int32_t* out_pid) {
  NaClLog(4, "Entered ZygotePosix::HandleSpawn\n");
  SelLdrLauncherStandaloneReal* real_obj = new SelLdrLauncherStandaloneReal;

  if (!real_obj->StartViaCommandLine(prefix, sel_ldr_argv, app_argv)) {
    return false;
  }
  struct NaClDescXferableDataDesc *desc =
      reinterpret_cast<NaClDescXferableDataDesc*>(
          malloc(sizeof *desc));
  if (!NaClDescXferableDataDescCtor(desc, real_obj->channel())) {
    free(desc);
    return false;
  }
  *out_channel = reinterpret_cast<struct NaClDesc *>(desc);
  NaClLog(4, "ZygotePosix::HandleSpawn: taking lock to insert desc\n");
  MutexLocker take(&mu_);
  while (channel_map_.find(++channel_ix_) != channel_map_.end()) {
    // find a channel_ix_ as an unused key
  }
  channel_map_[channel_ix_] = reinterpret_cast<struct NaClDesc *>(desc);
  *out_channel_id = channel_ix_;
  *out_pid = real_obj->child_process();
  real_obj->DisassociateChildProcess();
  return true;
}

bool ZygotePosix::HandleReleaseChannel(int channel_id) {
  NaClLog(4, "Entered ZygotePosix::HandleReleaseChannel\n");
  MutexLocker take(&mu_);
  std::map<int, struct NaClDesc *>::iterator it = channel_map_.find(channel_id);
  if (it == channel_map_.end()) {
    NaClLog(LOG_ERROR, "ZygotePosix::HandleReleaseChannel(%d): no such entry\n",
            channel_id);
    return false;
  }
  struct NaClDesc *desc = it->second;
  channel_map_.erase(it);
  NaClDescUnref(desc);
  return true;
}

}  // namespace nacl
