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


// Utility class for launching sel_ldr.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_H_

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/imc/nacl_imc.h"

struct NaClSrpcChannel;
struct NaClDesc;

namespace nacl {

/*
 * This class encapsulates the process of launching an instance of sel_ldr
 * to communicate over an IMC channel.
 */
struct SelLdrLauncher {
 public:
  explicit SelLdrLauncher();

  bool Start(const char* application_name,
             int imc_fd,
             int sel_ldr_argc,
             const char* sel_ldr_argv[],
             int application_argc,
             const char* application_argv[]);
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

  const char* GetApplicationName() const { return application_name_.c_str(); }

  static const char* GetPluginDirname();

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
  // application_argv can take a "$CHAN" as an argument which will be replaced
  // with `imc_fd` number if the target application is a NaCl module
  // and with `channel_` number if the target application is a native OS
  // binary.
  void BuildArgv(const char* sel_ldr_pathname,
                 const char* application_name,
                 int imc_fd,
                 Handle imc_channel_handle,
                 int sel_ldr_argc,
                 const char* sel_ldr_argv[],
                 int application_argc,
                 const char* application_argv[]);

  // If subprocess creation fails, both child_ and channel_  are set to
  // kInvalidHandle.
 public:
  Handle child_;
  Handle channel_;
 private:
  int argc_;
  char const** argv_;
  // The following buffer is used as an argv element for mapping the imc
  // channel to a nacl descriptor.
  char channel_buf_[23];
  // The following buffer is used to save a string that replaces an application
  // argument "$CHAN".
  char channel_number_[19];
  // The following boolean value is set to true if SelLdrLauncher has launched
  // an instance of sel_ldr, or false if child_ is executing a native OS binary
  // instead of sel_ldr for debugging.
  bool is_sel_ldr_;
  std::string application_name_;
  // The socket address returned from sel_ldr for connects.
  struct NaClDesc* sock_addr_;
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_H_
