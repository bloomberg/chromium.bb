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
#include "native_client/tests/irt_ext/libc/libc_test.h"

#define TEST_DIRECTORY "test_directory"
#define TEST_FILE "test_file.txt"

static const char TEST_TEXT[] = "test text";

typedef int (*TYPE_file_test)(struct file_desc_environment *file_desc_env);

/* Directory tests. */
static int do_mkdir_rmdir_test(struct file_desc_environment *file_desc_env) {
  struct inode_data *parent_dir = NULL;
  struct inode_data *test_dir = NULL;

  if (0 != mkdir(TEST_DIRECTORY, S_IRWXO)) {
    irt_ext_test_print("Could not create directory: %s\n",
                       strerror(errno));
    return 1;
  }

  test_dir = find_inode_path(file_desc_env, "/" TEST_DIRECTORY, &parent_dir);
  if (test_dir == NULL) {
    irt_ext_test_print("mkdir: dir was not successfully created.\n");
    return 1;
  }

  if (0 != rmdir(TEST_DIRECTORY))  {
    irt_ext_test_print("Could not remove directory: %s\n",
                       strerror(errno));
    return 1;
  }

  test_dir = find_inode_path(file_desc_env, "/" TEST_DIRECTORY, &parent_dir);
  if (test_dir != NULL) {
    irt_ext_test_print("rmdir: dir was not successfully removed.\n");
    return 1;
  }

  return 0;
}

static int do_chdir_test(struct file_desc_environment *file_desc_env) {
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
  if (strcmp(file_desc_env->current_dir->name, TEST_DIRECTORY) != 0) {
    irt_ext_test_print("do_chdir_test: directory change failed.\n");
    return 1;
  }

  if (0 != chdir("/")) {
    irt_ext_test_print("Could not change to root directory: %s\n",
                       strerror(errno));
    return 1;
  }
  if (file_desc_env->current_dir->name[0] != '\0') {
    irt_ext_test_print("do_chdir_test: directory was not changed to root.\n");
    return 1;
  }

  return 0;
}

static int do_cwd_test(struct file_desc_environment *file_desc_env) {
  char buffer[512];
  struct inode_data test_dir_node;

  /* Create a dummy directory on the stack to test cwd. */
  init_inode_data(&test_dir_node);
  test_dir_node.mode = S_IFDIR;
  strncpy(test_dir_node.name, TEST_DIRECTORY, sizeof(test_dir_node.name));

  /* Change the current directory to the dummy test directory. */
  test_dir_node.parent_dir = file_desc_env->current_dir;
  file_desc_env->current_dir = &test_dir_node;

  if (buffer != getcwd(buffer, sizeof(buffer)) ||
      strcmp(buffer, "/" TEST_DIRECTORY) != 0) {
    irt_ext_test_print("do_cwd_test: getcwd returned unexpected dir: %s\n",
                       buffer);
    return 1;
  }

  return 0;
}

/* File IO tests. */
static int do_fopenclose_test(struct file_desc_environment *file_desc_env) {
  FILE *fp = NULL;
  int fd = -1;
  struct inode_data *file_node_parent = NULL;
  struct inode_data *file_node = NULL;

  fp = fopen(TEST_FILE, "w");
  if (fp == NULL ) {
    irt_ext_test_print("do_fopenclose_test: fopen was not successful - %s.\n",
                       strerror(errno));
    return 1;
  }

  file_node = find_inode_path(file_desc_env, TEST_FILE, &file_node_parent);
  if (file_node == NULL) {
    irt_ext_test_print("do_fopenclose_test: did not create inode.\n");
    return 1;
  }

  fd = fileno(fp);
  if (fd < 0 ||
      fd >= NACL_ARRAY_SIZE(file_desc_env->file_descs) ||
      !file_desc_env->file_descs[fd].valid ||
      file_desc_env->file_descs[fd].data != file_node) {
    irt_ext_test_print("do_fopenclose_test: file descriptor (%d) invalid.\n",
                       fd);
    return 1;
  }

  fclose(fp);
  if (file_desc_env->file_descs[fd].valid) {
    irt_ext_test_print("do_fopenclose_test: did not close file descriptor.\n");
    return 1;
  }

  return 0;
}

