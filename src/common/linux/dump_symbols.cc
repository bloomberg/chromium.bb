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

#include <a.out.h>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cxxabi.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <stab.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>

#include <functional>
#include <list>
#include <vector>
#include <map>
#include <string.h>

#include "common/linux/dump_symbols.h"
#include "common/linux/file_id.h"
#include "common/linux/guid_creator.h"
#include "processor/scoped_ptr.h"

// This namespace contains helper functions.
namespace {

struct SourceFileInfo;

// Infomation of a line.
struct LineInfo {
  // Offset from start of the function.
  // Load from stab symbol.
  ElfW(Off) rva_to_func;
  // Offset from base of the loading binary.
  ElfW(Off) rva_to_base;
  // Size of the line.
  // It is the difference of the starting address of the line and starting
  // address of the next N_SLINE, N_FUN or N_SO.
  uint32_t size;
  // Line number.
  uint32_t line_num;
  // The source file this line belongs to.
  SourceFileInfo *file;
};

typedef std::list<struct LineInfo> LineInfoList;

// Information of a function.
struct FuncInfo {
  // Name of the function.
  const char *name;
  // Offset from the base of the loading address.
  ElfW(Off) rva_to_base;
  // Virtual address of the function.
  // Load from stab symbol.
  ElfW(Addr) addr;
  // Size of the function.
  // It is the difference of the starting address of the function and starting
  // address of the next N_FUN or N_SO.
  uint32_t size;
  // Total size of stack parameters.
  uint32_t stack_param_size;
  // Line information array.
  LineInfoList line_info;
};

typedef std::list<struct FuncInfo> FuncInfoList;

// Information of a source file.
struct SourceFileInfo {
  // Name of the source file.
  const char *name;
  // Starting address of the source file.
  ElfW(Addr) addr;
  // Id of the source file.
  int source_id;
  // Functions information.
  FuncInfoList func_info;
};

// A simple std::list of pointers to SourceFileInfo structures, that
// owns the structures pointed to: destroying the list destroys them,
// as well.
class SourceFileInfoList : public std::list<SourceFileInfo *> {
 public:
  ~SourceFileInfoList() {
    for (iterator it = this->begin(); it != this->end(); it++)
      delete *it;
  }
};

typedef std::map<const char *, SourceFileInfo *> NameToFileMap;

// Information of a symbol table.
// This is the root of all types of symbol.
struct SymbolInfo {
  // The main files used in this module.  This does not include header
  // files; it includes only files that were provided as the primary
  // source file for the compilation unit.  In STABS, these are files
  // named in 'N_SO' entries.
  SourceFileInfoList main_files;

  // Map from file names to source file structures.  Note that this
  // map's keys are compared as pointers, not strings, so if the same
  // name appears at two different addresses in stabstr, the map will
  // treat that as two different names.  If the linker didn't unify
  // names in .stabstr (which it does), this would result in duplicate
  // FILE lines, which is benign.
  NameToFileMap name_to_file;

