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
  void *const *exported_ptrs;
  const char *const *exported_names;
  size_t export_count;
};

void DumpSymbols(const PSLRoot *root) {
  for (size_t i = 0; i < root->export_count; i++) {
    printf("symbol: %s\n", root->exported_names[i]);
  }
}

void *GetExportedSym(const PSLRoot *root, const char *name) {
  for (size_t i = 0; i < root->export_count; i++) {
    if (strcmp(root->exported_names[i], name) == 0) {
      return root->exported_ptrs[i];
    }
  }
  return NULL;
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

  ASSERT_EQ(psl_root->export_count, 3);

  DumpSymbols(psl_root);

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

  return 0;
}
