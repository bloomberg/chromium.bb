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

#include <stdio.h>
#include <atlbase.h>
#include <dia2.h>
#include "pdb_source_line_writer.h"

namespace google_airbag {

PDBSourceLineWriter::PDBSourceLineWriter() : output_(NULL) {
}

PDBSourceLineWriter::~PDBSourceLineWriter() {
}

bool PDBSourceLineWriter::Open(const wstring &pdb_file) {
  Close();

  if (FAILED(CoInitialize(NULL))) {
    fprintf(stderr, "CoInitialize failed\n");
    return false;
  }

  CComPtr<IDiaDataSource> data_source;
  if (FAILED(data_source.CoCreateInstance(CLSID_DiaSource))) {
    fprintf(stderr, "CoCreateInstance CLSID_DiaSource failed "
            "(msdia80.dll unregistered?)\n");
    return false;
  }

  if (FAILED(data_source->loadDataFromPdb(pdb_file.c_str()))) {
    fprintf(stderr, "loadDataFromPdb failed\n");
    return false;
  }

  if (FAILED(data_source->openSession(&session_))) {
    fprintf(stderr, "openSession failed\n");
  }

  return true;
}

bool PDBSourceLineWriter::PrintLines(IDiaEnumLineNumbers *lines) {
  // The line number format is:
  // <rva> <line number> <source file id>
  CComPtr<IDiaLineNumber> line;
  ULONG count;

  while (SUCCEEDED(lines->Next(1, &line, &count)) && count == 1) {
    DWORD rva;
    if (FAILED(line->get_relativeVirtualAddress(&rva))) {
      fprintf(stderr, "failed to get line rva\n");
      return false;
    }

    DWORD length;
    if (FAILED(line->get_length(&length))) {
      fprintf(stderr, "failed to get line code length\n");
      return false;
    }

    DWORD source_id;
    if (FAILED(line->get_sourceFileId(&source_id))) {
      fprintf(stderr, "failed to get line source file id\n");
      return false;
    }

    DWORD line_num;
    if (FAILED(line->get_lineNumber(&line_num))) {
      fprintf(stderr, "failed to get line number\n");
      return false;
    }

    fprintf(output_, "%x %x %d %d\n", rva, length, line_num, source_id);
    line.Release();
  }
  return true;
}

bool PDBSourceLineWriter::PrintFunction(IDiaSymbol *function) {
  // The function format is:
  // FUNC <address> <function>
  CComBSTR name;
  if (FAILED(function->get_name(&name))) {
    fprintf(stderr, "failed to get function name\n");
    return false;
  }
  if (name.Length() == 0) {
    fprintf(stderr, "empty function name\n");
    return false;
  }

  ULONGLONG length;
  if (FAILED(function->get_length(&length))) {
    fprintf(stderr, "failed to get function length\n");
    return false;
  }

  DWORD rva;
  if (FAILED(function->get_relativeVirtualAddress(&rva))) {
    fprintf(stderr, "couldn't get rva\n");
    return false;
  }

  CComPtr<IDiaEnumLineNumbers> lines;
  if (FAILED(session_->findLinesByRVA(rva, DWORD(length), &lines))) {
    return false;
  }

  fwprintf(output_, L"FUNC %x %llx %s\n", rva, length, name);
  if (!PrintLines(lines)) {
    return false;
  }
  return true;
}

bool PDBSourceLineWriter::PrintSourceFiles() {
  CComPtr<IDiaSymbol> global;
  if (FAILED(session_->get_globalScope(&global))) {
    fprintf(stderr, "get_globalScope failed\n");
    return false;
  }

  CComPtr<IDiaEnumSymbols> compilands;
  if (FAILED(global->findChildren(SymTagCompiland, NULL,
                                  nsNone, &compilands))) {
    fprintf(stderr, "findChildren failed\n");
    return false;
  }

  CComPtr<IDiaSymbol> compiland;
  ULONG count;
  while (SUCCEEDED(compilands->Next(1, &compiland, &count)) && count == 1) {
    CComPtr<IDiaEnumSourceFiles> source_files;
    if (FAILED(session_->findFile(compiland, NULL, nsNone, &source_files))) {
      return false;
    }
    CComPtr<IDiaSourceFile> file;
    while (SUCCEEDED(source_files->Next(1, &file, &count)) && count == 1) {
      DWORD file_id;
      if (FAILED(file->get_uniqueId(&file_id))) {
        return false;
      }

      CComBSTR file_name;
      if (FAILED(file->get_fileName(&file_name))) {
        return false;
      }

      fwprintf(output_, L"FILE %d %s\n", file_id, file_name);
      file.Release();
    }
    compiland.Release();
  }
  return true;
}

bool PDBSourceLineWriter::PrintFunctions() {
  CComPtr<IDiaEnumSymbolsByAddr> symbols;
  if (FAILED(session_->getSymbolsByAddr(&symbols))) {
    fprintf(stderr, "failed to get symbol enumerator\n");
    return false;
  }

  CComPtr<IDiaSymbol> symbol;
  if (FAILED(symbols->symbolByAddr(1, 0, &symbol))) {
    fprintf(stderr, "failed to enumerate symbols\n");
    return false;
  }

  DWORD rva_last = 0;
  if (FAILED(symbol->get_relativeVirtualAddress(&rva_last))) {
    fprintf(stderr, "failed to get symbol rva\n");
    return  false;
  }

  ULONG count;
  do {
    DWORD tag;
    if (FAILED(symbol->get_symTag(&tag))) {
      fprintf(stderr, "failed to get symbol tag\n");
      return false;
    }
    if (tag == SymTagFunction) {
      if (!PrintFunction(symbol)) {
        return false;
      }
    }
    symbol.Release();
  } while (SUCCEEDED(symbols->Next(1, &symbol, &count)) && count == 1);

  return true;
}

bool PDBSourceLineWriter::WriteMap(FILE *map_file) {
  bool ret = false;
  output_ = map_file;
  if (PrintSourceFiles() && PrintFunctions()) {
    ret = true;
  }

  output_ = NULL;
  return ret;
}

void PDBSourceLineWriter::Close() {
  session_.Release();
}

}  // namespace google_airbag
