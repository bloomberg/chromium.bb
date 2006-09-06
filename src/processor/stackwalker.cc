// Copyright (C) 2006 Google Inc.
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

// stackwalker.cc: Generic stackwalker.
//
// See stackwalker.h for documentation.
//
// Author: Mark Mentovai


#include <memory>

#include "processor/stackwalker.h"
#include "processor/minidump.h"


namespace google_airbag {


using std::auto_ptr;


Stackwalker::Stackwalker(MemoryRegion* memory, MinidumpModuleList* modules)
    : memory_(memory), modules_(modules) {
}


StackFrames* Stackwalker::Walk() {
  auto_ptr<StackFrames> frames(new StackFrames());

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

  return frames.release();
}


} // namespace google_airbag