  // An array of some addresses at which a file boundary occurs.
  //
  // The STABS information describing a compilation unit gives the
  // unit's start address, but not its ending address or size.  Those
  // must be inferred by finding the start address of the next file.
  // For the last compilation unit, or when one compilation unit ends
  // before the next one starts, STABS includes an N_SO entry whose
  // filename is the empty string; such an entry's address serves
  // simply to mark the end of the preceding compilation unit.  Rather
  // than create FuncInfoList for such entries, we record their
  // addresses here.  These are not necessarily sorted.
  std::vector<ElfW(Addr)> file_boundaries;
};

// Stab section name.
static const char *kStabName = ".stab";

// Demangle using abi call.
// Older GCC may not support it.
static std::string Demangle(const char *mangled) {
  int status = 0;
  char *demangled = abi::__cxa_demangle(mangled, NULL, NULL, &status);
  if (status == 0 && demangled != NULL) {
    std::string str(demangled);
    free(demangled);
    return str;
  }
  return std::string(mangled);
}

// Fix offset into virtual address by adding the mapped base into offsets.
// Make life easier when want to find something by offset.
static void FixAddress(void *obj_base) {
  ElfW(Word) base = reinterpret_cast<ElfW(Word)>(obj_base);
  ElfW(Ehdr) *elf_header = static_cast<ElfW(Ehdr) *>(obj_base);
  elf_header->e_phoff += base;
  elf_header->e_shoff += base;
  ElfW(Shdr) *sections = reinterpret_cast<ElfW(Shdr) *>(elf_header->e_shoff);
  for (int i = 0; i < elf_header->e_shnum; ++i)
    sections[i].sh_offset += base;
}

// Find the prefered loading address of the binary.
static ElfW(Addr) GetLoadingAddress(const ElfW(Phdr) *program_headers,
                                    int nheader) {
  for (int i = 0; i < nheader; ++i) {
    const ElfW(Phdr) &header = program_headers[i];
    // For executable, it is the PT_LOAD segment with offset to zero.
    if (header.p_type == PT_LOAD &&
        header.p_offset == 0)
      return header.p_vaddr;
  }
  // For other types of ELF, return 0.
  return 0;
}

static bool IsValidElf(const ElfW(Ehdr) *elf_header) {
  return memcmp(elf_header, ELFMAG, SELFMAG) == 0;
}

static const ElfW(Shdr) *FindSectionByName(const char *name,
                                           const ElfW(Shdr) *sections,
                                           const ElfW(Shdr) *strtab,
                                           int nsection) {
  assert(name != NULL);
  assert(sections != NULL);
  assert(nsection > 0);

  int name_len = strlen(name);
  if (name_len == 0)
    return NULL;

  for (int i = 0; i < nsection; ++i) {
    const char *section_name =
      (char*)(strtab->sh_offset + sections[i].sh_name);
    if (!strncmp(name, section_name, name_len))
      return sections + i;
  }
  return NULL;
}

// Return the SourceFileInfo for the file named NAME in SYMBOLS, as
// recorden in the name_to_file map.  If none exists, create a new
// one.
//
// If the file is a main file, it is the caller's responsibility to
// set its address and add it to the list of main files.
//
// When creating a new file, this function does not make a copy of
// NAME; NAME must stay alive for as long as the symbol table does.
static SourceFileInfo *FindSourceFileInfo(SymbolInfo *symbols,
                                          const char *name) {
  SourceFileInfo **map_entry = &symbols->name_to_file[name];
  SourceFileInfo *file;
  if (*map_entry)
    file = *map_entry;
  else {
    file = new SourceFileInfo;
    file->name = name;
    file->source_id = -1;
    file->addr = 0;
    *map_entry = file;
  }
  return file;
}

static int LoadLineInfo(struct nlist *list,
                        struct nlist *list_end,
                        SymbolInfo *symbols,
                        struct SourceFileInfo *source_file_info,
                        struct FuncInfo *func_info,
                        const ElfW(Shdr) *stabstr_section) {
  struct nlist *cur_list = list;
  // The source file to which subsequent lines belong.
  SourceFileInfo *current_source_file = source_file_info;
  // The name of the file any subsequent lines would belong to.
  const char *last_source_name = current_source_file->name;
  do {
    // Skip non line information.
    while (cur_list < list_end && cur_list->n_type != N_SLINE) {
      // Only exit when got another function, or source file.
      if (cur_list->n_type == N_FUN || cur_list->n_type == N_SO)
        return cur_list - list;
      // N_SOL means source lines following it will be from another
      // source file.  But don't actually create a file entry yet;
      // wait until we see executable code attributed to the file.
      if (cur_list->n_type == N_SOL
          && cur_list->n_un.n_strx > 0)
        last_source_name = reinterpret_cast<char *>(cur_list->n_un.n_strx
                                                 + stabstr_section->sh_offset);
      ++cur_list;
    }
    struct LineInfo line;
    while (cur_list < list_end && cur_list->n_type == N_SLINE) {
      // If this line is attributed to a new file, create its entry now.
      if (last_source_name != current_source_file->name)
        current_source_file = FindSourceFileInfo(symbols, last_source_name);
      line.file = current_source_file;
      line.rva_to_func = cur_list->n_value;
      // n_desc is a signed short
      line.line_num = (unsigned short)cur_list->n_desc;
      // We will compute these later.  For now, pacify compiler warnings.
      line.size = 0;
      line.rva_to_base = 0;
      func_info->line_info.push_back(line);
      ++cur_list;
    }
  } while (list < list_end);

  return cur_list - list;
}

static int LoadFuncSymbols(struct nlist *list,
                           struct nlist *list_end,
                           SymbolInfo *symbols,
                           struct SourceFileInfo *source_file_info,
                           const ElfW(Shdr) *stabstr_section) {
  struct nlist *cur_list = list;
  assert(cur_list->n_type == N_SO);
  ++cur_list;
  source_file_info->func_info.clear();
  while (cur_list < list_end) {
    // Go until the function symbol.
    while (cur_list < list_end && cur_list->n_type != N_FUN) {
      if (cur_list->n_type == N_SO) {
        return cur_list - list;
      }
      ++cur_list;
      continue;
    }
    if (cur_list->n_type == N_FUN) {
      struct FuncInfo func_info;
      func_info.name =
        reinterpret_cast<char *>(cur_list->n_un.n_strx +
                                 stabstr_section->sh_offset);
      func_info.addr = cur_list->n_value;
      func_info.rva_to_base = 0;
      func_info.size = 0;
      func_info.stack_param_size = 0;
      cur_list++;

      // Line info.
      cur_list += LoadLineInfo(cur_list,
                               list_end,
                               symbols,
                               source_file_info,
                               &func_info,
                               stabstr_section);

      // Functions in this module should have address bigger than the module
      // startring address.
      // There maybe a lot of duplicated entry for a function in the symbol,
      // only one of them can met this.
      if (func_info.addr >= source_file_info->addr) {
        source_file_info->func_info.push_back(func_info);
      }
    }
  }
  return cur_list - list;
}

// Compute size and rva information based on symbols loaded from stab section.
static bool ComputeSizeAndRVA(ElfW(Addr) loading_addr,
                              struct SymbolInfo *symbols) {
  SourceFileInfoList::iterator file_it;
  FuncInfoList::iterator func_it;
  LineInfoList::iterator line_it;

