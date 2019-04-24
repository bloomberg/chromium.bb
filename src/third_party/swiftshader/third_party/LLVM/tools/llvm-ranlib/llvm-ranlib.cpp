//===-- llvm-ranlib.cpp - LLVM archive index generator --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Adds or updates an index (symbol table) for an LLVM archive file.
//
//===----------------------------------------------------------------------===//

#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Bitcode/Archive.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"
#include <memory>
using namespace llvm;

// llvm-ar operation code and modifier flags
static cl::opt<std::string>
ArchiveName(cl::Positional, cl::Optional, cl::desc("<archive-file>"));

static cl::opt<bool>
Verbose("verbose",cl::Optional,cl::init(false),
        cl::desc("Print the symbol table"));

// printSymbolTable - print out the archive's symbol table.
void printSymbolTable(Archive* TheArchive) {
  outs() << "\nArchive Symbol Table:\n";
  const Archive::SymTabType& symtab = TheArchive->getSymbolTable();
  for (Archive::SymTabType::const_iterator I=symtab.begin(), E=symtab.end();
       I != E; ++I ) {
    unsigned offset = TheArchive->getFirstFileOffset() + I->second;
    outs() << " " << format("%9u", offset) << "\t" << I->first <<"\n";
  }
}

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::PrettyStackTraceProgram X(argc, argv);

  LLVMContext &Context = getGlobalContext();
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

  // Have the command line options parsed and handle things
  // like --help and --version.
  cl::ParseCommandLineOptions(argc, argv,
    "LLVM Archive Index Generator (llvm-ranlib)\n\n"
    "  This program adds or updates an index of bitcode symbols\n"
    "  to an LLVM archive file."
  );

  int exitCode = 0;

  // Make sure we don't exit with "unhandled exception".
  try {

    // Check the path name of the archive
    sys::Path ArchivePath;
    if (!ArchivePath.set(ArchiveName))
      throw std::string("Archive name invalid: ") + ArchiveName;

    // Make sure it exists, we don't create empty archives
    bool Exists;
    if (llvm::sys::fs::exists(ArchivePath.str(), Exists) || !Exists)
      throw std::string("Archive file does not exist");

    std::string err_msg;
    std::auto_ptr<Archive>
      AutoArchive(Archive::OpenAndLoad(ArchivePath, Context, &err_msg));
    Archive* TheArchive = AutoArchive.get();
    if (!TheArchive)
      throw err_msg;

    if (TheArchive->writeToDisk(true, false, false, &err_msg ))
      throw err_msg;

    if (Verbose)
      printSymbolTable(TheArchive);

  } catch (const char* msg) {
    errs() << argv[0] << ": " << msg << "\n\n";
    exitCode = 1;
  } catch (const std::string& msg) {
    errs() << argv[0] << ": " << msg << "\n";
    exitCode = 2;
  } catch (...) {
    errs() << argv[0] << ": An unexpected unknown exception occurred.\n";
    exitCode = 3;
  }
  return exitCode;
}
