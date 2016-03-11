// Copyright 2016 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/untrusted/pll_loader/pll_loader.h"


namespace {

template <typename VarType>
void CheckTlsVar(ModuleSet *modset, const char *name_of_getter,
                 int alignment, VarType expected_value) {
  auto getter = (VarType *(*)()) (uintptr_t) modset->GetSym(name_of_getter);
  ASSERT_NE(getter, NULL);
  VarType *var_ptr = getter();
  ASSERT_NE(var_ptr, NULL);
  ASSERT_EQ((uintptr_t) var_ptr & (alignment - 1), 0);
  ASSERT_EQ(*var_ptr, expected_value);
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "Usage: pll_loader_test <3 ELF files>\n");
    return 1;
  }

  const char *module_a_filename = argv[1];
  const char *module_b_filename = argv[2];
  const char *module_tls_filename = argv[3];

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

  modset.AddByFilename(module_tls_filename);

  modset.ResolveRefs();

  // Check that "addr_of_module_a_var" has been correctly relocated to
  // point to the other module.
  int **addr_of_module_a_var = (int **) modset.GetSym("addr_of_module_a_var");
  ASSERT_NE(addr_of_module_a_var, NULL);
  ASSERT_EQ(*addr_of_module_a_var, module_a_var);

  // Test that TLS variables are instantiated and aligned properly.
  CheckTlsVar<int>(&modset, "get_tls_var1", sizeof(int), 123);
  CheckTlsVar<int *>(&modset, "get_tls_var2", sizeof(int), module_a_var);
  CheckTlsVar<int>(&modset, "get_tls_var_aligned", 256, 345);
  CheckTlsVar<int>(&modset, "get_tls_bss_var1", sizeof(int), 0);
  CheckTlsVar<int>(&modset, "get_tls_bss_var_aligned", 256, 0);

  return 0;
}