  // A table of all the addresses at which files and functions start
  // or end.  We build this from the file boundary list and our lists
  // of files and functions, sort it, and then use it to find the ends
  // of functions and source lines for which we have no size
  // information.
  std::vector<ElfW(Addr)> boundaries = symbols->file_boundaries;
  for (file_it = symbols->main_files.begin();
       file_it != symbols->main_files.end(); file_it++) {
    boundaries.push_back((*file_it)->addr);
    for (func_it = (*file_it)->func_info.begin();
         func_it != (*file_it)->func_info.end(); func_it++)
      boundaries.push_back(func_it->addr);
  }
  std::sort(boundaries.begin(), boundaries.end());

  int no_next_addr_count = 0;
  for (file_it = symbols->main_files.begin();
       file_it != symbols->main_files.end(); file_it++) {
    for (func_it = (*file_it)->func_info.begin();
         func_it != (*file_it)->func_info.end(); func_it++) {
      struct FuncInfo &func_info = *func_it;
      assert(func_info.addr >= loading_addr);
      func_info.rva_to_base = func_info.addr - loading_addr;
      func_info.size = 0;
      std::vector<ElfW(Addr)>::iterator boundary
        = std::upper_bound(boundaries.begin(), boundaries.end(),
                           func_info.addr);
      ElfW(Addr) next_addr = (boundary == boundaries.end()) ? 0 : *boundary;
      // I've noticed functions with an address bigger than any other functions
      // and source files modules, this is probably the last function in the
      // module, due to limitions of Linux stab symbol, it is impossible to get
      // the exact size of this kind of function, thus we give it a default
      // very big value. This should be safe since this is the last function.
      // But it is a ugly hack.....
      // The following code can reproduce the case:
      // template<class T>
      // void Foo(T value) {
      // }
      //
      // int main(void) {
      //   Foo(10);
      //   Foo(std::string("hello"));
      //   return 0;
      // }
      // TODO(liuli): Find a better solution.
      static const int kDefaultSize = 0x10000000;
      if (next_addr != 0) {
        func_info.size = next_addr - func_info.addr;
      } else {
        if (no_next_addr_count > 1) {
          fprintf(stderr, "Got more than one funtion without the \
                  following symbol. Igore this function.\n");
          fprintf(stderr, "The dumped symbol may not correct.\n");
          assert(!"This should not happen!\n");
          func_info.size = 0;
          continue;
        }

        no_next_addr_count++;
        func_info.size = kDefaultSize;
      }
      // Compute line size.
      for (line_it = func_info.line_info.begin(); 
	   line_it != func_info.line_info.end(); line_it++) {
        struct LineInfo &line_info = *line_it;
	LineInfoList::iterator next_line_it = line_it;
	next_line_it++;
        line_info.size = 0;
        if (next_line_it != func_info.line_info.end()) {
          line_info.size =
            next_line_it->rva_to_func - line_info.rva_to_func;
        } else {
          // The last line in the function.
          // If we can find a function or source file symbol immediately
          // following the line, we can get the size of the line by computing
          // the difference of the next address to the starting address of this
          // line.
          // Otherwise, we need to set a default big enough value. This occurs
          // mostly because the this function is the last one in the module.
          if (next_addr != 0) {
            ElfW(Off) next_addr_offset = next_addr - func_info.addr;
            line_info.size = next_addr_offset - line_info.rva_to_func;
          } else {
            line_info.size = kDefaultSize;
          }
        }
        line_info.rva_to_base = line_info.rva_to_func + func_info.rva_to_base;
      }  // for each line.
    }  // for each function.
  } // for each source file.
  return true;
}

static bool LoadSymbols(const ElfW(Shdr) *stab_section,
                        const ElfW(Shdr) *stabstr_section,
                        ElfW(Addr) loading_addr,
                        struct SymbolInfo *symbols) {
  if (stab_section == NULL || stabstr_section == NULL)
    return false;

