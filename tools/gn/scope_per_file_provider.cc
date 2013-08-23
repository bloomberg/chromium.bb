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

  if (ident == variables::kRelativeBuildToSourceRootDir)
    return GetRelativeBuildToSourceRootDir();
  if (ident == variables::kRelativeRootOutputDir)
    return GetRelativeRootOutputDir();
  if (ident == variables::kRelativeRootGenDir)
    return GetRelativeRootGenDir();
  if (ident == variables::kRelativeSourceRootDir)
    return GetRelativeSourceRootDir();
  if (ident == variables::kRelativeTargetOutputDir)
    return GetRelativeTargetOutputDir();
  if (ident == variables::kRelativeTargetGenDir)
    return GetRelativeTargetGenDir();
  if (ident == variables::kRootGenDir)
    return GetRootGenDir();
  if (ident == variables::kTargetGenDir)
    return GetTargetGenDir();
  return NULL;
}

const Value* ScopePerFileProvider::GetCurrentToolchain() {
  if (!current_toolchain_) {
    current_toolchain_.reset(new Value(NULL,
        scope_->settings()->toolchain()->label().GetUserVisibleName(false)));
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

const Value* ScopePerFileProvider::GetRelativeBuildToSourceRootDir() {
  if (!relative_build_to_source_root_dir_) {
    const SourceDir& build_dir =
        scope_->settings()->build_settings()->build_dir();
    relative_build_to_source_root_dir_.reset(
        new Value(NULL, InvertDirWithNoLastSlash(build_dir)));
  }
  return relative_build_to_source_root_dir_.get();
}

const Value* ScopePerFileProvider::GetRelativeRootOutputDir() {
  if (!relative_root_output_dir_) {
    relative_root_output_dir_.reset(new Value(NULL,
        GetRelativeRootWithNoLastSlash() +
        GetRootOutputDirWithNoLastSlash(scope_->settings())));
  }
  return relative_root_output_dir_.get();
}

const Value* ScopePerFileProvider::GetRelativeRootGenDir() {
  if (!relative_root_gen_dir_) {
    relative_root_gen_dir_.reset(new Value(NULL,
        GetRelativeRootWithNoLastSlash() +
        GetRootGenDirWithNoLastSlash(scope_->settings())));
  }
  return relative_root_gen_dir_.get();
}

const Value* ScopePerFileProvider::GetRelativeSourceRootDir() {
  if (!relative_source_root_dir_) {
    relative_source_root_dir_.reset(new Value(NULL,
        GetRelativeRootWithNoLastSlash()));
  }
  return relative_source_root_dir_.get();
}

const Value* ScopePerFileProvider::GetRelativeTargetOutputDir() {
  if (!relative_target_output_dir_) {
    relative_target_output_dir_.reset(new Value(NULL,
        GetRelativeRootWithNoLastSlash() +
        GetRootOutputDirWithNoLastSlash(scope_->settings()) + "/obj" +
        GetFileDirWithNoLastSlash()));
  }
  return relative_target_output_dir_.get();
}

const Value* ScopePerFileProvider::GetRelativeTargetGenDir() {
  if (!relative_target_gen_dir_) {
    relative_target_gen_dir_.reset(new Value(NULL,
        GetRelativeRootWithNoLastSlash() +
        GetRootGenDirWithNoLastSlash(scope_->settings()) +
        GetFileDirWithNoLastSlash()));
  }
  return relative_target_gen_dir_.get();
}

const Value* ScopePerFileProvider::GetRootGenDir() {
  if (!root_gen_dir_) {
    root_gen_dir_.reset(new Value(NULL,
        "/" + GetRootGenDirWithNoLastSlash(scope_->settings())));
  }
  return root_gen_dir_.get();
}

const Value* ScopePerFileProvider::GetTargetGenDir() {
  if (!target_gen_dir_) {
    target_gen_dir_.reset(new Value(NULL,
        "/" +
        GetRootGenDirWithNoLastSlash(scope_->settings()) +
        GetFileDirWithNoLastSlash()));
  }
  return target_gen_dir_.get();
}

// static
std::string ScopePerFileProvider::GetRootOutputDirWithNoLastSlash(
    const Settings* settings) {
  const std::string& output_dir =
      settings->build_settings()->build_dir().value();

  // Trim off a leading and trailing slash. So "//foo/bar/" -> /foo/bar".
  DCHECK(output_dir.size() > 2 && output_dir[0] == '/' &&
         output_dir[output_dir.size() - 1] == '/');
  return output_dir.substr(1, output_dir.size() - 2);
}

// static
std::string ScopePerFileProvider::GetRootGenDirWithNoLastSlash(
    const Settings* settings) {
  return GetRootOutputDirWithNoLastSlash(settings) + "/gen";
}

std::string ScopePerFileProvider::GetFileDirWithNoLastSlash() const {
  const std::string& dir_value = scope_->GetSourceDir().value();

  // Trim off a leading and trailing slash. So "//foo/bar/" -> /foo/bar".
  DCHECK(dir_value.size() > 2 && dir_value[0] == '/' &&
         dir_value[dir_value.size() - 1] == '/');
  return dir_value.substr(1, dir_value.size() - 2);
}

std::string ScopePerFileProvider::GetRelativeRootWithNoLastSlash() const {
  return InvertDirWithNoLastSlash(scope_->GetSourceDir());
}

// static
std::string ScopePerFileProvider::InvertDirWithNoLastSlash(
    const SourceDir& dir) {
  std::string inverted = InvertDir(dir);
  if (inverted.empty())
    return ".";
  return inverted.substr(0, inverted.size() - 1);
}
