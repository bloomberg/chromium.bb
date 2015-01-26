/* -*- c++ -*- */
/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/nonnacl_util/launcher_factory.h"

#include <vector>

#include "native_client/src/include/build_config.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

#if NACL_LINUX || NACL_OSX
# define NACL_NEED_ZYGOTE 1
#elif NACL_WINDOWS
# define NACL_NEED_ZYGOTE 0
#else
# error "What OS?!?"
#endif

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher_zygote.h"

namespace nacl {

using std::vector;

#if NACL_NEED_ZYGOTE
class SelLdrLauncherStandaloneProxy : public SelLdrLauncherStandalone {
 public:
  explicit SelLdrLauncherStandaloneProxy(Zygote* zygote);

  virtual bool StartViaCommandLine(
      const vector<nacl::string>& prefix,
      const vector<nacl::string>& sel_ldr_argv,
      const vector<nacl::string>& app_argv);
  // This version of StartViaCommandLine essentially splits
  // SelLdrLauncherStandalone's version in half: we transfer the 3
  // arguments to the zygote, where the zygote will create its own
  // SelLdrLauncherStandalone object where the process creation
  // occurs.  Note that InitCommandLine and BuildCommandLine will
  // execute in the zygote's copy of the object, and it is in
  // InitCommandLine where the CreateBootstrapSocket's dest_fd get
  // pushed into the command line being built (preceded with "-X").
  // After the zygote's copy of
  // SelLdrLauncherStandalone::StartViaCommandLine has executed, the
  // channel_ member has the desired communication channel needed to
  // get (receive) the command channel and application channel socket
  // addresses, and we send this channel_ back in the RPC to forcibly
  // stuff into the SelLdrLauncherStandaloneProxy object.  Since the
  // three argv objects are needed after this, the fact that they
  // aren't initialized won't matter.  Ditto for close_after_launch_
  // etc.
  //
  // NB: the zygote's copy of SelLdrLauncherStandalone must have its
  // child_process_ pid set the NACL_INVALID_HANDLE before being
  // dtor'd, since otherwise the newly launched process will
  // immediately get killed.  The pid is retrieved along with the
  // channel from the zygote, and we stuff that into the proxy object
  // as well, so that the dtor semantics will work out.
 private:
  Zygote* zygote_;
};

SelLdrLauncherStandaloneProxy::SelLdrLauncherStandaloneProxy(Zygote* zygote)
    : zygote_(zygote) {}

bool SelLdrLauncherStandaloneProxy::StartViaCommandLine(
    const vector<nacl::string>& prefix,
    const vector<nacl::string>& sel_ldr_argv,
    const vector<nacl::string>& app_argv) {
  NaClHandle bootstrap_channel;
  int channel_id;
  int pid;

  if (!zygote_->SpawnNaClSubprocess(prefix, sel_ldr_argv, app_argv,
                                   &bootstrap_channel, &channel_id, &pid)) {
    return false;
  }
  if (!zygote_->ReleaseChannelById(channel_id)) {
    (void) close(bootstrap_channel);
    // On Unix-like OSes, closing the bootstrap channel suffices to
    // ensure that the spawned subprocess will exit: the bootstrap
    // channel contains the socketaddress socketpair endpoints for
    // trusted and untrusted connections, and if the zygote had
    // crashed, the last reference to the sockpair endpoints will
    // disappear with the close, and the imc_accept would get an EOF.
    // On Windows, we may be leaking a process, since the IMC channels
    // are implemented with named pipes, and there won't be any EOF
    // indication.  Since this code should never run unless the zygote
    // process crashed and thus is hard to test, we leave this
    // commented out.
    //
    // child_process_ = pid;
    // KillChildProcess();
    return false;
  }
  channel_ = bootstrap_channel;
  child_process_ = pid;
  return true;
}
#endif

SelLdrLauncherStandaloneFactory::SelLdrLauncherStandaloneFactory(
    ZygoteBase* zygote_interface)
    : zygote_(zygote_interface) {}

SelLdrLauncherStandaloneFactory::~SelLdrLauncherStandaloneFactory() {
}

#if NACL_NEED_ZYGOTE
SelLdrLauncherStandalone* SelLdrLauncherStandaloneFactory::
MakeSelLdrLauncherStandalone() {
  return new SelLdrLauncherStandaloneProxy(
      reinterpret_cast<ZygotePosix *>(zygote_));
}
#else
SelLdrLauncherStandalone* SelLdrLauncherStandaloneFactory::
MakeSelLdrLauncherStandalone() {
  return new SelLdrLauncherStandalone();
}
#endif

}  // namespace nacl
