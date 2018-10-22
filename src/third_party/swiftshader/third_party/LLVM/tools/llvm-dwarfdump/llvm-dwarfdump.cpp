//===-- llvm-dwarfdump.cpp - Debug info dumping utility for llvm -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This program is a utility that works like "dwarfdump".
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/MemoryObject.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"
#include <algorithm>
#include <cstring>
using namespace llvm;
using namespace object;

static cl::list<std::string>
InputFilenames(cl::Positional, cl::desc("<input object files>"),
               cl::ZeroOrMore);

static cl::opt<unsigned long long>
Address("address", cl::init(-1ULL),
        cl::desc("Print line information for a given address"));

static void DumpInput(const StringRef &Filename) {
  OwningPtr<MemoryBuffer> Buff;

  if (error_code ec = MemoryBuffer::getFileOrSTDIN(Filename, Buff)) {
    errs() << Filename << ": " << ec.message() << "\n";
    return;
  }

  OwningPtr<ObjectFile> Obj(ObjectFile::createObjectFile(Buff.take()));

  StringRef DebugInfoSection;
  StringRef DebugAbbrevSection;
  StringRef DebugLineSection;
  StringRef DebugArangesSection;
  StringRef DebugStringSection;

  error_code ec;
  for (section_iterator i = Obj->begin_sections(),
                        e = Obj->end_sections();
                        i != e; i.increment(ec)) {
    StringRef name;
    i->getName(name);
    StringRef data;
    i->getContents(data);

    if (name.startswith("__DWARF,"))
      name = name.substr(8); // Skip "__DWARF," prefix.
    name = name.substr(name.find_first_not_of("._")); // Skip . and _ prefixes.
    if (name == "debug_info")
      DebugInfoSection = data;
    else if (name == "debug_abbrev")
      DebugAbbrevSection = data;
    else if (name == "debug_line")
      DebugLineSection = data;
    else if (name == "debug_aranges")
      DebugArangesSection = data;
    else if (name == "debug_str")
      DebugStringSection = data;
  }

  OwningPtr<DIContext> dictx(DIContext::getDWARFContext(/*FIXME*/true,
                                                        DebugInfoSection,
                                                        DebugAbbrevSection,
                                                        DebugArangesSection,
                                                        DebugLineSection,
                                                        DebugStringSection));
  if (Address == -1ULL) {
    outs() << Filename
           << ":\tfile format " << Obj->getFileFormatName() << "\n\n";
    // Dump the complete DWARF structure.
    dictx->dump(outs());
  } else {
    // Print line info for the specified address.
    DILineInfo dli = dictx->getLineInfoForAddress(Address);
    outs() << (dli.getFileName() ? dli.getFileName() : "<unknown>") << ':'
           << dli.getLine() << ':' << dli.getColumn() << '\n';
  }
}

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

  cl::ParseCommandLineOptions(argc, argv, "llvm dwarf dumper\n");

  // Defaults to a.out if no filenames specified.
  if (InputFilenames.size() == 0)
    InputFilenames.push_back("a.out");

  std::for_each(InputFilenames.begin(), InputFilenames.end(), DumpInput);

  return 0;
}
