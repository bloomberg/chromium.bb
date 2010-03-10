/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// Utility class for launching sel_ldr.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_H_

#include <string>
#include <vector>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/imc/nacl_imc.h"

struct NaClSrpcChannel;
struct NaClDesc;

namespace nacl {

/*
 * This class defines an interface that locates sel_ldr.
 */
class SelLdrLocator {
 public:
  // Returns a directory with sel_ldr.
  virtual void GetDirectory(char* buffer, size_t len) = 0;

  SelLdrLocator() {}

  virtual ~SelLdrLocator() { }

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

  PluginSelLdrLocator() {
  }

  DISALLOW_COPY_AND_ASSIGN(PluginSelLdrLocator);
};

/*
 * This class encapsulates the process of launching an instance of sel_ldr
 * to communicate over an IMC channel.
 */
struct SelLdrLauncher {
 public:
  SelLdrLauncher();

  explicit SelLdrLauncher(SelLdrLocator* sel_ldr_locator);

  // TODO(robertm): shouldn't some of these args go in the constructor
  // application_argv can take a "$CHAN" as an argument which will be replaced
  // with `imc_fd` number if the target application is a NaCl module
  // and with `channel_` number if the target application is a native OS
  // binary.

  bool Start(const std::string& application_name,
             int imc_fd,
             const std::vector<std::string>& sel_ldr_argv,
             const std::vector<std::string>& app_argv) {
    Init(application_name, imc_fd, sel_ldr_argv, app_argv);
    return Launch();
  }

  // Obsolete: compatibility interface
  bool Start(const std::string& application_name,
             int imc_fd,
             int sel_ldr_argc,
             const char* sel_ldr_argv[],
             int app_argc,
             const char* app_argv[]) {
    const std::vector<std::string> sel_ldr_vec(sel_ldr_argv,
                                               sel_ldr_argv + sel_ldr_argc);
    const std::vector<std::string> app_vec(app_argv,
                                           app_argv + app_argc);
    return Start(application_name, imc_fd, sel_ldr_vec, app_vec);
  }
  // TODO(gregoryd): add some background story here
#ifdef NACL_STANDALONE
  // Should never be called when not running in Chrome.
  bool Start(const char* url, int imc_fd) {
    UNREFERENCED_PARAMETER(url);
    UNREFERENCED_PARAMETER(imc_fd);
    return false;
  }
#else
  // Launch sel_ldr process in Chrome by sending a message
  // to the browser process.
  bool Start(const char* url, int imc_fd);
#endif
  // OpenSrpcChannels essentially is a triple Ctor for the three
  // NaClSrpcChannel objects; if it returns true (success), all were
  // constructed; if it returns false, none were constructed (and thus
  // none needs to be Dtor'd).
  bool OpenSrpcChannels(NaClSrpcChannel* command,
                        NaClSrpcChannel* untrusted_command,
                        NaClSrpcChannel* untrusted);

  ~SelLdrLauncher();

  Handle child() const { return child_; }

  // User is responsible for invoking channel() and then taking
  // ownership of the handle prior to the Dtor firing.
  Handle channel() const { return channel_; }

  std::string GetApplicationName() const { return application_name_; }

  // Returns the socket address used to connect to the sel_ldr.
  struct NaClDesc* GetSelLdrSocketAddress() const { return sock_addr_; }

  // Returns true if SelLdrLauncher has launched an instance of sel_ldr, or
  // false if SelLdrLauncher has launched a native OS binary instead of sel_ldr
  // for debugging.
  bool IsSelLdr() const { return is_sel_ldr_; }

  // Kill the child process.  The channel() remains valid, but nobody
  // is talking on the other end.  Returns true if successful.
  bool KillChild();

 private:
  void GetPluginDirectory(char* buffer, size_t len);

  std::string GetSelLdrPathName();

  std::string ExpandVar(std::string arg);

  void BuildArgv(std::vector<std::string>* argv);

  // If subprocess creation fails, both child_ and channel_ are set to
  // kInvalidHandle. We have different implementations for unix and win.
  // NOTE: you must call Init() and InitChannelBuf() before Launch()
  bool Launch();

  void Init(const std::string& application_name,
            int imc_fd,
            const std::vector<std::string>& sel_ldr_argv,
            const std::vector<std::string>& application_argv);

  void InitChannelBuf(Handle handle);

 private:
  Handle child_;
  Handle channel_;
  // The following strings and vectors are used by BuildArgv to
  // create a command line, they are initialized by InitBasic().
  std::string channel_number_;
  std::string sel_ldr_;
  std::string application_name_;
  std::vector<std::string> sel_ldr_argv_;
  std::vector<std::string> application_argv_;
  // unlike the others above this is set from within Launch();
  std::string channel_buf_;

  // The socket address returned from sel_ldr for connects.
  struct NaClDesc* sock_addr_;

  // The following boolean value is set to true if SelLdrLauncher has launched
  // an instance of sel_ldr, or false if child_ is executing a native OS binary
  // instead of sel_ldr for debugging.
  bool is_sel_ldr_;

  SelLdrLocator* sel_ldr_locator_;
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_H_