static int do_fwriteread_test(struct file_desc_environment *file_desc_env) {
  char buffer[512];
  size_t num_bytes;
  FILE *fp = NULL;
  struct inode_data *file_node_parent = NULL;
  struct inode_data *file_node = NULL;

  fp = fopen(TEST_FILE, "w+");
  if (fp == NULL ) {
    irt_ext_test_print("do_fwriteread_test: fopen was not successful - %s.\n",
                       strerror(errno));
    return 1;
  }

  file_node = find_inode_path(file_desc_env, TEST_FILE, &file_node_parent);
  if (file_node == NULL) {
    irt_ext_test_print("do_fwriteread_test: did not create inode.\n");
    return 1;
  }

  num_bytes = fwrite(TEST_TEXT, sizeof(TEST_TEXT[0]), sizeof(TEST_TEXT), fp);
  if (num_bytes != sizeof(TEST_TEXT)) {
    irt_ext_test_print("do_fwriteread_test: fwrite was not successful.\n");
    return 1;
  }

  if (0 != fseek(fp, 0, SEEK_SET)) {
    irt_ext_test_print("do_fwriteread_test: fseek was not successful - %s.\n",
                       strerror(errno));
    return 1;
  }

  num_bytes = fread(buffer, sizeof(TEST_TEXT[0]), sizeof(TEST_TEXT), fp);
  if (num_bytes != sizeof(TEST_TEXT)) {
    irt_ext_test_print("do_fwriteread_test: fread was not successful.\n");
    return 1;
  }

  if (strcmp(buffer, TEST_TEXT) != 0) {
    irt_ext_test_print("do_fwriteread_test: read/write text does not match.\n");
    return 1;
  }

  if (strcmp(file_node->content, TEST_TEXT) != 0) {
    irt_ext_test_print("do_fwriteread_test: inode content does not match.\n");
    return 1;
  }

  return 0;
}

/* Standard stream tests. */
static int do_isatty_test(struct file_desc_environment *file_desc_env) {
  if (!isatty(STDIN_FILENO) ||
      !isatty(STDOUT_FILENO) ||
      !isatty(STDERR_FILENO)) {
    irt_ext_test_print("do_istty_test: not all standard streams are a tty.\n");
    return 1;
  }

  deactivate_file_desc_env();
  if (isatty(STDIN_FILENO) ||
      isatty(STDOUT_FILENO) ||
      isatty(STDERR_FILENO)) {
    irt_ext_test_print("do_istty_test: valid tty after deactivating env.\n");
    return 1;
  }

  return 0;
}

static int do_printf_stream_test(struct file_desc_environment *file_desc_env) {
  const struct inode_data *stdout_data =
      file_desc_env->file_descs[STDOUT_FILENO].data;

  printf(TEST_TEXT);
  fflush(stdout);
  if (strncmp(stdout_data->content, TEST_TEXT,
              NACL_ARRAY_SIZE(TEST_TEXT) - 1) != 0) {
    irt_ext_test_print("do_printf_test: printf did not output to test env.\n");
    return 1;
  }

  return 0;
}

static int do_fprintf_stream_test(struct file_desc_environment *file_desc_env) {
  const struct inode_data *stdout_data =
      file_desc_env->file_descs[STDOUT_FILENO].data;
  const struct inode_data *stderr_data =
      file_desc_env->file_descs[STDERR_FILENO].data;

  fprintf(stdout, TEST_TEXT);
  fflush(stdout);
  if (strncmp(stdout_data->content, TEST_TEXT,
              NACL_ARRAY_SIZE(TEST_TEXT) - 1) != 0) {
    irt_ext_test_print("do_fprintf_stream_test: fprintf(stdout) did not output"
                       " to test env.\n");
    return 1;
  }

  fprintf(stderr, TEST_TEXT);
  if (strncmp(stderr_data->content, TEST_TEXT,
              NACL_ARRAY_SIZE(TEST_TEXT) - 1) != 0) {
    irt_ext_test_print("do_fprintf_stream_test: fprintf(stderr) did not output"
                       " to test env.\n");
    return 1;
  }

  return 0;
}

static int do_fread_stream_test(struct file_desc_environment *file_desc_env) {
  struct inode_data *stdin_data = file_desc_env->file_descs[STDIN_FILENO].data;
  char buffer[512];

  strcpy(stdin_data->content, TEST_TEXT);
  stdin_data->size = NACL_ARRAY_SIZE(TEST_TEXT);

  fread(buffer, sizeof(buffer[0]), sizeof(buffer), stdin);

  if (strcmp(buffer, TEST_TEXT) != 0) {
    irt_ext_test_print("do_fread_stream_test: fread(stdin) did not match"
                       " expected test text.\n");
    return 1;
  }

  return 0;
}

/*
 * These tests should not be in alphabetical order but ordered by complexity,
 * simpler tests should break first. For example, changing to a directory
 * depends on being able to make a directory first, so the make directory test
 * should be run first.
 */
static const TYPE_file_test g_file_tests[] = {
  /* Directory tests. */
  do_mkdir_rmdir_test,
  do_chdir_test,
  do_cwd_test,

  /* File IO tests. */
  do_fopenclose_test,
  do_fwriteread_test,

  /* Standard stream tests. */
  do_isatty_test,
  do_printf_stream_test,
  do_fprintf_stream_test,
  do_fread_stream_test,
};

int run_file_tests(void) {
  struct file_desc_environment file_desc_env;
  int num_failures = 0;

  irt_ext_test_print("Running %d file tests...\n",
                     NACL_ARRAY_SIZE(g_file_tests));

  for (int i = 0; i < NACL_ARRAY_SIZE(g_file_tests); i++) {
    init_file_desc_environment(&file_desc_env);
    activate_file_desc_env(&file_desc_env);

    if (0 != g_file_tests[i](&file_desc_env)) {
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
