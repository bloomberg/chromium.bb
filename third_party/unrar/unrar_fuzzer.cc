// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ftw.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <fstream>
#include <memory>
#include <string>

#include "rar.hpp"

static int removeFile(const char* fpath,
                      const struct stat* sb,
                      int typeflag,
                      struct FTW* ftwbuf) {
  if (remove(fpath) != 0)
    abort();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // unrar likes to create files in the current directory.
  // So, the following few lines created and 'cd' to a directory named 'o'
  // in the current working directory.
  constexpr size_t kMaxCwdLength = 10000;
  static char cwd[kMaxCwdLength];
  if (!getcwd(cwd, sizeof(cwd)))
    abort();

  std::string original_path = std::string(cwd, strlen(cwd));
  std::string out_path = original_path + "/o";

  if (!(opendir(out_path.c_str()) || mkdir(out_path.c_str(), 0700) == 0))
    abort();
  if (chdir(out_path.c_str()) != 0)
    abort();

  static const std::string filename = "temp.rar";
  std::ofstream file(filename,
                     std::ios::binary | std::ios::out | std::ios::trunc);
  if (!file.is_open())
    abort();
  file.write(reinterpret_cast<const char*>(data), size);
  file.close();

  std::unique_ptr<CommandData> cmd_data(new CommandData);
  cmd_data->ParseArg(const_cast<wchar_t*>(L"-p"));
  cmd_data->ParseArg(const_cast<wchar_t*>(L"x"));
  cmd_data->ParseDone();
  std::wstring wide_filename(filename.begin(), filename.end());
  cmd_data->AddArcName(wide_filename.c_str());

  try {
    CmdExtract extractor(cmd_data.get());
    extractor.DoExtract();
  } catch (...) {
  }

  // 'cd' back to the original directory and delete 'o' along with
  // all its contents.
  if (chdir(original_path.c_str()) != 0)
    abort();
  if (nftw(out_path.c_str(), removeFile, 20,
           FTW_DEPTH | FTW_MOUNT | FTW_PHYS) != 0)
    abort();
  return 0;
}
