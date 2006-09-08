// Copyright (C) 2006 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Windows utility to dump the line number data from a pdb file to
// a text-based format that we can use from the minidump processor.

#include <stdio.h>
#include <string>
#include "pdb_source_line_writer.h"

using std::wstring;

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <pdb file>\n", argv[0]);
    return 1;
  }

  wchar_t filename[_MAX_PATH];
  if (mbstowcs_s(NULL, filename, argv[1], _MAX_PATH) == -1) {
    fprintf(stderr, "invalid multibyte character in %s\n", argv[1]);
    return 1;
  }

  google_airbag::PDBSourceLineWriter writer;
  if (!writer.Open(wstring(filename))) {
    fprintf(stderr, "Open failed\n");
    return 1;
  }

  if (!writer.WriteMap(stdout)) {
    fprintf(stderr, "WriteMap failed\n");
    return 1;
  }

  writer.Close();
  return 0;
}