  struct nlist *lists =
    reinterpret_cast<struct nlist *>(stab_section->sh_offset);
  int nstab = stab_section->sh_size / sizeof(struct nlist);
  // First pass, load all symbols from the object file.
  for (int i = 0; i < nstab; ) {
    int step = 1;
    struct nlist *cur_list = lists + i;
    if (cur_list->n_type == N_SO) {
      if (cur_list->n_un.n_strx) {
        const char *name = reinterpret_cast<char *>(cur_list->n_un.n_strx
                                                 + stabstr_section->sh_offset);
        struct SourceFileInfo *source_file_info
            = FindSourceFileInfo(symbols, name);
        // Add it to the list; use ADDR to tell whether we've already done so.
        if (! source_file_info->addr)
          symbols->main_files.push_back(source_file_info);
        source_file_info->addr = cur_list->n_value;
        step = LoadFuncSymbols(cur_list, lists + nstab, symbols,
                               source_file_info, stabstr_section);
      } else {
        // N_SO entries with no name mark file boundary addresses.
        symbols->file_boundaries.push_back(cur_list->n_value);
      }
    }
    i += step;
  }

  // Second pass, compute the size of functions and lines.
  return ComputeSizeAndRVA(loading_addr, symbols);
}

static bool LoadSymbols(ElfW(Ehdr) *elf_header, struct SymbolInfo *symbols) {
  // Translate all offsets in section headers into address.
  FixAddress(elf_header);
  ElfW(Addr) loading_addr = GetLoadingAddress(
      reinterpret_cast<ElfW(Phdr) *>(elf_header->e_phoff),
      elf_header->e_phnum);

  const ElfW(Shdr) *sections =
    reinterpret_cast<ElfW(Shdr) *>(elf_header->e_shoff);
  const ElfW(Shdr) *strtab = sections + elf_header->e_shstrndx;
  const ElfW(Shdr) *stab_section =
    FindSectionByName(kStabName, sections, strtab, elf_header->e_shnum);
  if (stab_section == NULL) {
    fprintf(stderr, "Stab section not found.\n");
    return false;
  }
  const ElfW(Shdr) *stabstr_section = stab_section->sh_link + sections;

  // Load symbols.
  return LoadSymbols(stab_section, stabstr_section, loading_addr, symbols);
}

static bool WriteModuleInfo(FILE *file,
                            ElfW(Half) arch,
                            const std::string &obj_file) {
  const char *arch_name = NULL;
  if (arch == EM_386)
    arch_name = "x86";
  else if (arch == EM_X86_64)
    arch_name = "x86_64";
  else
    return false;

  unsigned char identifier[16];
  google_breakpad::FileID file_id(obj_file.c_str());
  if (file_id.ElfFileIdentifier(identifier)) {
    char identifier_str[40];
    file_id.ConvertIdentifierToString(identifier,
                                      identifier_str, sizeof(identifier_str));
    char id_no_dash[40];
    int id_no_dash_len = 0;
    memset(id_no_dash, 0, sizeof(id_no_dash));
    for (int i = 0; identifier_str[i] != '\0'; ++i)
      if (identifier_str[i] != '-')
        id_no_dash[id_no_dash_len++] = identifier_str[i];
    // Add an extra "0" by the end.
    id_no_dash[id_no_dash_len++] = '0';
    std::string filename = obj_file;
    size_t slash_pos = obj_file.find_last_of("/");
    if (slash_pos != std::string::npos)
      filename = obj_file.substr(slash_pos + 1);
    return 0 <= fprintf(file, "MODULE Linux %s %s %s\n", arch_name,
                        id_no_dash, filename.c_str());
  }
  return false;
}

// Set *INCLUDED_FILES to the list of included files in SYMBOLS,
// ordered appropriately for output.  Included files should appear in
// the order in which they are first referenced by source line info.
// Assign these files source id numbers starting with NEXT_SOURCE_ID.
//
// Note that the name_to_file map may contain #included files that are
// unreferenced; these are the result of LoadFuncSymbols omitting
// functions from the list whose addresses fall outside the address
// range of the file that contains them.
static void CollectIncludedFiles(const struct SymbolInfo &symbols,
                                 std::vector<SourceFileInfo *> *included_files,
                                 int next_source_id) {
  for (SourceFileInfoList::const_iterator file_it = symbols.main_files.begin();
       file_it != symbols.main_files.end(); file_it++) {
    for (FuncInfoList::const_iterator func_it = (*file_it)->func_info.begin();
         func_it != (*file_it)->func_info.end(); func_it++) {
      for (LineInfoList::const_iterator line_it = func_it->line_info.begin();
           line_it != func_it->line_info.end(); line_it++) {
        SourceFileInfo *file = line_it->file;
        if (file->source_id == -1) {
          file->source_id = next_source_id++;
          // Here we use the source id as a mark, ensuring that each
          // file appears in the list only once.
          included_files->push_back(file);
        }
      }
    }
  }
}

// Write 'FILE' lines for all source files in SYMBOLS to FILE.  We
// assign source id numbers to files here.
static bool WriteSourceFileInfo(FILE *file, struct SymbolInfo &symbols) {
  int next_source_id = 0;
  // Assign source id numbers to main files, and write them out to the file.
  for (SourceFileInfoList::iterator file_it = symbols.main_files.begin();
       file_it != symbols.main_files.end(); file_it++) {
    SourceFileInfo *file_info = *file_it;
    assert(file_info->addr);
    // We only output 'FILE' lines for main files if their names
    // contain '.'.  The extensionless C++ header files are #included,
    // not main files, so it wouldn't affect them.  If you know the
    // story, please patch this comment.
    if (strchr(file_info->name, '.')) {
      file_info->source_id = next_source_id++;
      if (0 > fprintf(file, "FILE %d %s\n",
                      file_info->source_id, file_info->name))
        return false;
    }
  }
  // Compute the list of included files, and write them out.
  // Can't use SourceFileInfoList here, because that owns the files it
  // points to.
  std::vector<SourceFileInfo *> included_files;
  std::vector<SourceFileInfo *>::const_iterator file_it;
  CollectIncludedFiles(symbols, &included_files, next_source_id);
  for (file_it = included_files.begin(); file_it != included_files.end();
       file_it++) {
    if (0 > fprintf(file, "FILE %d %s\n",
                    (*file_it)->source_id, (*file_it)->name))
      return false;
  }
  return true;
}

static bool WriteOneFunction(FILE *file,
                             const struct FuncInfo &func_info){
  // Discard the ending part of the name.
  std::string func_name(func_info.name);
  std::string::size_type last_colon = func_name.find_last_of(':');
  if (last_colon != std::string::npos)
    func_name = func_name.substr(0, last_colon);
  func_name = Demangle(func_name.c_str());

  if (func_info.size <= 0)
    return true;

  if (0 <= fprintf(file, "FUNC %lx %lx %d %s\n",
                   (unsigned long) func_info.rva_to_base,
                   (unsigned long) func_info.size,
                   func_info.stack_param_size,
                   func_name.c_str())) {
    for (LineInfoList::const_iterator it = func_info.line_info.begin();
	 it != func_info.line_info.end(); it++) {
      const struct LineInfo &line_info = *it;
      if (0 > fprintf(file, "%lx %lx %d %d\n",
                      (unsigned long) line_info.rva_to_base,
                      (unsigned long) line_info.size,
                      line_info.line_num,
                      line_info.file->source_id))
        return false;
    }
    return true;
  }
  return false;
}

static bool WriteFunctionInfo(FILE *file, const struct SymbolInfo &symbols) {
  for (SourceFileInfoList::const_iterator it = symbols.main_files.begin();
       it != symbols.main_files.end(); it++) {
    const struct SourceFileInfo &file_info = **it;
    for (FuncInfoList::const_iterator fiIt = file_info.func_info.begin(); 
	 fiIt != file_info.func_info.end(); fiIt++) {
      const struct FuncInfo &func_info = *fiIt;
      if (!WriteOneFunction(file, func_info))
        return false;
    }
  }
  return true;
}

static bool DumpStabSymbols(FILE *file, struct SymbolInfo &symbols) {
  return WriteSourceFileInfo(file, symbols) &&
    WriteFunctionInfo(file, symbols);
}

//
// FDWrapper
//
// Wrapper class to make sure opened file is closed.
//
class FDWrapper {
 public:
  explicit FDWrapper(int fd) :
    fd_(fd) {
    }
  ~FDWrapper() {
    if (fd_ != -1)
      close(fd_);
  }
  int get() {
    return fd_;
  }
  int release() {
    int fd = fd_;
    fd_ = -1;
    return fd;
  }
 private:
  int fd_;
};

//
// MmapWrapper
//
// Wrapper class to make sure mapped regions are unmapped.
//
class MmapWrapper {
  public:
   MmapWrapper(void *mapped_address, size_t mapped_size) :
     base_(mapped_address), size_(mapped_size) {
   }
   ~MmapWrapper() {
     if (base_ != NULL) {
       assert(size_ > 0);
       munmap(base_, size_);
     }
   }
   void release() {
     base_ = NULL;
     size_ = 0;
   }

