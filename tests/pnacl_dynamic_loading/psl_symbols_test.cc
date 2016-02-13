// Copyright (c) 2015 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/untrusted/pnacl_dynloader/dynloader.h"

// This tests the symbol tables created by the ConvertToPSO pass.

namespace {

struct PSLRoot {
  const char *string_table;

  // Exports.
  void *const *exported_ptrs;
  const size_t *exported_names;
  size_t export_count;

  void *const *imported_ptrs;
  const size_t *imported_names;
  size_t import_count;
};

const char *GetExportedSymbolName(const PSLRoot *root, size_t i) {
  return root->string_table + root->exported_names[i];
}

const char *GetImportedSymbolName(const PSLRoot *root, size_t i) {
  return root->string_table + root->imported_names[i];
}

void DumpExportedSymbols(const PSLRoot *root) {
  for (size_t i = 0; i < root->export_count; i++) {
    printf("exported symbol: %s\n", GetExportedSymbolName(root, i));
  }
}

void DumpImportedSymbols(const PSLRoot *root) {
  for (size_t i = 0; i < root->import_count; i++) {
    printf("imported symbol: %s\n", GetImportedSymbolName(root, i));
  }
}

void *GetExportedSym(const PSLRoot *root, const char *name) {
  for (size_t i = 0; i < root->export_count; i++) {
    if (strcmp(GetExportedSymbolName(root, i), name) == 0) {
      return root->exported_ptrs[i];
    }
  }
  return NULL;
}

void TestImportReloc(const PSLRoot *psl_root,
                     const char *imported_sym_name,
                     int import_addend,
                     const char *dest_name,
                     int offset_from_dest) {
  printf("Checking for relocation that assigns to \"%s+%d\" "
         "the value of \"%s+%d\"\n",
         dest_name, offset_from_dest, imported_sym_name, import_addend);

  void *dest_sym_addr = GetExportedSym(psl_root, dest_name);
  ASSERT_NE(dest_sym_addr, NULL);
  uintptr_t addr_to_modify = (uintptr_t) dest_sym_addr + offset_from_dest;

  // Check the addend.
  ASSERT_EQ(*(uintptr_t *) addr_to_modify, import_addend);

  // Search for a relocation that applies to the given target address.  We
  // do not require the relocations to appear in a specific order in the
  // import list.
  bool found = false;
  for (size_t index = 0; index < psl_root->import_count; index++) {
    if (psl_root->imported_ptrs[index] == (void *) addr_to_modify) {
      ASSERT(!found);
      found = true;

      // Check name of symbol being imported.
      ASSERT_EQ(strcmp(GetImportedSymbolName(psl_root, index),
                       imported_sym_name), 0);
    }
  }
  ASSERT(found);
}

// Resolves any imports that refer to |symbol_name| with the given value.
void ResolveReferenceToSym(const PSLRoot *psl_root,
                           const char *symbol_name,
                           uintptr_t value) {
  bool found = false;
  for (size_t index = 0; index < psl_root->import_count; index++) {
    if (strcmp(GetImportedSymbolName(psl_root, index), symbol_name) == 0) {
      found = true;
      *(uintptr_t *) psl_root->imported_ptrs[index] += value;
    }
  }
  ASSERT(found);
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: dynloader_test <ELF file>...\n");
    return 1;
  }
  const char *test_dso_file = argv[1];

  printf("Testing %s...\n", test_dso_file);
  void *pso_root;
  int err = pnacl_load_elf_file(test_dso_file, &pso_root);
  ASSERT_EQ(err, 0);
  const PSLRoot *psl_root = (PSLRoot *) pso_root;

  // Test exports.

  DumpExportedSymbols(psl_root);

  ASSERT_EQ(GetExportedSym(psl_root, "does_not_exist"), NULL);

  int *var = (int *) GetExportedSym(psl_root, "var");
  ASSERT_NE(var, NULL);
  ASSERT_EQ(*var, 2345);

  int *(*get_var)(void) =
    (int *(*)(void)) (uintptr_t) GetExportedSym(psl_root, "get_var");
  ASSERT_NE(get_var, NULL);
  ASSERT_EQ(get_var(), var);

  void *example_func = GetExportedSym(psl_root, "example_func");
  ASSERT_NE(example_func, NULL);

  // For "var", "get_var" and "example_func".
  int expected_exports = 3;

  // Test imports referenced by variables.  We can test these directly, by
  // checking that the relocations refer to the correct addresses.

  DumpImportedSymbols(psl_root);

  TestImportReloc(psl_root, "imported_var", 0, "reloc_var", 0);
  TestImportReloc(psl_root, "imported_var_addend", sizeof(int),
                  "reloc_var_addend", 0);
  TestImportReloc(psl_root, "imported_var2", sizeof(int) * 100,
                  "reloc_var_offset", sizeof(int));
  TestImportReloc(psl_root, "imported_var3", sizeof(int) * 200,
                  "reloc_var_offset", sizeof(int) * 2);
  // For the 4 calls to TestImportReloc().
  int expected_imports = 4;
  // For "reloc_var", "reloc_var_addend" and "reloc_var_offset".
  expected_exports += 3;

  // Test that local (non-imported) relocations still work and that they
  // don't get mixed up with relocations for imports.
  int **local_reloc_var = (int **) GetExportedSym(psl_root, "local_reloc_var");
  ASSERT_EQ(**local_reloc_var, 1234);
  // For "local_reloc_var".
  expected_exports += 1;

  // Test imports referenced by functions.  We can only test these
  // indirectly, by checking that the functions' return values change when
  // we apply relocations.

  uintptr_t (*get_imported_var)() =
    (uintptr_t (*)()) (uintptr_t) GetExportedSym(psl_root, "get_imported_var");
  uintptr_t (*get_imported_var_addend)() =
    (uintptr_t (*)()) (uintptr_t) GetExportedSym(psl_root,
                                                 "get_imported_var_addend");
  uintptr_t (*get_imported_func)() =
    (uintptr_t (*)()) (uintptr_t) GetExportedSym(psl_root,
                                                 "get_imported_func");
  ASSERT_EQ(get_imported_var(), 0);
  ASSERT_EQ(get_imported_var_addend(), sizeof(int));
  ASSERT_EQ(get_imported_func(), 0);
  uintptr_t example_ptr1 = 0x100000;
  uintptr_t example_ptr2 = 0x200000;
  uintptr_t example_ptr3 = 0x300000;
  ResolveReferenceToSym(psl_root, "imported_var", example_ptr1);
  ResolveReferenceToSym(psl_root, "imported_var_addend", example_ptr2);
  ResolveReferenceToSym(psl_root, "imported_func", example_ptr3);
  ASSERT_EQ(get_imported_var(), example_ptr1);
  ASSERT_EQ(get_imported_var_addend(), example_ptr2 + sizeof(int));
  ASSERT_EQ(get_imported_func(), example_ptr3);
  // For "get_imported_var", "get_imported_var_addend" and "get_imported_func".
  expected_exports += 3;
  // For "imported_var", "imported_var_addend" and "imported_func".
  expected_imports += 3;

  ASSERT_EQ(psl_root->export_count, expected_exports);
  ASSERT_EQ(psl_root->import_count, expected_imports);

  return 0;
}
