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

#include <stdio.h>
#include <map>
#include <string.h>
#include <vector>
#include <utility>
#include "processor/source_line_resolver.h"
#include "google/stack_frame.h"
#include "processor/linked_ptr.h"
#include "processor/range_map.h"

using std::map;
using std::vector;
using std::make_pair;
using __gnu_cxx::hash;

namespace google_airbag {

struct SourceLineResolver::Line {
  Line(MemAddr addr, MemAddr code_size, int file_id, int source_line)
      : address(addr)
      , size(code_size)
      , source_file_id(file_id)
      , line(source_line) { }

  MemAddr address;
  MemAddr size;
  int source_file_id;
  int line;
};

struct SourceLineResolver::Function {
  Function(const string &function_name,
           MemAddr function_address,
           MemAddr code_size)
      : name(function_name), address(function_address), size(code_size) { }

  string name;
  MemAddr address;
  MemAddr size;
  RangeMap<MemAddr, linked_ptr<Line> > lines;
};

class SourceLineResolver::Module {
 public:
  Module(const string &name) : name_(name) { }

  // Loads the given map file, returning true on success.
  bool LoadMap(const string &map_file);

  // Looks up the given relative address, and fills the StackFrame struct
  // with the result.
  void LookupAddress(MemAddr address, StackFrame *frame) const;

 private:
  friend class SourceLineResolver;
  typedef hash_map<int, string> FileMap;

  // Parses a file declaration
  void ParseFile(char *file_line);

  // Parses a function declaration, returning a new Function object.
  Function* ParseFunction(char *function_line);

  // Parses a line declaration, returning a new Line object.
  Line* ParseLine(char *line_line);

  string name_;
  FileMap files_;
  RangeMap<MemAddr, linked_ptr<Function> > functions_;
};

SourceLineResolver::SourceLineResolver() : modules_(new ModuleMap) {
}

SourceLineResolver::~SourceLineResolver() {
  ModuleMap::iterator it;
  for (it = modules_->begin(); it != modules_->end(); ++it) {
    delete it->second;
  }
  delete modules_;
}

bool SourceLineResolver::LoadModule(const string &module_name,
                                    const string &map_file) {
  // Make sure we don't already have a module with the given name.
  if (modules_->find(module_name) != modules_->end()) {
    return false;
  }

  Module *module = new Module(module_name);
  if (!module->LoadMap(map_file)) {
    delete module;
    return false;
  }

  modules_->insert(make_pair(module_name, module));
  return true;
}

void SourceLineResolver::FillSourceLineInfo(StackFrame *frame) const {
  ModuleMap::const_iterator it = modules_->find(frame->module_name);
  if (it != modules_->end()) {
    it->second->LookupAddress(frame->instruction - frame->module_base, frame);
  }
}

bool SourceLineResolver::Module::LoadMap(const string &map_file) {
  FILE *f = fopen(map_file.c_str(), "r");
  if (!f) {
    return false;
  }

  char buffer[1024];
  Function *cur_func = NULL;

  while (fgets(buffer, sizeof(buffer), f)) {
    if (strncmp(buffer, "FILE ", 5) == 0) {
      ParseFile(buffer);
    } else if (strncmp(buffer, "FUNC ", 5) == 0) {
      cur_func = ParseFunction(buffer);
      if (!cur_func) {
        return false;
      }
      functions_.StoreRange(cur_func->address, cur_func->size,
                            linked_ptr<Function>(cur_func));
    } else {
      if (!cur_func) {
        return false;
      }
      Line *line = ParseLine(buffer);
      if (!line) {
        return false;
      }
      cur_func->lines.StoreRange(line->address, line->size,
                                 linked_ptr<Line>(line));
    }
  }

  fclose(f);
  return true;
}

void SourceLineResolver::Module::LookupAddress(MemAddr address,
                                               StackFrame *frame) const {
  linked_ptr<Function> func;
  if (!functions_.RetrieveRange(address, &func)) {
    return;
  }

  frame->function_name = func->name;
  linked_ptr<Line> line;
  if (!func->lines.RetrieveRange(address, &line)) {
    return;
  }

  FileMap::const_iterator it = files_.find(line->source_file_id);
  if (it != files_.end()) {
    frame->source_file_name = files_.find(line->source_file_id)->second;
  }
  frame->source_line = line->line;
}

void SourceLineResolver::Module::ParseFile(char *file_line) {
  // FILE <id> <filename>
  file_line += 5;  // skip prefix
  char *id = strtok(file_line, " ");
  if (!id) {
    return;
  }

  int index = atoi(id);
  if (index < 0) {
    return;
  }

  char *filename = strtok(NULL, "\r\n");
  if (filename) {
    files_.insert(make_pair(index, string(filename)));
  }
}

SourceLineResolver::Function* SourceLineResolver::Module::ParseFunction(
    char *function_line) {
  // FUNC <address> <name>
  function_line += 5;  // skip prefix
  char *addr = strtok(function_line, " ");
  if (!addr) {
    return NULL;
  }

  char *size = strtok(NULL, " ");
  if (!size) {
    return NULL;
  }

  char *name = strtok(NULL, "\r\n");
  if (!name) {
    return NULL;
  }

  return new Function(name, strtoull(addr, NULL, 16), strtoull(size, NULL, 16));
}

SourceLineResolver::Line* SourceLineResolver::Module::ParseLine(
    char *line_line) {
  // <address> <line number> <source file id>
  char *addr = strtok(line_line, " ");
  if (!addr) {
    return NULL;
  }

  char *size = strtok(NULL, " ");
  if (!size) {
    return NULL;
  }

  char *line_num_str = strtok(NULL, "\r\n");
  if (!line_num_str) {
    return NULL;
  }

  int line_number, source_file;
  if (sscanf(line_num_str, "%d %d", &line_number, &source_file) != 2) {
    return NULL;
  }

  return new Line(strtoull(addr, NULL, 16),
                  strtoull(size, NULL, 16),
                  source_file,
                  line_number);
}

size_t SourceLineResolver::HashString::operator()(const string &s) const {
  return hash<const char*>()(s.c_str());
}

} // namespace google_airbag
