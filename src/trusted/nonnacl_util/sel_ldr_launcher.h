/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


// Utility class for launching sel_ldr.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_H_

#include <vector>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

struct NaClSrpcChannel;
struct NaClDesc;

namespace nacl {
// TODO(robertm): Move this to new header if it becomes more popular.
template <class T> nacl::string ToString(const T& t) {
  nacl::stringstream ss;
  ss << t;
  return ss.str();
}

/*
 * This class defines an interface that locates sel_ldr.
 */
class SelLdrLocator {
 public:
  // Returns a directory with sel_ldr.
  virtual void GetDirectory(char* buffer, size_t len) = 0;

  SelLdrLocator() {}

  virtual ~SelLdrLocator() {}

  DISALLOW_COPY_AND_ASSIGN(SelLdrLocator);
};

/*
 * Default implementation of SelLdrLocator which tries to
 * locate a browser plugin. Default constructor of SelLdrLauncher use it.
 */
class PluginSelLdrLocator : public SelLdrLocator {
 public:
  // We have different implementations for all platforms.
  virtual void GetDirectory(char* buffer, size_t len);

  PluginSelLdrLocator() {}

  DISALLOW_COPY_AND_ASSIGN(PluginSelLdrLocator);
};

/*
 * This class encapsulates the process of launching an instance of sel_ldr
 * to communicate with the NaCl plugin over an IMC channel.
 *
 * The sel_ldr process can be forked directly using a command line of args
 * or by sending a message to the (Chrome) browser process.
 */
struct SelLdrLauncher {
 public:
  SelLdrLauncher();

  explicit SelLdrLauncher(SelLdrLocator* sel_ldr_locator);
  ~SelLdrLauncher();

  Handle child_process() const { return child_process_; }

  /////////////////////////////////////////////////////////////////////////////
  // Command line start-up: (Only used by sel_universal.)
  //
  // The command line must include a file path for the nexe application or an
  // indicator that a reference will be supplied after the launch over RPC.
  /////////////////////////////////////////////////////////////////////////////

  // Creates a socket pair.  Maps the first socket into the NaCl
  // subprocess's FD table as dest_fd. Sets the corresponding command line arg.
  // Returns the second socket. Returns kInvalidHandle on failure.
  Handle ExportImcFD(int dest_fd);

  // Sets up the command line to start a sel_ldr.  Specifies |imc_fd| to
  // use for bootstrapping. |sel_ldr_argv| specifies the arguments to be
  // passed to sel_ldr itself, while |application_argv| specifies the arguments
  // to be passed to the nexe.
  void InitCommandLine(int imc_fd,
                       const std::vector<nacl::string>& sel_ldr_argv,
                       const std::vector<nacl::string>& application_argv);

  // If subprocess creation fails, both child_process_ and channel_ are set to
  // kInvalidHandle. We have different implementations for posix and win.
  // You must call InitCommandLine() before calling this function.
  bool LaunchFromCommandLine();

  // Builds a command line out of the prepopulated args.
  void BuildCommandLine(std::vector<nacl::string>* command);

  // Sets up the command channel |command| and sends the SRPC to load |nexe|.
  bool SetupCommandAndLoad(NaClSrpcChannel* command,
                           DescWrapper* nexe);

  // Sends the SRPC to start the nexe over |command| and sets up the application
  // SRPC chanel |out_app_chan|.
  bool StartModuleAndSetupAppChannel(NaClSrpcChannel* command,
                                     NaClSrpcChannel* out_app_chan);

  // Add a prefix shell program, like 'time', to run the sel_ldr in
  // This is primarily intended to provide a hook for qemu emulation
  void SetCommandPrefix(const nacl::string& prefix);

  // Kill the child process.  The channel() remains valid, but nobody
  // is talking on the other end.  Returns true if successful.
  bool KillChildProcess();

  // User is responsible for invoking channel() and then taking
  // ownership of the handle prior to the Dtor firing.
  Handle channel() const { return channel_; }

  // Returns the socket address used to connect to the sel_ldr.
  DescWrapper* socket_addr() const { return socket_addr_.get(); }

  // Wraps a raw NaClDesc descriptor.  If NULL is returned, caller retains
  // ownership of the reference.
  DescWrapper* Wrap(NaClDesc* raw_desc);

  // As above, but raw_desc is Unref'd on failure.
  DescWrapper* WrapCleanup(NaClDesc* raw_desc);

  /////////////////////////////////////////////////////////////////////////////
  // Start the sel_ldr process.  No nexe information is passed at this point.
  // It will be supplied over RPC after start-up.
  /////////////////////////////////////////////////////////////////////////////

  bool Start(int socket_count, Handle* result_sockets);

 private:
  // OpenSrpcChannels is essentially the following sequence of
  // (lower-level) operations.

  // BEGIN EQUIVALENT SEQUENCE

  bool SetupBootstrapChannel();
  bool GetLdrSocketAddress();
  bool SetupCommandChannel(NaClSrpcChannel* command);

  // LoadModule supplies, via the |command| channel, the |nexe| file
  // for the service runtime to load.
  bool LoadModule(NaClSrpcChannel* command, DescWrapper* nexe);

  // ----

  // Tell the service runtime to start the NaCl module via the
  // |command| channel.  If |error| is non-NULL, any failures will
  // cause the error code to be written there.
  bool StartModule(NaClSrpcChannel* command, NaClErrorCode* error);

  bool SetupApplicationChannel(NaClSrpcChannel* app_channel);

  // END EQUIVALENT SEQUENCE

  void GetPluginDirectory(char* buffer, size_t len);
  nacl::string GetSelLdrPathName();
  void CloseHandlesAfterLaunch();

  Handle child_process_;
  Handle channel_;
  int channel_number_;  // IMC file descriptor.

  // The following members are used to initialize and build the command line.
  // The detailed magic is in BuildCommandLine() but roughly we run
  // <prefix> <sel_ldr> <extra stuff> -f <nexe> <sel_ldr_argv> -- <nexe_args>
  // Path to prefix tool or empty if not used
  nacl::string command_prefix_;
  nacl::string sel_ldr_bootstrap_;
  // Path to the sel_ldr executable
  nacl::string sel_ldr_;
  // arguments to sel_ldr
  std::vector<nacl::string> sel_ldr_argv_;
  // arguments to the nexe
  std::vector<nacl::string> application_argv_;

  std::vector<Handle> close_after_launch_;

  // lifetime of bootstrap_socket_ must be at least that of factory_
  scoped_ptr<DescWrapperFactory> factory_;
  scoped_ptr<DescWrapper> bootstrap_socket_;
  // The socket address returned from sel_ldr for connects.
  scoped_ptr<DescWrapper> socket_addr_;
  scoped_ptr<SelLdrLocator> sel_ldr_locator_;
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_H_
