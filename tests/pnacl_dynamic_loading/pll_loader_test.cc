// Copyright 2016 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/untrusted/pll_loader/pll_loader.h"


int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: pll_loader_test <ELF file> <ELF file>\n");
    return 1;
  }

  const char *module_a_filename = argv[1];
  const char *module_b_filename = argv[2];

  ModuleSet modset;

  // "module_a_var" should only be resolvable after we load module A.
  int *module_a_var = (int *) modset.GetSym("module_a_var");
  ASSERT_EQ(module_a_var, NULL);

  modset.AddByFilename(module_a_filename);
  module_a_var = (int *) modset.GetSym("module_a_var");
  ASSERT_NE(module_a_var, NULL);
  ASSERT_EQ(*module_a_var, 2345);

  modset.AddByFilename(module_b_filename);
  int *module_b_var = (int *) modset.GetSym("module_b_var");
  ASSERT_NE(module_b_var, NULL);
  ASSERT_EQ(*module_b_var, 1234);

  modset.ResolveRefs();

  // Check that "addr_of_module_a_var" has been correctly relocated to
  // point to the other module.
  int **addr_of_module_a_var = (int **) modset.GetSym("addr_of_module_a_var");
  ASSERT_NE(addr_of_module_a_var, NULL);
  ASSERT_EQ(*addr_of_module_a_var, module_a_var);

  return 0;
}
