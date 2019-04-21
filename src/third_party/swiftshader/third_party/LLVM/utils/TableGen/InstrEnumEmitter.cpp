//===- InstrEnumEmitter.cpp - Generate Instruction Set Enums --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This tablegen backend is responsible for emitting enums for each machine
// instruction.
//
//===----------------------------------------------------------------------===//

#include "InstrEnumEmitter.h"
#include "CodeGenTarget.h"
#include "llvm/TableGen/Record.h"
#include <cstdio>
using namespace llvm;

// runEnums - Print out enum values for all of the instructions.
void InstrEnumEmitter::run(raw_ostream &OS) {
  EmitSourceFileHeader("Target Instruction Enum Values", OS);
  OS << "namespace llvm {\n\n";

  CodeGenTarget Target(Records);

  // We must emit the PHI opcode first...
  std::string Namespace = Target.getInstNamespace();
  
  if (Namespace.empty()) {
    fprintf(stderr, "No instructions defined!\n");
    exit(1);
  }

  const std::vector<const CodeGenInstruction*> &NumberedInstructions =
    Target.getInstructionsByEnumValue();

  OS << "namespace " << Namespace << " {\n";
  OS << "  enum {\n";
  for (unsigned i = 0, e = NumberedInstructions.size(); i != e; ++i) {
    OS << "    " << NumberedInstructions[i]->TheDef->getName()
       << "\t= " << i << ",\n";
  }
  OS << "    INSTRUCTION_LIST_END = " << NumberedInstructions.size() << "\n";
  OS << "  };\n}\n";
  OS << "} // End llvm namespace \n";
}
