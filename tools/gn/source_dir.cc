// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/source_dir.h"

#include "base/logging.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/source_file.h"

namespace {

void AssertValueSourceDirString(const std::string& s) {
  if (!s.empty()) {
    DCHECK(s[0] == '/');
    DCHECK(EndsWithSlash(s));
  }
}

}  // namespace

SourceDir::SourceDir() {
}

SourceDir::SourceDir(const base::StringPiece& p)
    : value_(p.data(), p.size()) {
  if (!EndsWithSlash(value_))
    value_.push_back('/');
  AssertValueSourceDirString(value_);
}

SourceDir::SourceDir(SwapIn, std::string* s) {
  value_.swap(*s);
  if (!EndsWithSlash(value_))
    value_.push_back('/');
  AssertValueSourceDirString(value_);
}

SourceDir::~SourceDir() {
}

SourceFile SourceDir::ResolveRelativeFile(
    const base::StringPiece& p,
    const base::StringPiece& source_root) const {
  SourceFile ret;

  // It's an error to resolve an empty string or one that is a directory
  // (indicated by a trailing slash) because this is the function that expects
  // to return a file.
  if (p.empty() || (p.size() > 0 && p[p.size() - 1] == '/'))
    return SourceFile();
  if (p.size() >= 2 && p[0] == '/' && p[1] == '/') {
    // Source-relative.
    ret.value_.assign(p.data(), p.size());
    NormalizePath(&ret.value_);
    return ret;
  } else if (IsPathAbsolute(p)) {
    if (source_root.empty() ||
        !MakeAbsolutePathRelativeIfPossible(source_root, p, &ret.value_)) {
#if defined(OS_WIN)
      // On Windows we'll accept "C:\foo" as an absolute path, which we want
      // to convert to "/C:..." here.
      if (p[0] != '/')
        ret.value_ = "/";
#endif
      ret.value_.append(p.data(), p.size());
    }
    NormalizePath(&ret.value_);
    return ret;
  }

  ret.value_.reserve(value_.size() + p.size());
  ret.value_.assign(value_);
  ret.value_.append(p.data(), p.size());

  NormalizePath(&ret.value_);
  return ret;
}

SourceDir SourceDir::ResolveRelativeDir(
    const base::StringPiece& p,
    const base::StringPiece& source_root) const {
  SourceDir ret;

  if (p.empty())
    return ret;
  if (p.size() >= 2 && p[0] == '/' && p[1] == '/') {
    // Source-relative.
    ret.value_.assign(p.data(), p.size());
    if (!EndsWithSlash(ret.value_))
      ret.value_.push_back('/');
    NormalizePath(&ret.value_);
    return ret;
  } else if (IsPathAbsolute(p)) {
    if (source_root.empty() ||
        !MakeAbsolutePathRelativeIfPossible(source_root, p, &ret.value_)) {
#if defined(OS_WIN)
      if (p[0] != '/')  // See the file case for why we do this check.
        ret.value_ = "/";
#endif
      ret.value_.append(p.data(), p.size());
    }
    NormalizePath(&ret.value_);
    if (!EndsWithSlash(ret.value_))
      ret.value_.push_back('/');
    return ret;
  }

  ret.value_.reserve(value_.size() + p.size());
  ret.value_.assign(value_);
  ret.value_.append(p.data(), p.size());

  NormalizePath(&ret.value_);
  if (!EndsWithSlash(ret.value_))
    ret.value_.push_back('/');
  AssertValueSourceDirString(ret.value_);

  return ret;
}

base::FilePath SourceDir::Resolve(const base::FilePath& source_root) const {
  if (is_null())
    return base::FilePath();

  std::string converted;
  if (is_system_absolute()) {
    if (value_.size() > 2 && value_[2] == ':') {
      // Windows path, strip the leading slash.
      converted.assign(&value_[1], value_.size() - 1);
    } else {
      converted.assign(value_);
    }
    return base::FilePath(UTF8ToFilePath(converted));
  }

  // String the double-leading slash for source-relative paths.
  converted.assign(&value_[2], value_.size() - 2);
  return source_root.Append(UTF8ToFilePath(converted))
      .NormalizePathSeparatorsTo('/');
}

void SourceDir::SwapValue(std::string* v) {
  value_.swap(*v);
  AssertValueSourceDirString(value_);
}
