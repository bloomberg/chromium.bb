// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_INPUT_FILE_H_
#define TOOLS_GN_INPUT_FILE_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/source_file.h"

class InputFile {
 public:
  InputFile(const SourceFile& name);

  // Constructor for testing. Uses an empty file path and a given contents.
  //InputFile(const char* contents);
  ~InputFile();

  const SourceFile& name() const { return name_; }

  // The directory is just a cached version of name_->GetDir() but we get this
  // a lot so computing it once up front saves a bunch of work.
  const SourceDir& dir() const { return dir_; }

  const std::string& contents() const {
    DCHECK(contents_loaded_);
    return contents_;
  }

  // For testing and in cases where this input doesn't actually refer to
  // "a file".
  void SetContents(const std::string& c);

  // Loads the given file synchronously, returning true on success. This
  bool Load(const base::FilePath& system_path);

 private:
  SourceFile name_;
  SourceDir dir_;

  bool contents_loaded_;
  std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(InputFile);
};

#endif  // TOOLS_GN_INPUT_FILE_H_
