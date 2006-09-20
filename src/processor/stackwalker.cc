// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// stackwalker.cc: Generic stackwalker.
//
// See stackwalker.h for documentation.
//
// Author: Mark Mentovai


#include <memory>

#include "processor/stackwalker.h"
#include "processor/minidump.h"
#include "processor/source_line_resolver.h"
#include "google/symbol_supplier.h"


namespace google_airbag {


using std::auto_ptr;


Stackwalker::Stackwalker(MemoryRegion* memory, MinidumpModuleList* modules,
                         SymbolSupplier* supplier)
    : memory_(memory),
      modules_(modules),
      supplier_(supplier) {
}


void Stackwalker::Walk(StackFrames *frames) {
  frames->clear();
  SourceLineResolver resolver;

  // Begin with the context frame, and keep getting callers until there are
  // no more.

  auto_ptr<StackFrame> frame(new StackFrame());
  bool valid = GetContextFrame(frame.get());
  while (valid) {
    // frame already contains a good frame with properly set instruction and
    // frame_pointer fields.  The frame structure comes from either the
    // context frame (above) or a caller frame (below).

    // Resolve the module information, if a module map was provided.
    if (modules_) {
      MinidumpModule* module =
          modules_->GetModuleForAddress(frame->instruction);
      if (module) {
        frame->module_name = *(module->GetName());
        frame->module_base = module->base_address();
        if (modules_ && supplier_) {
          string symbol_file =
            supplier_->GetSymbolFile(module);
          if (!symbol_file.empty()) {
            resolver.LoadModule(*(module->GetName()), symbol_file);
            resolver.FillSourceLineInfo(frame.get());
          }
        }
      }
    }

    // Copy the frame into the frames vector.
    frames->push_back(*frame);

    // Use a new object for the next frame, even though the old object was
    // copied.  If StackFrame provided some sort of Clear() method, then
    // the same frame could be reused.
    frame.reset(new StackFrame());

    // Get the next frame.
    valid = GetCallerFrame(frame.get());
  }
}


} // namespace google_airbag
