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
#include "common/windows/pdb_source_line_writer.h"

namespace google_airbag {

PDBSourceLineWriter::PDBSourceLineWriter() : output_(NULL) {
}

PDBSourceLineWriter::~PDBSourceLineWriter() {
}

bool PDBSourceLineWriter::Open(const wstring &file, FileFormat format) {
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

  switch (format) {
    case PDB_FILE:
      if (FAILED(data_source->loadDataFromPdb(file.c_str()))) {
        fprintf(stderr, "loadDataFromPdb failed\n");
        return false;
      }
      break;
    case EXE_FILE:
      if (FAILED(data_source->loadDataForExe(file.c_str(), NULL, NULL))) {
        fprintf(stderr, "loadDataForExe failed\n");
        return false;
      }
      break;
    default:
      fprintf(stderr, "Unknown file format\n");
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

bool PDBSourceLineWriter::PrintFrameData() {
  // It would be nice if it were possible to output frame data alongside the
  // associated function, as is done with line numbers, but the DIA API
  // doesn't make it possible to get the frame data in that way.

  CComPtr<IDiaEnumTables> tables;
  if (FAILED(session_->getEnumTables(&tables)))
    return false;

  // Pick up the first table that supports IDiaEnumFrameData.
  CComPtr<IDiaEnumFrameData> frame_data_enum;
  CComPtr<IDiaTable> table;
  ULONG count;
  while (!frame_data_enum &&
         SUCCEEDED(tables->Next(1, &table, &count)) &&
	 count == 1) {
    table->QueryInterface(_uuidof(IDiaEnumFrameData),
                          reinterpret_cast<void**>(&frame_data_enum));
    table.Release();
  }
  if (!frame_data_enum)
    return false;

  CComPtr<IDiaFrameData> frame_data;
  while (SUCCEEDED(frame_data_enum->Next(1, &frame_data, &count)) &&
         count == 1) {
    DWORD type;
    if (FAILED(frame_data->get_type(&type)))
      return false;

    DWORD rva;
    if (FAILED(frame_data->get_relativeVirtualAddress(&rva)))
      return false;

    DWORD code_size;
    if (FAILED(frame_data->get_lengthBlock(&code_size)))
      return false;

    DWORD prolog_size;
    if (FAILED(frame_data->get_lengthProlog(&prolog_size)))
      return false;

    // epliog_size is always 0.
    DWORD epilog_size = 0;

    DWORD parameter_size;
    if (FAILED(frame_data->get_lengthParams(&parameter_size)))
      return false;

    DWORD saved_register_size;
    if (FAILED(frame_data->get_lengthSavedRegisters(&saved_register_size)))
      return false;

    DWORD local_size;
    if (FAILED(frame_data->get_lengthLocals(&local_size)))
      return false;

    DWORD max_stack_size;
    if (FAILED(frame_data->get_maxStack(&max_stack_size)))
      return false;

    BSTR program_string;
    if (FAILED(frame_data->get_program(&program_string)))
      return false;

    fprintf(output_, "STACK WIN %x %x %x %x %x %x %x %x %x %ws\n",
            type, rva, code_size, prolog_size, epilog_size,
            parameter_size, saved_register_size, local_size, max_stack_size,
            program_string);

    frame_data.Release();
  }

  return true;
}

bool PDBSourceLineWriter::WriteMap(FILE *map_file) {
  bool ret = false;
  output_ = map_file;
  if (PrintSourceFiles() && PrintFunctions() && PrintFrameData()) {
    ret = true;
  }

  output_ = NULL;
  return ret;
}

void PDBSourceLineWriter::Close() {
  session_.Release();
}

wstring PDBSourceLineWriter::GetModuleGUID() {
  CComPtr<IDiaSymbol> global;
  if (FAILED(session_->get_globalScope(&global))) {
    return L"";
  }

  GUID guid;
  if (FAILED(global->get_guid(&guid))) {
    return L"";
  }

  wchar_t guid_buf[37];
  _snwprintf_s(guid_buf, sizeof(guid_buf)/sizeof(wchar_t), _TRUNCATE,
               L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
               guid.Data1, guid.Data2, guid.Data3,
               guid.Data4[0], guid.Data4[1], guid.Data4[2],
               guid.Data4[3], guid.Data4[4], guid.Data4[5],
               guid.Data4[6], guid.Data4[7]);
  return guid_buf;
}

}  // namespace google_airbag
