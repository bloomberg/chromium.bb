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

#ifndef GOOGLE_MINIDUMP_PROCESSOR_H__
#define GOOGLE_MINIDUMP_PROCESSOR_H__

#include <string>
#include "google/stack_frame.h"

namespace google_airbag {

using std::string;

class SymbolSupplier;

class MinidumpProcessor {
 public:
  // Initializes this MinidumpProcessor.  supplier should be an
  // implementation of the SymbolSupplier abstract base class.
  MinidumpProcessor(SymbolSupplier *supplier);
  ~MinidumpProcessor();

  // Fills in the given StackFrames vector by processing the minidump file.
  // supplier_data is an opaque pointer which is passed to
  // SymbolSupplier::GetSymbolFile().  Returns true on success.
  bool Process(const string &minidump_file, void *supplier_data,
               StackFrames *stack_frames);

 private:
  SymbolSupplier *supplier_;
};

}  // namespace google_airbag

#endif  // GOOGLE_MINIDUMP_PROCESSOR_H__
