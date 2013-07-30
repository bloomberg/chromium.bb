// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/scope_per_file_provider.h"

#include "tools/gn/filesystem_utils.h"
#include "tools/gn/settings.h"
#include "tools/gn/source_file.h"
#include "tools/gn/toolchain_manager.h"
#include "tools/gn/value.h"

const char* ScopePerFileProvider::kDefaultToolchain =
    "default_toolchain";
const char* ScopePerFileProvider::kPythonPath =
    "python_path";
const char* ScopePerFileProvider::kToolchain =
    "toolchain";
const char* ScopePerFileProvider::kRootOutputDirName =
    "root_output_dir";
const char* ScopePerFileProvider::kRootGenDirName =
    "root_gen_dir";
const char* ScopePerFileProvider::kTargetOutputDirName =
    "target_output_dir";
const char* ScopePerFileProvider::kTargetGenDirName =
    "target_gen_dir";
const char* ScopePerFileProvider::kRelativeRootOutputDirName =
    "relative_root_output_dir";
const char* ScopePerFileProvider::kRelativeRootGenDirName =
    "relative_root_gen_dir";
const char* ScopePerFileProvider::kRelativeTargetOutputDirName =
    "relative_target_output_dir";
const char* ScopePerFileProvider::kRelativeTargetGenDirName =
    "relative_target_gen_dir";

ScopePerFileProvider::ScopePerFileProvider(Scope* scope,
                                           const SourceFile& source_file)
    : ProgrammaticProvider(scope),
      source_file_(source_file) {
}

ScopePerFileProvider::~ScopePerFileProvider() {
}

const Value* ScopePerFileProvider::GetProgrammaticValue(
    const base::StringPiece& ident) {
  if (ident == kDefaultToolchain)
    return GetDefaultToolchain();
  if (ident == kPythonPath)
    return GetPythonPath();

  if (ident == kTargetOutputDirName)
    return GetTargetOutputDir();
  if (ident == kTargetGenDirName)
    return GetTargetGenDir();

  if (ident == kRelativeRootOutputDirName)
    return GetRelativeRootOutputDir();
  if (ident == kRelativeRootGenDirName)
    return GetRelativeRootGenDir();
  if (ident == kRelativeTargetOutputDirName)
    return GetRelativeTargetOutputDir();
  if (ident == kRelativeTargetGenDirName)
    return GetRelativeTargetGenDir();
  return NULL;
}

// static
Value ScopePerFileProvider::GetRootOutputDir(const Settings* settings) {
  return Value(NULL, GetRootOutputDirWithNoLastSlash(settings));
}

// static
Value ScopePerFileProvider::GetRootGenDir(const Settings* settings) {
  return Value(NULL, GetRootGenDirWithNoLastSlash(settings));
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

const Value* ScopePerFileProvider::GetToolchain() {
  if (!toolchain_) {
    toolchain_.reset(new Value(NULL,
        scope_->settings()->toolchain()->label().GetUserVisibleName(false)));
  }
  return toolchain_.get();
}

const Value* ScopePerFileProvider::GetTargetOutputDir() {
  if (!target_output_dir_) {
    target_output_dir_.reset(new Value(NULL,
        GetRootOutputDirWithNoLastSlash(scope_->settings()) +
        GetFileDirWithNoLastSlash()));
  }
  return target_output_dir_.get();
}

const Value* ScopePerFileProvider::GetTargetGenDir() {
  if (!target_output_dir_) {
    target_gen_dir_.reset(new Value(NULL,
        GetRootGenDirWithNoLastSlash(scope_->settings()) +
        GetFileDirWithNoLastSlash()));
  }
  return target_gen_dir_.get();
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

const Value* ScopePerFileProvider::GetRelativeTargetOutputDir() {
  if (!relative_target_output_dir_) {
    relative_target_output_dir_.reset(new Value(NULL,
        GetRelativeRootWithNoLastSlash() +
        GetRootOutputDirWithNoLastSlash(scope_->settings()) + "obj/" +
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

// static
std::string ScopePerFileProvider::GetRootOutputDirWithNoLastSlash(
    const Settings* settings) {
  const std::string& output_dir =
      settings->build_settings()->build_dir().value();
  CHECK(!output_dir.empty());
  return output_dir.substr(0, output_dir.size() - 1);
}

// static
std::string ScopePerFileProvider::GetRootGenDirWithNoLastSlash(
    const Settings* settings) {
  return GetRootOutputDirWithNoLastSlash(settings) + "/gen";
}

std::string ScopePerFileProvider::GetFileDirWithNoLastSlash() const {
  std::string dir_value = source_file_.GetDir().value();
  return dir_value.substr(0, dir_value.size() - 1);
}

std::string ScopePerFileProvider::GetRelativeRootWithNoLastSlash() const {
  std::string inverted = InvertDir(source_file_.GetDir());
  if (inverted.empty())
    return ".";
  return inverted.substr(0, inverted.size() - 1);
}
