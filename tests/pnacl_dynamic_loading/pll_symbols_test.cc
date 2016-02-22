// Copyright (c) 2015 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/untrusted/pnacl_dynloader/dynloader.h"

// This tests the symbol tables created by the ConvertToPSO pass.

namespace {

const unsigned kFormatVersion = 2;

struct PLLRoot {
  size_t format_version;
  const char *string_table;

  // Exports.
  void *const *exported_ptrs;
  const size_t *exported_names;
  size_t export_count;

  // Imports.
  void *const *imported_ptrs;
  const size_t *imported_names;
  size_t import_count;

  // Hash Table, for quick exported symbol lookup.
  size_t bucket_count;
  const int32_t *hash_buckets;
  const uint32_t *hash_chains;
};

const char *GetExportedSymbolName(const PLLRoot *root, size_t i) {
  return root->string_table + root->exported_names[i];
}

const char *GetImportedSymbolName(const PLLRoot *root, size_t i) {
  return root->string_table + root->imported_names[i];
}

void DumpExportedSymbols(const PLLRoot *root) {
  for (size_t i = 0; i < root->export_count; i++) {
    printf("exported symbol: %s\n", GetExportedSymbolName(root, i));
  }
}

void DumpImportedSymbols(const PLLRoot *root) {
  for (size_t i = 0; i < root->import_count; i++) {
    printf("imported symbol: %s\n", GetImportedSymbolName(root, i));
  }
}

void *GetExportedSymSlow(const PLLRoot *root, const char *name) {
  for (size_t i = 0; i < root->export_count; i++) {
    if (strcmp(GetExportedSymbolName(root, i), name) == 0) {
      return root->exported_ptrs[i];
    }
  }
  return NULL;
}

uint32_t HashString(const char *sp) {
  uint32_t h = 5381;
  for (unsigned char c = *sp; c != '\0'; c = *++sp)
    h = h * 33 + c;
  return h;
}

void *GetExportedSymHash(const PLLRoot *root, const char *name) {
  uint32_t hash = HashString(name);
  uint32_t bucket_index = hash % root->bucket_count;
  int32_t chain_index = root->hash_buckets[bucket_index];
  // Bucket empty -- value not found.
  if (chain_index == -1)
    return NULL;

  for (; chain_index < root->export_count; chain_index++) {
    uint32_t chain_value = root->hash_chains[chain_index];
    if ((hash & ~1) == (chain_value & ~1) &&
        strcmp(name, GetExportedSymbolName(root, chain_index)) == 0)
      return root->exported_ptrs[chain_index];

    // End of chain -- value not found.
    if ((chain_value & 1) == 1)
      return NULL;
  }

  ASSERT(false);
}

void VerifyHashTable(const PLLRoot *root) {
  // Confirm that each entry in hash_chains[] contains a hash
  // corresponding with the hashes of "exported_names", in order.
  for (uint32_t chain_index = 0; chain_index < root->export_count;
       chain_index++) {
    uint32_t hash = HashString(GetExportedSymbolName(root, chain_index));
    ASSERT_EQ(hash & ~1, root->hash_chains[chain_index] & ~1);
  }

  // Confirm that each entry in hash_buckets[] is either -1 or a valid index
  // into hash_chains[].
  for (uint32_t bucket_index = 0; bucket_index < root->bucket_count;
       bucket_index++) {
    int32_t chain_index = root->hash_buckets[bucket_index];
    ASSERT_GE(chain_index, -1);
    ASSERT_LT(chain_index, root->export_count);

    if (chain_index != -1) {
      // For each chain marked in hash_buckets[], confirm that it is terminated
      // and the hash matches the hash_buckets[] index.
      bool chain_terminated = false;
      for (; chain_index < root->export_count; chain_index++) {
        uint32_t hash = HashString(GetExportedSymbolName(root, chain_index));
        ASSERT_EQ(bucket_index, hash % root->bucket_count);
        if ((root->hash_chains[chain_index] & 1) == 1) {
          chain_terminated = true;
          break;
        }
      }
      ASSERT(chain_terminated);
    }
  }
}

void *GetExportedSym(const PLLRoot *root, const char *name) {
  // There are two possible ways to get the exported symbol. First, by manually
  // scanning all exports, and secondly, by using the exported symbol hash
  // table. Use both methods, and assert that they provide an equivalent result.
  void *sym_slow = GetExportedSymSlow(root, name);
  void *sym_hash = GetExportedSymHash(root, name);
  ASSERT_EQ(sym_slow, sym_hash);
  return sym_slow;
}

void TestImportReloc(const PLLRoot *pll_root,
                     const char *imported_sym_name,
                     int import_addend,
                     const char *dest_name,
                     int offset_from_dest) {
  printf("Checking for relocation that assigns to \"%s+%d\" "
         "the value of \"%s+%d\"\n",
         dest_name, offset_from_dest, imported_sym_name, import_addend);

  void *dest_sym_addr = GetExportedSym(pll_root, dest_name);
  ASSERT_NE(dest_sym_addr, NULL);
  uintptr_t addr_to_modify = (uintptr_t) dest_sym_addr + offset_from_dest;

  // Check the addend.
  ASSERT_EQ(*(uintptr_t *) addr_to_modify, import_addend);

  // Search for a relocation that applies to the given target address.  We
  // do not require the relocations to appear in a specific order in the
  // import list.
  bool found = false;
  for (size_t index = 0; index < pll_root->import_count; index++) {
    if (pll_root->imported_ptrs[index] == (void *) addr_to_modify) {
      ASSERT(!found);
      found = true;

      // Check name of symbol being imported.
      ASSERT_EQ(strcmp(GetImportedSymbolName(pll_root, index),
                       imported_sym_name), 0);
    }
  }
  ASSERT(found);
}

// Resolves any imports that refer to |symbol_name| with the given value.
void ResolveReferenceToSym(const PLLRoot *pll_root,
                           const char *symbol_name,
                           uintptr_t value) {
  bool found = false;
  for (size_t index = 0; index < pll_root->import_count; index++) {
    if (strcmp(GetImportedSymbolName(pll_root, index), symbol_name) == 0) {
      found = true;
      *(uintptr_t *) pll_root->imported_ptrs[index] += value;
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
  const PLLRoot *pll_root = (PLLRoot *) pso_root;

  ASSERT_EQ(pll_root->format_version, kFormatVersion);

  VerifyHashTable(pll_root);

  // Test exports.

  DumpExportedSymbols(pll_root);

  ASSERT_EQ(GetExportedSym(pll_root, "does_not_exist"), NULL);

  int *var = (int *) GetExportedSym(pll_root, "var");
  ASSERT_NE(var, NULL);
  ASSERT_EQ(*var, 2345);

  int *(*get_var)(void) =
    (int *(*)(void)) (uintptr_t) GetExportedSym(pll_root, "get_var");
  ASSERT_NE(get_var, NULL);
  ASSERT_EQ(get_var(), var);

  void *example_func = GetExportedSym(pll_root, "example_func");
  ASSERT_NE(example_func, NULL);

  // For "var", "get_var" and "example_func".
  int expected_exports = 3;

  // Test imports referenced by variables.  We can test these directly, by
  // checking that the relocations refer to the correct addresses.

  DumpImportedSymbols(pll_root);

  TestImportReloc(pll_root, "imported_var", 0, "reloc_var", 0);
  TestImportReloc(pll_root, "imported_var_addend", sizeof(int),
                  "reloc_var_addend", 0);
  TestImportReloc(pll_root, "imported_var2", sizeof(int) * 100,
                  "reloc_var_offset", sizeof(int));
  TestImportReloc(pll_root, "imported_var3", sizeof(int) * 200,
                  "reloc_var_offset", sizeof(int) * 2);
  // For the 4 calls to TestImportReloc().
  int expected_imports = 4;
  // For "reloc_var", "reloc_var_addend" and "reloc_var_offset".
  expected_exports += 3;

  // Test that local (non-imported) relocations still work and that they
  // don't get mixed up with relocations for imports.
  int **local_reloc_var = (int **) GetExportedSym(pll_root, "local_reloc_var");
  ASSERT_EQ(**local_reloc_var, 1234);
  // For "local_reloc_var".
  expected_exports += 1;

  // Test imports referenced by functions.  We can only test these
  // indirectly, by checking that the functions' return values change when
  // we apply relocations.

  uintptr_t (*get_imported_var)() =
    (uintptr_t (*)()) (uintptr_t) GetExportedSym(pll_root, "get_imported_var");
  uintptr_t (*get_imported_var_addend)() =
    (uintptr_t (*)()) (uintptr_t) GetExportedSym(pll_root,
                                                 "get_imported_var_addend");
  uintptr_t (*get_imported_func)() =
    (uintptr_t (*)()) (uintptr_t) GetExportedSym(pll_root,
                                                 "get_imported_func");
  ASSERT_EQ(get_imported_var(), 0);
  ASSERT_EQ(get_imported_var_addend(), sizeof(int));
  ASSERT_EQ(get_imported_func(), 0);
  uintptr_t example_ptr1 = 0x100000;
  uintptr_t example_ptr2 = 0x200000;
  uintptr_t example_ptr3 = 0x300000;
  ResolveReferenceToSym(pll_root, "imported_var", example_ptr1);
  ResolveReferenceToSym(pll_root, "imported_var_addend", example_ptr2);
  ResolveReferenceToSym(pll_root, "imported_func", example_ptr3);
  ASSERT_EQ(get_imported_var(), example_ptr1);
  ASSERT_EQ(get_imported_var_addend(), example_ptr2 + sizeof(int));
  ASSERT_EQ(get_imported_func(), example_ptr3);
  // For "get_imported_var", "get_imported_var_addend" and "get_imported_func".
  expected_exports += 3;
  // For "imported_var", "imported_var_addend" and "imported_func".
  expected_imports += 3;

  ASSERT_EQ(pll_root->export_count, expected_exports);
  ASSERT_EQ(pll_root->import_count, expected_imports);

  return 0;
}
