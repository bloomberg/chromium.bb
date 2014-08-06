// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_OUTPUT_FILE_H_
#define TOOLS_GN_OUTPUT_FILE_H_

#include <string>

#include "base/containers/hash_tables.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/source_file.h"

// A simple wrapper around a string that indicates the string is a path
// relative to the output directory.
class OutputFile {
 public:
  OutputFile() : value_() {}
  explicit OutputFile(const base::StringPiece& str)
      : value_(str.data(), str.size()) {
  }

  std::string& value() { return value_; }
  const std::string& value() const { return value_; }

  // Converts to a SourceFile by prepending the build directory to the file.
  SourceFile GetSourceFile(const BuildSettings* build_settings) const {
    return SourceFile(build_settings->build_dir().value() + value_);
  }

  bool operator==(const OutputFile& other) const {
    return value_ == other.value_;
  }
  bool operator!=(const OutputFile& other) const {
    return value_ != other.value_;
  }
  bool operator<(const OutputFile& other) const {
    return value_ < other.value_;
  }

 private:
  std::string value_;
};

namespace BASE_HASH_NAMESPACE {

#if defined(COMPILER_GCC)
template<> struct hash<OutputFile> {
  std::size_t operator()(const OutputFile& v) const {
    hash<std::string> h;
    return h(v.value());
  }
};
#elif defined(COMPILER_MSVC)
inline size_t hash_value(const OutputFile& v) {
  return hash_value(v.value());
}
#endif  // COMPILER...

}  // namespace BASE_HASH_NAMESPACE

inline void swap(OutputFile& lhs, OutputFile& rhs) {
  lhs.value().swap(rhs.value());
}

#endif  // TOOLS_GN_OUTPUT_FILE_H_
