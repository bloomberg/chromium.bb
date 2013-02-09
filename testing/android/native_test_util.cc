// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/android/native_test_util.h"

#include <android/log.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/strings/string_tokenizer.h"

namespace {

const char kLogTag[] = "chromium";

void AndroidLogError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  __android_log_vprint(ANDROID_LOG_ERROR, kLogTag, format, args);
  va_end(args);
}

void ParseArgsFromString(const std::string& command_line,
                         std::vector<std::string>* args) {
  base::StringTokenizer tokenizer(command_line, kWhitespaceASCII);
  tokenizer.set_quote_chars("\"");
  while (tokenizer.GetNext()) {
    std::string token;
    RemoveChars(tokenizer.token(), "\"", &token);
    args->push_back(token);
  }
}

}  // namespace

namespace testing {
namespace native_test_util {

void CreateFIFO(const char* fifo_path) {
  unlink(fifo_path);
  // Default permissions for mkfifo is ignored, chmod is required.
  if (mkfifo(fifo_path, 0666) || chmod(fifo_path, 0666)) {
    AndroidLogError("Failed to create fifo %s: %s\n",
                    fifo_path, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void RedirectStream(
    FILE* stream, const char* path, const char* mode) {
  if (!freopen(path, mode, stream)) {
    AndroidLogError("Failed to redirect stream to file: %s: %s\n",
                    path, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void ParseArgsFromCommandLineFile(
    const char* path, std::vector<std::string>* args) {
  base::FilePath command_line(path);
  std::string command_line_string;
  if (file_util::ReadFileToString(command_line, &command_line_string)) {
    ParseArgsFromString(command_line_string, args);
  }
}

int ArgsToArgv(const std::vector<std::string>& args,
                std::vector<char*>* argv) {
  // We need to pass in a non-const char**.
  int argc = args.size();

  argv->resize(argc + 1);
  for (int i = 0; i < argc; ++i) {
    (*argv)[i] = const_cast<char*>(args[i].c_str());
  }
  (*argv)[argc] = NULL;  // argv must be NULL terminated.

  return argc;
}

}  // namespace native_test_util
}  // namespace testing
