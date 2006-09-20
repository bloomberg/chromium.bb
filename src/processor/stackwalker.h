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

// stackwalker.h: Generic stackwalker.
//
// The Stackwalker class is an abstract base class providing common generic
// methods that apply to stacks from all systems.  Specific implementations
// will extend this class by providing GetContextFrame and GetCallerFrame
// methods to fill in system-specific data in a StackFrame structure.
// Stackwalker assembles these StackFrame strucutres into a vector of
// StackFrames.
//
// Author: Mark Mentovai


#ifndef PROCESSOR_STACKWALKER_H__
#define PROCESSOR_STACKWALKER_H__


#include "google/stack_frame.h"
#include "processor/memory_region.h"


namespace google_airbag {


class MinidumpModuleList;
class SymbolSupplier;


class Stackwalker {
 public:
  virtual ~Stackwalker() {}

  // Fills the given vector of StackFrames by calling GetContextFrame and
  // GetCallerFrame, and populating the returned frames with all available
  // data.
  void Walk(StackFrames *frames);

 protected:
  // memory identifies a MemoryRegion that provides the stack memory
  // for the stack to walk.  modules, if non-NULL, is a MinidumpModuleList
  // that is used to look up which code module each stack frame is
  // associated with.  supplier is an optional caller-supplied SymbolSupplier
  // implementation.  If supplier is NULL, source line info will not be
  // resolved.
  Stackwalker(MemoryRegion* memory,
              MinidumpModuleList* modules,
              SymbolSupplier* supplier);

  // The stack memory to walk.  Subclasses will require this region to
  // get information from the stack.
  MemoryRegion* memory_;

 private:
  // Obtains the context frame, the innermost called procedure in a stack
  // trace.  Returns false on failure.
  virtual bool GetContextFrame(StackFrame* frame) = 0;

  // Obtains a caller frame.  Each call to GetCallerFrame should return the
  // frame that called the last frame returned by GetContextFrame or
  // GetCallerFrame.  GetCallerFrame should return false on failure or
  // when there are no more caller frames (when the end of the stack has
  // been reached).
  virtual bool GetCallerFrame(StackFrame* frame) = 0;

  // A list of modules, for populating each StackFrame's module information.
  // This field is optional and may be NULL.
  MinidumpModuleList* modules_;

  // The optional SymbolSupplier for resolving source line info.
  SymbolSupplier* supplier_;
};


} // namespace google_airbag


#endif // PROCESSOR_STACKWALKER_H__
