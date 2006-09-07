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

// SourceLineResolver returns function/file/line info for a memory address.
// It uses address map files produced by a compatible writer, e.g.
// PDBSourceLineWriter.

#ifndef PROCESSOR_SOURCE_LINE_RESOLVER_H__
#define PROCESSOR_SOURCE_LINE_RESOLVER_H__

#include <string>
#include <ext/hash_map>

namespace google_airbag {

using std::string;
using __gnu_cxx::hash_map;

class StackFrame;

class SourceLineResolver {
 public:
  typedef unsigned long long MemAddr;

  SourceLineResolver();
  ~SourceLineResolver();

  // Adds a module to this resolver, returning true on success.
  //
  // module_name may be an arbitrary string.  Typically, it will be the
  // filename of the module, optionally with version identifiers.
  //
  // map_file should contain line/address mappings for this module.
  bool LoadModule(const string &module_name, const string &map_file);

  // Fills in the function_base, function_name, source_file_name,
  // and source_line fields of the StackFrame.  The instruction and
  // module_name fields must already be filled in.
  void FillSourceLineInfo(StackFrame *frame) const;

 private:
  template<class T> class MemAddrMap;
  struct Line;
  struct Function;
  struct File;
  struct HashString {
    size_t operator()(const string &s) const;
  };
  class Module;

  // All of the modules we've loaded
  typedef hash_map<string, Module*, HashString> ModuleMap;
  ModuleMap *modules_;

  // Disallow unwanted copy ctor and assignment operator
  SourceLineResolver(const SourceLineResolver&);
  void operator=(const SourceLineResolver&);
};

} // namespace google_airbag

#endif  // PROCESSOR_SOURCE_LINE_RESOLVER_H__
