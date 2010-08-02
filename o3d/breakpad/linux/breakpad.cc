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

#include "breakpad.h"

#if defined(LINUX)
#include <sys/stat.h>
#endif  // defined(LINUX)

#include <cstring>

namespace o3d {
#if defined(WIN32)
static const std::string kBreakpadProduct = "Google_O3D_Plugin";
#elif defined(OSX)
static const std::string kBreakpadProduct = "Google_O3D_Plugin_Mac";
#elif defined(LINUX)
static const std::string kBreakpadProduct = "Google_O3D_Plugin_Linux";
#endif
static const std::string kBreakpadVersion = "unknown";
static const std::string kBreakpadGUID = "unknown";
static const std::string kBreakpadEmail = "unknown";
#ifdef _DEBUG
static const std::string kBreakpadServer =
  "http://clients2.google.com/cr/staging_report";
#else
static const std::string kBreakpadServer =
  "http://clients2.google.com/cr/report";
#endif

//TODO(zhurunz): Add/use decent path utility functions.

bool Breakpad::Initialize() {
  if (NULL != breakpad_)
    return true;

  // Set default configuration.
  set_product(kBreakpadProduct);
  set_version(kBreakpadVersion);
  set_guid(kBreakpadGUID);
  set_email(kBreakpadEmail);
  set_path("/tmp/");
  set_server(kBreakpadServer);

  // TODO(zhurunz): consider using HANDLE_CRASHES
  // Setup callback.
  breakpad_ = new
    google_breakpad::ExceptionHandler(path_.c_str(),
                                      NULL,  /* filter callback */
                                      BreakpadCallback,
                                      this,
                                      true  /* install handlers */);

  return (NULL != breakpad_);
}

bool Breakpad::Shutdown() {
  delete breakpad_;
  breakpad_ = NULL;
  return true;
}

#if defined(LINUX)
bool Breakpad::BuildDumpFilename(char* filename,
                                 int len,
                                 const char* dump_path,
                                 const char* minidump_id) {
  const int dump_path_len = strlen(dump_path);
  const int minidump_id_len = strlen(minidump_id);
  int len_needed = dump_path_len + minidump_id_len +
                   4 /* ".dmp" */ + 1 /* NUL */;
  if (len_needed > len)
    return false;

  filename[0] = '\0';
  std::strcat(filename, dump_path);
  std::strcat(filename, minidump_id);
  std::strcat(filename, ".dmp");

  return true;
}

bool Breakpad::SendDumpFile(const char* filename) {
  struct stat st;
  if (stat(filename, &st) != 0 || st.st_size <= 0)
    return false;

  google_breakpad::GoogleCrashdumpUploader
    uploader(product_.c_str(),
             version_.c_str(),
             guid_.c_str(),
             ptime_.c_str(),
             ctime_.c_str(),
             email_.c_str(),
             comments_.c_str(),
             filename,
             server_.c_str(),
             proxy_.c_str(),
             passwd_.c_str());
  return uploader.Upload();
}

bool Breakpad::OnBreakpadCallback(const char* dump_path,
                                  const char* minidump_id,
                                  bool succeeded) {
  // WARNING: this code runs in a compromised context. It may not call into
  // libc nor allocate memory normally.
  if (!succeeded)
    return succeeded;

  // Build dump filename.
  char filename[256];
  if (!BuildDumpFilename(filename, sizeof(filename),
                         dump_path, minidump_id))
    return false;

  // TODO(zhurunz): Change in-proc upload to out-proc upload.
  // TODO(zhurunz): Sync with other teams in ChromeOS and figure out if
  // we could have a GC proc to upload all dumps on ChromeOS/Linux.
  bool ret = SendDumpFile(filename);
  unlink(filename);

  return ret;
}
#endif  // defined(LINUX)

bool BreakpadCallback(const char* dump_path,
                      const char* minidump_id,
                      void* context,
                      bool succeeded) {
  Breakpad* breakpad = reinterpret_cast<Breakpad*>(context);
  if (NULL == breakpad)
    return false;

  return breakpad->OnBreakpadCallback(dump_path, minidump_id, succeeded);
}

}  // namespace o3d
