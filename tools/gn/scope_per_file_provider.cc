// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/scope_per_file_provider.h"

#include "tools/gn/filesystem_utils.h"
#include "tools/gn/settings.h"
#include "tools/gn/source_file.h"
#include "tools/gn/toolchain_manager.h"
#include "tools/gn/value.h"
#include "tools/gn/variables.h"

ScopePerFileProvider::ScopePerFileProvider(Scope* scope)
    : ProgrammaticProvider(scope) {
}

ScopePerFileProvider::~ScopePerFileProvider() {
}

const Value* ScopePerFileProvider::GetProgrammaticValue(
    const base::StringPiece& ident) {
  if (ident == variables::kCurrentToolchain)
    return GetCurrentToolchain();
  if (ident == variables::kDefaultToolchain)
    return GetDefaultToolchain();
  if (ident == variables::kPythonPath)
    return GetPythonPath();

  if (ident == variables::kRootBuildDir)
    return GetRootBuildDir();
  if (ident == variables::kRootGenDir)
    return GetRootGenDir();
  if (ident == variables::kRootOutDir)
    return GetRootOutDir();
  if (ident == variables::kTargetGenDir)
    return GetTargetGenDir();
  if (ident == variables::kTargetOutDir)
    return GetTargetOutDir();
  return NULL;
}

const Value* ScopePerFileProvider::GetCurrentToolchain() {
  if (!current_toolchain_) {
    current_toolchain_.reset(new Value(NULL,
        scope_->settings()->toolchain_label().GetUserVisibleName(false)));
  }
  return current_toolchain_.get();
}

const Value* ScopePerFileProvider::GetDefaultToolchain() {
  if (!default_toolchain_) {
    const ToolchainManager& toolchain_manager =
        scope_->settings()->build_settings()->toolchain_manager();
    default_toolchain_.reset(new Value(NULL,
        toolchain_manager.GetDefaultToolchainUnlocked().GetUserVisibleName(
            false)));
  }
  return default_toolchain_.get();
}

const Value* ScopePerFileProvider::GetPythonPath() {
  if (!python_path_) {
    python_path_.reset(new Value(NULL,
        FilePathToUTF8(scope_->settings()->build_settings()->python_path())));
  }
  return python_path_.get();
}

const Value* ScopePerFileProvider::GetRootBuildDir() {
  if (!root_build_dir_) {
    root_build_dir_.reset(new Value(NULL,
        "/" + GetRootOutputDirWithNoLastSlash(scope_->settings())));
  }
  return root_build_dir_.get();
}

const Value* ScopePerFileProvider::GetRootGenDir() {
  if (!root_gen_dir_) {
    root_gen_dir_.reset(new Value(NULL,
        "/" + GetToolchainGenDirWithNoLastSlash(scope_->settings())));
  }
  return root_gen_dir_.get();
}

const Value* ScopePerFileProvider::GetRootOutDir() {
  if (!root_out_dir_) {
    root_out_dir_.reset(new Value(NULL,
        "/" + GetToolchainOutputDirWithNoLastSlash(scope_->settings())));
  }
  return root_out_dir_.get();
}

const Value* ScopePerFileProvider::GetTargetGenDir() {
  if (!target_gen_dir_) {
    target_gen_dir_.reset(new Value(NULL,
        "/" +
        GetToolchainGenDirWithNoLastSlash(scope_->settings()) +
        GetFileDirWithNoLastSlash()));
  }
  return target_gen_dir_.get();
}

const Value* ScopePerFileProvider::GetTargetOutDir() {
  if (!target_out_dir_) {
    target_out_dir_.reset(new Value(NULL,
        "/" +
        GetToolchainOutputDirWithNoLastSlash(scope_->settings()) + "/obj" +
        GetFileDirWithNoLastSlash()));
  }
  return target_out_dir_.get();
}

// static
std::string ScopePerFileProvider::GetRootOutputDirWithNoLastSlash(
    const Settings* settings) {
  const std::string& output_dir =
      settings->build_settings()->build_dir().value();

  if (output_dir == "//")
    return "//.";

  // Trim off a leading and trailing slash. So "//foo/bar/" -> /foo/bar".
  DCHECK(output_dir.size() > 2 && output_dir[0] == '/' &&
         output_dir[output_dir.size() - 1] == '/');
  return output_dir.substr(1, output_dir.size() - 2);
}

// static
std::string ScopePerFileProvider::GetToolchainOutputDirWithNoLastSlash(
    const Settings* settings) {
  const OutputFile& toolchain_subdir = settings->toolchain_output_subdir();

  std::string result;
  if (toolchain_subdir.value().empty()) {
    result = GetRootOutputDirWithNoLastSlash(settings);
  } else {
    // The toolchain subdir ends in a slash, trim it.
    result = GetRootOutputDirWithNoLastSlash(settings) + "/" +
        toolchain_subdir.value();
    DCHECK(toolchain_subdir.value()[toolchain_subdir.value().size() - 1] ==
           '/');
    result.resize(result.size() - 1);
  }
  return result;
}

// static
std::string ScopePerFileProvider::GetToolchainGenDirWithNoLastSlash(
    const Settings* settings) {
  return GetToolchainOutputDirWithNoLastSlash(settings) + "/gen";
}

std::string ScopePerFileProvider::GetFileDirWithNoLastSlash() const {
  const std::string& dir_value = scope_->GetSourceDir().value();

  if (dir_value == "//")
    return "//.";

  // Trim off a leading and trailing slash. So "//foo/bar/" -> /foo/bar".
  DCHECK(dir_value.size() > 2 && dir_value[0] == '/' &&
         dir_value[dir_value.size() - 1] == '/');
  return dir_value.substr(1, dir_value.size() - 2);
}
