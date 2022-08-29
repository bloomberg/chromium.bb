// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/platform/input_stream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace location {
namespace nearby {
namespace {
constexpr size_t kSkipBufferSize = 64 * 1024;
}  // namespace

ExceptionOr<size_t> InputStream::Skip(size_t offset) {
  size_t bytes_left = offset;
  while (bytes_left > 0) {
    size_t chunk_size = std::min(bytes_left, kSkipBufferSize);
    ExceptionOr<ByteArray> result = Read(chunk_size);
    if (!result.ok()) {
      return result.GetException();
    }
    bytes_left -= chunk_size;
  }
  return ExceptionOr<size_t>(offset);
}

}  // namespace nearby
}  // namespace location
