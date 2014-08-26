/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/tests/irt_ext/error_report.h"
#include "native_client/tests/irt_ext/file_desc.h"
#include "native_client/tests/irt_ext/libc/file_tests.h"

#define TEST_DIRECTORY "test_directory"

typedef int (*TYPE_file_test)(void);
static struct file_desc_environment g_file_desc_env;

int do_mkdir_rmdir_test(void) {
  struct inode_data *parent_dir = NULL;
  struct inode_data *test_dir = NULL;

  if (0 != mkdir(TEST_DIRECTORY, S_IRWXO)) {
    irt_ext_test_print("Could not create directory: %s\n",
                       strerror(errno));
    return 1;
  }

  test_dir = find_inode_path(&g_file_desc_env, "/" TEST_DIRECTORY, &parent_dir);
  if (test_dir == NULL) {
    irt_ext_test_print("mkdir: dir was not successfully created.\n");
    return 1;
  }

  if (0 != rmdir(TEST_DIRECTORY))  {
    irt_ext_test_print("Could not remove directory: %s\n",
                       strerror(errno));
    return 1;
  }

  test_dir = find_inode_path(&g_file_desc_env, "/" TEST_DIRECTORY, &parent_dir);
  if (test_dir != NULL) {
    irt_ext_test_print("rmdir: dir was not successfully removed.\n");
    return 1;
  }

  return 0;
}

int do_chdir_test(void) {
  if (0 != mkdir(TEST_DIRECTORY, S_IRWXO)) {
    irt_ext_test_print("Could not create directory: %s\n",
                       strerror(errno));
    return 1;
  }

  if (0 != chdir(TEST_DIRECTORY)) {
    irt_ext_test_print("Could not change directory: %s\n",
                       strerror(errno));
    return 1;
  }
  if (strcmp(g_file_desc_env.current_dir->name, TEST_DIRECTORY) != 0) {
    irt_ext_test_print("do_chdir_test: directory change failed.\n");
    return 1;
  }

  if (0 != chdir("/")) {
    irt_ext_test_print("Could not change to root directory: %s\n",
                       strerror(errno));
    return 1;
  }
  if (g_file_desc_env.current_dir->name[0] != '\0') {
    irt_ext_test_print("do_chdir_test: directory was not changed to root.\n");
    return 1;
  }

  return 0;
}

int do_cwd_test(void) {
  char buffer[512];
  struct inode_data test_dir_node;

  /* Create a dummy directory on the stack to test cwd. */
  init_inode_data(&test_dir_node);
  test_dir_node.mode = S_IFDIR;
  strncpy(test_dir_node.name, TEST_DIRECTORY, sizeof(test_dir_node.name));

  /* Change the current directory to the dummy test directory. */
  test_dir_node.parent_dir = g_file_desc_env.current_dir;
  g_file_desc_env.current_dir = &test_dir_node;

  if (buffer != getcwd(buffer, sizeof(buffer)) ||
      strcmp(buffer, "/" TEST_DIRECTORY) != 0) {
    irt_ext_test_print("do_cwd_test: getcwd returned unexpected dir: %s\n",
                       buffer);
    return 1;
  }

  return 0;
}

static const TYPE_file_test g_file_tests[] = {
  do_mkdir_rmdir_test,
  do_chdir_test,
  do_cwd_test,
};

int run_file_tests(void) {
  int num_failures = 0;
  irt_ext_test_print("Running %d file tests...\n",
                     NACL_ARRAY_SIZE(g_file_tests));

  for (int i = 0; i < NACL_ARRAY_SIZE(g_file_tests); i++) {
    init_file_desc_environment(&g_file_desc_env);
    activate_file_desc_env(&g_file_desc_env);

    if (0 != g_file_tests[i]()) {
      num_failures++;
    }

    deactivate_file_desc_env();
  }

  if (num_failures > 0) {
    irt_ext_test_print("File Tests Failed [%d/%d]\n", num_failures,
                       NACL_ARRAY_SIZE(g_file_tests));
  } else {
    irt_ext_test_print("File Tests Passed [%d/%d]\n",
                       NACL_ARRAY_SIZE(g_file_tests),
                       NACL_ARRAY_SIZE(g_file_tests));
  }

  return num_failures;
}
