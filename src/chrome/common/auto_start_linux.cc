// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/auto_start_linux.h"

#include <stddef.h>

#include <memory>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_tokenizer.h"

namespace {

const base::FilePath::CharType kAutostart[] = "autostart";

base::FilePath GetAutostartDirectory(base::Environment* environment) {
  base::FilePath result = base::nix::GetXDGDirectory(
      environment,
      base::nix::kXdgConfigHomeEnvVar,
      base::nix::kDotConfigDir);
  result = result.Append(kAutostart);
  return result;
}

}  // namespace

bool AutoStart::AddApplication(const std::string& autostart_filename,
                               const std::string& application_name,
                               const std::string& command_line,
                               bool is_terminal_app) {
  std::unique_ptr<base::Environment> environment(base::Environment::Create());
  base::FilePath autostart_directory = GetAutostartDirectory(environment.get());
  if (!base::DirectoryExists(autostart_directory) &&
      !base::CreateDirectory(autostart_directory)) {
    return false;
  }

  base::FilePath autostart_file =
      autostart_directory.Append(autostart_filename);
  std::string terminal = is_terminal_app ? "true" : "false";
  std::string autostart_file_contents =
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Terminal=" + terminal + "\n"
      "Exec=" + command_line + "\n"
      "Name=" + application_name + "\n";
  std::string::size_type content_length = autostart_file_contents.length();
  if (base::WriteFile(autostart_file, autostart_file_contents.c_str(),
                      content_length) !=
      static_cast<int>(content_length)) {
    base::DeleteFile(autostart_file, false);
    return false;
  }
  return true;
}

bool AutoStart::Remove(const std::string& autostart_filename) {
  std::unique_ptr<base::Environment> environment(base::Environment::Create());
  base::FilePath autostart_directory = GetAutostartDirectory(environment.get());
  base::FilePath autostart_file =
      autostart_directory.Append(autostart_filename);
  return base::DeleteFile(autostart_file, false);
}

bool AutoStart::GetAutostartFileContents(
    const std::string& autostart_filename, std::string* contents) {
  std::unique_ptr<base::Environment> environment(base::Environment::Create());
  base::FilePath autostart_directory = GetAutostartDirectory(environment.get());
  base::FilePath autostart_file =
      autostart_directory.Append(autostart_filename);
  return base::ReadFileToString(autostart_file, contents);
}

bool AutoStart::GetAutostartFileValue(const std::string& autostart_filename,
                                      const std::string& value_name,
                                      std::string* value) {
  std::string contents;
  if (!GetAutostartFileContents(autostart_filename, &contents))
    return false;
  base::StringTokenizer tokenizer(contents, "\n");
  std::string token = value_name + "=";
  while (tokenizer.GetNext()) {
    if (tokenizer.token().substr(0, token.length()) == token) {
      *value = tokenizer.token().substr(token.length());
      return true;
    }
  }
  return false;
}