  private:
   void *base_;
   size_t size_;
};

}  // namespace

namespace google_breakpad {

bool DumpSymbols::WriteSymbolFile(const std::string &obj_file,
                                  FILE *sym_file) {
  int obj_fd = open(obj_file.c_str(), O_RDONLY);
  if (obj_fd < 0)
    return false;
  FDWrapper obj_fd_wrapper(obj_fd);
  struct stat st;
  if (fstat(obj_fd, &st) != 0 && st.st_size <= 0)
    return false;
  void *obj_base = mmap(NULL, st.st_size,
                        PROT_READ | PROT_WRITE, MAP_PRIVATE, obj_fd, 0);
  if (obj_base == MAP_FAILED)
    return false;
  MmapWrapper map_wrapper(obj_base, st.st_size);
  ElfW(Ehdr) *elf_header = reinterpret_cast<ElfW(Ehdr) *>(obj_base);
  if (!IsValidElf(elf_header))
    return false;
  struct SymbolInfo symbols;

  if (!LoadSymbols(elf_header, &symbols))
     return false;
  // Write to symbol file.
  if (WriteModuleInfo(sym_file, elf_header->e_machine, obj_file) &&
      DumpStabSymbols(sym_file, symbols))
    return true;

  return false;
}

}  // namespace google_breakpad
