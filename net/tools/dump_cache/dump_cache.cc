// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This command-line program dumps the contents of a set of cache files, either
// to stdout or to another set of cache files.

#include <stdio.h>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/disk_cache/blockfile/disk_format.h"
#include "net/tools/dump_cache/dump_files.h"
#include "net/tools/dump_cache/simple_cache_dumper.h"

enum Errors {
  GENERIC = -1,
  ALL_GOOD = 0,
  INVALID_ARGUMENT = 1,
  FILE_ACCESS_ERROR,
  UNKNOWN_VERSION,
  TOOL_NOT_FOUND,
};

// Folders to read and write cache files.
const char kInputPath[] = "input";
const char kOutputPath[] = "output";

// Dumps the file headers to stdout.
const char kDumpHeaders[] = "dump-headers";

// Dumps all entries to stdout.
const char kDumpContents[] = "dump-contents";

// Convert the cache to files.
const char kDumpToFiles[] = "dump-to-files";

int Help() {
  printf("warning: input files are modified by this tool\n");
  printf("dump_cache --input=path1 [--output=path2]\n");
  printf("--dump-headers: display file headers\n");
  printf("--dump-contents: display all entries\n");
  printf("--dump-to-files: write the contents of the cache to files\n");
  return INVALID_ARGUMENT;
}

// -----------------------------------------------------------------------

int main(int argc, const char* argv[]) {
  // Setup an AtExitManager so Singleton objects will be destroyed.
  base::AtExitManager at_exit_manager;

  base::CommandLine::Init(argc, argv);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  base::FilePath input_path = command_line.GetSwitchValuePath(kInputPath);
  if (input_path.empty())
    return Help();

  bool dump_to_files = command_line.HasSwitch(kDumpToFiles);

  base::FilePath output_path = command_line.GetSwitchValuePath(kOutputPath);
  if (dump_to_files && output_path.empty())
    return Help();

  int version = GetMajorVersion(input_path);
  if (!version)
    return FILE_ACCESS_ERROR;

  if (dump_to_files) {
    net::SimpleCacheDumper dumper(input_path, output_path);
    dumper.Run();
    return ALL_GOOD;
  }

  if (command_line.HasSwitch(kDumpContents))
    return DumpContents(input_path);

  if (command_line.HasSwitch(kDumpHeaders))
    return DumpHeaders(input_path);

  return Help();
}
