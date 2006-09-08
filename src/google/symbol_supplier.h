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

// The caller may implement the SymbolSupplier abstract base class
// to provide symbols for a given module.

#ifndef GOOGLE_SYMBOL_SUPPLIER_H__
#define GOOGLE_SYMBOL_SUPPLIER_H__

#include <string>

namespace google_airbag {

using std::string;
class MinidumpModule;
struct CrashReport;

class SymbolSupplier {
 public:
  virtual ~SymbolSupplier() {}

  // Returns the path to the symbol file for the given module.
  // report will be the pointer supplied to ProcessReport().
  virtual string GetSymbolFile(MinidumpModule *module,
                               const CrashReport *report) = 0;
};

}  // namespace google_airbag

#endif  // GOOGLE_SYMBOL_SUPPLIER_H__
