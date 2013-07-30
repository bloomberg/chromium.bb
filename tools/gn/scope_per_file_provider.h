// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SCOPE_PER_FILE_PROVIDER_H_
#define TOOLS_GN_SCOPE_PER_FILE_PROVIDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "tools/gn/scope.h"
#include "tools/gn/source_file.h"

// ProgrammaticProvider for a scope to provide it with per-file built-in
// variable support.
class ScopePerFileProvider : public Scope::ProgrammaticProvider {
 public:
  ScopePerFileProvider(Scope* scope, const SourceFile& source_file);
  virtual ~ScopePerFileProvider();

  // ProgrammaticProvider implementation.
  virtual const Value* GetProgrammaticValue(
      const base::StringPiece& ident) OVERRIDE;

  // Returns the value to expose to script for the given thing. These values
  // are acually set globally, but are put here so we can keep all logic
  // for converting paths to built-in values in this one file.
  static Value GetRootOutputDir(const Settings* settings);
  static Value GetRootGenDir(const Settings* settings);

  // Variable names. These two should be set globally independent of the file
  // being processed.
  static const char* kRootOutputDirName;
  static const char* kRootGenDirName;

  // Variable names. These are provided by this class as needed.
  static const char* kDefaultToolchain;
  static const char* kPythonPath;
  static const char* kToolchain;
  static const char* kTargetOutputDirName;
  static const char* kTargetGenDirName;
  static const char* kRelativeRootOutputDirName;
  static const char* kRelativeRootGenDirName;
  static const char* kRelativeTargetOutputDirName;
  static const char* kRelativeTargetGenDirName;

 private:
  const Value* GetDefaultToolchain();
  const Value* GetPythonPath();
  const Value* GetToolchain();
  const Value* GetTargetOutputDir();
  const Value* GetTargetGenDir();
  const Value* GetRelativeRootOutputDir();
  const Value* GetRelativeRootGenDir();
  const Value* GetRelativeTargetOutputDir();
  const Value* GetRelativeTargetGenDir();

  static std::string GetRootOutputDirWithNoLastSlash(const Settings* settings);
  static std::string GetRootGenDirWithNoLastSlash(const Settings* settings);

  std::string GetFileDirWithNoLastSlash() const;
  std::string GetRelativeRootWithNoLastSlash() const;

  SourceFile source_file_;

  // All values are lazily created.
  scoped_ptr<Value> default_toolchain_;
  scoped_ptr<Value> python_path_;
  scoped_ptr<Value> toolchain_;
  scoped_ptr<Value> target_output_dir_;
  scoped_ptr<Value> target_gen_dir_;
  scoped_ptr<Value> relative_root_output_dir_;
  scoped_ptr<Value> relative_root_gen_dir_;
  scoped_ptr<Value> relative_target_output_dir_;
  scoped_ptr<Value> relative_target_gen_dir_;

  DISALLOW_COPY_AND_ASSIGN(ScopePerFileProvider);
};

#endif  // TOOLS_GN_SCOPE_PER_FILE_PROVIDER_H_
