// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This binary tool lists the boxes of any box-based format (JPEG XL,
// JPEG 2000, MP4, ...).
// This exists as a test for manual verification, rather than an actual tool.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lib/jxl/base/file_io.h"
#include "lib/jxl/base/override.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/status.h"
#include "tools/box/box.h"

namespace jpegxl {
namespace tools {

int RunMain(int argc, const char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <filename>", argv[0]);
    return 1;
  }

  jxl::PaddedBytes compressed;
  if (!jxl::ReadFile(argv[1], &compressed)) return 1;
  fprintf(stderr, "Read %zu compressed bytes\n", compressed.size());

  const uint8_t* in = compressed.data();
  size_t available_in = compressed.size();

  fprintf(stderr, "File size: %zu\n", compressed.size());

  while (available_in != 0) {
    const uint8_t* start = in;
    Box box;
    if (!ParseBoxHeader(&in, &available_in, &box)) {
      fprintf(stderr, "Failed at %zu\n", compressed.size() - available_in);
      break;
    }

    size_t data_size = box.data_size_given ? box.data_size : available_in;
    size_t header_size = in - start;
    size_t box_size = header_size + data_size;

    for (size_t i = 0; i < sizeof(box.type); i++) {
      char c = box.type[i];
      if (c < 32 || c > 127) {
        printf("Unprintable character in box type, likely not a box file.\n");
        return 0;
      }
    }

    printf("box: \"%.4s\" box_size:%zu data_size:%zu", box.type, box_size,
           data_size);
    if (!memcmp("uuid", box.type, 4)) {
      printf(" -- extended type:\"%.16s\"", box.extended_type);
    }
    if (!memcmp("ftyp", box.type, 4) && data_size > 4) {
      std::string ftype(in, in + 4);
      printf(" -- ftype:\"%s\"", ftype.c_str());
    }
    printf("\n");

    if (data_size > available_in) {
      fprintf(stderr, "Unexpected end of file %zu %zu %zu\n",
              static_cast<size_t>(box.data_size), available_in,
              compressed.size());
      break;
    }

    in += data_size;
    available_in -= data_size;
  }

  return 0;
}

}  // namespace tools
}  // namespace jpegxl

int main(int argc, const char* argv[]) {
  return jpegxl::tools::RunMain(argc, argv);
}
