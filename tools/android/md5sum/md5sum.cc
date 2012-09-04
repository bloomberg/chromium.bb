// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Md5sum implementation for Android. This version handles files as well as
// directories. Its output is sorted by file path.

#include <fstream>
#include <iostream>
#include <set>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/md5.h"

namespace {

const int kBufferSize = 1024;

// Returns whether |path|'s MD5 was successfully written to |digest_string|.
bool MD5Sum(const char* path, std::string* digest_string) {
  std::ifstream stream(path);
  if (!stream.good()) {
    LOG(ERROR) << "Could not open file " << path;
    return false;
  }
  base::MD5Context ctx;
  base::MD5Init(&ctx);
  char buf[kBufferSize];
  while (stream.good()) {
    std::streamsize bytes_read = stream.readsome(buf, sizeof(buf));
    if (bytes_read == 0)
      break;
    base::MD5Update(&ctx, base::StringPiece(buf, bytes_read));
  }
  if (stream.fail()) {
    LOG(ERROR) << "Error reading file " << path;
    return false;
  }
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  *digest_string = base::MD5DigestToBase16(digest);
  return true;
}

// Returns the set of all files contained in |files|. This handles directories
// by walking them recursively.
std::set<std::string> MakeFileSet(const char** files) {
  std::set<std::string> file_set;
  for (const char** file = files; *file; ++file) {
    FilePath file_path(*file);
    if (file_util::DirectoryExists(file_path)) {
      file_util::FileEnumerator file_enumerator(
          file_path, true /* recurse */, file_util::FileEnumerator::FILES);
      for (FilePath child, empty; (child = file_enumerator.Next()) != empty; ) {
        file_util::AbsolutePath(&child);
        file_set.insert(child.value());
      }
    } else {
      file_set.insert(*file);
    }
  }
  return file_set;
}

}  // namespace

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    LOG(ERROR) << "Usage: md5sum <path/to/file_or_dir>...";
    return 1;
  }
  const std::set<std::string> files = MakeFileSet(argv + 1);
  bool failed = false;
  std::string digest;
  for (std::set<std::string>::const_iterator it = files.begin();
       it != files.end(); ++it) {
    if (!MD5Sum(it->c_str(), &digest))
      failed = true;
    FilePath file_path(*it);
    file_util::AbsolutePath(&file_path);
    std::cout << digest << "  " << file_path.value() << std::endl;
  }
  return failed;
}
