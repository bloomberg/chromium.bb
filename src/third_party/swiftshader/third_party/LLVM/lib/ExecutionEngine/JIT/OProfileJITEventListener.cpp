//===-- OProfileJITEventListener.cpp - Tell OProfile about JITted code ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines a JITEventListener object that calls into OProfile to tell
// it about JITted functions.  For now, we only record function names and sizes,
// but eventually we'll also record line number information.
//
// See http://oprofile.sourceforge.net/doc/devel/jit-interface.html for the
// definition of the interface we're using.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "oprofile-jit-event-listener"
#include "llvm/Function.h"
#include "llvm/Metadata.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ValueHandle.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Errno.h"
#include "llvm/Config/config.h"
#include <stddef.h>
using namespace llvm;

#if USE_OPROFILE

#include <opagent.h>

namespace {

class OProfileJITEventListener : public JITEventListener {
  op_agent_t Agent;
public:
  OProfileJITEventListener();
  ~OProfileJITEventListener();

  virtual void NotifyFunctionEmitted(const Function &F,
                                     void *FnStart, size_t FnSize,
                                     const EmittedFunctionDetails &Details);
  virtual void NotifyFreeingMachineCode(void *OldPtr);
};

OProfileJITEventListener::OProfileJITEventListener()
    : Agent(op_open_agent()) {
  if (Agent == NULL) {
    const std::string err_str = sys::StrError();
    DEBUG(dbgs() << "Failed to connect to OProfile agent: " << err_str << "\n");
  } else {
    DEBUG(dbgs() << "Connected to OProfile agent.\n");
  }
}

OProfileJITEventListener::~OProfileJITEventListener() {
  if (Agent != NULL) {
    if (op_close_agent(Agent) == -1) {
      const std::string err_str = sys::StrError();
      DEBUG(dbgs() << "Failed to disconnect from OProfile agent: "
                   << err_str << "\n");
    } else {
      DEBUG(dbgs() << "Disconnected from OProfile agent.\n");
    }
  }
}

class FilenameCache {
  // Holds the filename of each Scope, so that we can pass a null-terminated
  // string into oprofile.  Use an AssertingVH rather than a ValueMap because we
  // shouldn't be modifying any MDNodes while this map is alive.
  DenseMap<AssertingVH<MDNode>, std::string> Filenames;

 public:
  const char *getFilename(MDNode *Scope) {
    std::string &Filename = Filenames[Scope];
    if (Filename.empty()) {
      Filename = DIScope(Scope).getFilename();
    }
    return Filename.c_str();
  }
};

static debug_line_info LineStartToOProfileFormat(
    const MachineFunction &MF, FilenameCache &Filenames,
    uintptr_t Address, DebugLoc Loc) {
  debug_line_info Result;
  Result.vma = Address;
  Result.lineno = Loc.getLine();
  Result.filename = Filenames.getFilename(
    Loc.getScope(MF.getFunction()->getContext()));
  DEBUG(dbgs() << "Mapping " << reinterpret_cast<void*>(Result.vma) << " to "
               << Result.filename << ":" << Result.lineno << "\n");
  return Result;
}

// Adds the just-emitted function to the symbol table.
void OProfileJITEventListener::NotifyFunctionEmitted(
    const Function &F, void *FnStart, size_t FnSize,
    const EmittedFunctionDetails &Details) {
  assert(F.hasName() && FnStart != 0 && "Bad symbol to add");
  if (op_write_native_code(Agent, F.getName().data(),
                           reinterpret_cast<uint64_t>(FnStart),
                           FnStart, FnSize) == -1) {
    DEBUG(dbgs() << "Failed to tell OProfile about native function "
          << F.getName() << " at ["
          << FnStart << "-" << ((char*)FnStart + FnSize) << "]\n");
    return;
  }

  if (!Details.LineStarts.empty()) {
    // Now we convert the line number information from the address/DebugLoc
    // format in Details to the address/filename/lineno format that OProfile
    // expects.  Note that OProfile 0.9.4 has a bug that causes it to ignore
    // line numbers for addresses above 4G.
    FilenameCache Filenames;
    std::vector<debug_line_info> LineInfo;
    LineInfo.reserve(1 + Details.LineStarts.size());

    DebugLoc FirstLoc = Details.LineStarts[0].Loc;
    assert(!FirstLoc.isUnknown()
           && "LineStarts should not contain unknown DebugLocs");
    MDNode *FirstLocScope = FirstLoc.getScope(F.getContext());
    DISubprogram FunctionDI = getDISubprogram(FirstLocScope);
    if (FunctionDI.Verify()) {
      // If we have debug info for the function itself, use that as the line
      // number of the first several instructions.  Otherwise, after filling
      // LineInfo, we'll adjust the address of the first line number to point at
      // the start of the function.
      debug_line_info line_info;
      line_info.vma = reinterpret_cast<uintptr_t>(FnStart);
      line_info.lineno = FunctionDI.getLineNumber();
      line_info.filename = Filenames.getFilename(FirstLocScope);
      LineInfo.push_back(line_info);
    }

    for (std::vector<EmittedFunctionDetails::LineStart>::const_iterator
           I = Details.LineStarts.begin(), E = Details.LineStarts.end();
         I != E; ++I) {
      LineInfo.push_back(LineStartToOProfileFormat(
                           *Details.MF, Filenames, I->Address, I->Loc));
    }

    // In case the function didn't have line info of its own, adjust the first
    // line info's address to include the start of the function.
    LineInfo[0].vma = reinterpret_cast<uintptr_t>(FnStart);

    if (op_write_debug_line_info(Agent, FnStart,
                                 LineInfo.size(), &*LineInfo.begin()) == -1) {
      DEBUG(dbgs()
            << "Failed to tell OProfile about line numbers for native function "
            << F.getName() << " at ["
            << FnStart << "-" << ((char*)FnStart + FnSize) << "]\n");
    }
  }
}

// Removes the being-deleted function from the symbol table.
void OProfileJITEventListener::NotifyFreeingMachineCode(void *FnStart) {
  assert(FnStart && "Invalid function pointer");
  if (op_unload_native_code(Agent, reinterpret_cast<uint64_t>(FnStart)) == -1) {
    DEBUG(dbgs()
          << "Failed to tell OProfile about unload of native function at "
          << FnStart << "\n");
  }
}

}  // anonymous namespace.

namespace llvm {
JITEventListener *createOProfileJITEventListener() {
  return new OProfileJITEventListener;
}
}

#else  // USE_OPROFILE

namespace llvm {
// By defining this to return NULL, we can let clients call it unconditionally,
// even if they haven't configured with the OProfile libraries.
JITEventListener *createOProfileJITEventListener() {
  return NULL;
}
}  // namespace llvm

#endif  // USE_OPROFILE
