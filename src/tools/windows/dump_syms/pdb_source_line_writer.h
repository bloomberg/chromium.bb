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

// PDBSourceLineWriter uses a pdb file produced by Visual C++ to output
// a line/address map for use with SourceLineResolver.

#ifndef _PDB_SOURCE_LINE_WRITER_H__
#define _PDB_SOURCE_LINE_WRITER_H__

#include <string>
#include <atlcomcli.h>

struct IDiaEnumLineNumbers;
struct IDiaSession;
struct IDiaSymbol;

namespace google_airbag {

using std::wstring;

class PDBSourceLineWriter {
 public:
  explicit PDBSourceLineWriter();
  ~PDBSourceLineWriter();

  // Opens the given pdb file.  If there is already a pdb file open,
  // it is automatically closed.  Returns true on success.
  bool Open(const wstring &pdb_file);

  // Writes a map file from the current pdb file to the given file stream.
  // Returns true on success.
  bool WriteMap(FILE *map_file);

  // Closes the current pdb file and its associated resources.
  void Close();

 private:
  // Outputs the line/address pairs for each line in the enumerator.
  // Returns true on success.
  bool PrintLines(IDiaEnumLineNumbers *lines);

  // Outputs a function address and name, followed by its source line list.
  // Returns true on success.
  bool PrintFunction(IDiaSymbol *function);

  // Outputs all functions as described above.  Returns true on success.
  bool PrintFunctions();

  // Outputs all of the source files in the session's pdb file.
  // Returns true on success.
  bool PrintSourceFiles();

  // The session for the currently-open pdb file.
  CComPtr<IDiaSession> session_;

  // The current output file for this WriteMap invocation.
  FILE *output_;

  // Disallow copy ctor and operator=
  PDBSourceLineWriter(const PDBSourceLineWriter&);
  void operator=(const PDBSourceLineWriter&);
};

}  // namespace google_airbag

#endif  // _PDB_SOURCE_LINE_WRITER_H__
