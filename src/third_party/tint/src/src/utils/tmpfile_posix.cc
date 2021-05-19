// Copyright 2021 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/utils/tmpfile.h"

#include <unistd.h>

namespace tint {
namespace utils {

namespace {

std::string TmpFilePath() {
  char name[] = "tint_XXXXXX";
  int file = mkstemp(name);
  if (file != -1) {
    close(file);
    return name;
  }
  return "";
}

}  // namespace

TmpFile::TmpFile() : path_(TmpFilePath()) {}

TmpFile::~TmpFile() {
  if (!path_.empty()) {
    remove(path_.c_str());
  }
}

bool TmpFile::Append(const void* data, size_t size) const {
  if (auto* file = fopen(path_.c_str(), "ab")) {
    fwrite(data, size, 1, file);
    fclose(file);
    return true;
  }
  return false;
}

}  // namespace utils
}  // namespace tint
