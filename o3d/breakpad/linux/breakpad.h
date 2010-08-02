/*
 * Copyright 2010, Google Inc.
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


#ifndef O3D_LINUX_BREAKPAD_H_
#define O3D_LINUX_BREAKPAD_H_

#include <string>

#if defined(LINUX)
#include "client/linux/handler/exception_handler.h"
#include "client/linux/minidump_writer/minidump_writer.h"
#include "common/linux/linux_syscall_support.h"
#include "common/linux/google_crashdump_uploader.h"
#endif  // defined(LINUX)

namespace o3d {
class Breakpad {
 public:
  const std::string& product() const { return product_; }
  void set_product(const std::string & product) { product_ = product; }

  const std::string& version() const { return version_; }
  void set_version(const std::string & version) { version_ = version; }

  const std::string& guid() const { return guid_; }
  void set_guid(const std::string & guid) { guid_ = guid; }

  const std::string& email() const { return email_; }
  void set_email(const std::string & email) { email_ = email; }

  const std::string& path() const { return path_; }
  void set_path(const std::string & path) { path_ = path; }

  const std::string& server() const { return server_; }
  void set_server(const std::string & server) { server_ = server; }

  bool Initialize();
  bool Shutdown();

 private:
  static bool BuildDumpFilename(char* filename,
                                int len,
                                const char* dump_path,
                                const char* minidump_id);

  // This is virtual to allow unittest to overwirte it and not sending
  // out minidump file in test.
  virtual bool SendDumpFile(const char* filename);

  // Invoked by friend function BreakpadCallback
  bool OnBreakpadCallback(const char* dump_path,
                          const char* minidump_id,
                          bool succeeded);

  friend bool BreakpadCallback(const char* dump_path,
                               const char* minidump_id,
                               void* context,
                               bool succeeded);

  std::string product_;
  std::string version_;
  std::string guid_;
  std::string ptime_;
  std::string ctime_;
  std::string email_;
  std::string comments_;
  std::string path_;
  std::string server_;
  std::string proxy_;
  std::string passwd_;

  google_breakpad::ExceptionHandler* breakpad_;
};

// Breakpad minidump callback
// A friend function of Breakpad to invoke OnBreakpadCallback.
bool BreakpadCallback(const char* dump_path,
                      const char* minidump_id,
                      void* context,
                      bool succeeded);


#if defined(LINUX)
// This provides a wrapper around system calls which may be
// interrupted by a signal and return EINTR. See man 7 signal.
#define HANDLE_EINTR(x) ({ \
  typeof(x) __eintr_result__; \
  do { \
    __eintr_result__ = x; \
  } while (__eintr_result__ == -1 && errno == EINTR); \
  __eintr_result__;\
})
#endif  // defined(LINUX)

}  // namespace o3d

#endif  // O3D_LINUX_BREAKPAD_H_
