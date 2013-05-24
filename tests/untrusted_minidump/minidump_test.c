/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "native_client/src/include/nacl/nacl_minidump.h"
#include "native_client/src/untrusted/minidump_generator/build_id.h"


const char *g_minidump_filename;


/* TODO(mseaborn): Make build IDs work in the other toolchains. */
#if defined(__pnacl__) || defined(__arm__)
# define BUILD_ID_WORKS 1
#else
# define BUILD_ID_WORKS 0
#endif

static void crash_callback(const void *minidump_data, size_t size) {
  assert(size != 0);
  FILE *fp = fopen(g_minidump_filename, "wb");
  assert(fp != NULL);
  int written = fwrite(minidump_data, 1, size, fp);
  assert(written == size);
  int rc = fclose(fp);
  assert(rc == 0);

  fprintf(stderr, "** intended_exit_status=0\n");
  exit(0);
}

__attribute__((noinline))
void crash(void) {
  *(volatile int *) 1 = 1;
}

/* Use some nested function calls to test stack backtracing. */
__attribute__((noinline))
void crash_wrapper1(void) {
  crash();
}

__attribute__((noinline))
void crash_wrapper2(void) {
  crash_wrapper1();
}

int main(int argc, char **argv) {
  assert(argc == 2);
  g_minidump_filename = argv[1];

  const char *id_data;
  size_t id_size;
  int got_build_id = nacl_get_build_id(&id_data, &id_size);
  if (BUILD_ID_WORKS) {
    assert(got_build_id);
    assert(id_data != NULL);
    assert(id_size == 20);  /* ld uses SHA1 hash by default. */
  } else {
    uint8_t dummy_build_id[NACL_MINIDUMP_BUILD_ID_SIZE];
    memset(dummy_build_id, 0x12, sizeof(dummy_build_id));
    nacl_minidump_set_module_build_id(dummy_build_id);
  }

  nacl_minidump_set_callback(crash_callback);
  nacl_minidump_set_module_name("minidump_test.nexe");
  nacl_minidump_register_crash_handler();

  /* Cause crash. */
  crash_wrapper2();

  /* Should not reach here. */
  return 1;
}
