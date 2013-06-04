/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/public/name_service.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_interfaces.h"
#include "native_client/src/untrusted/irt/irt_private.h"

__thread int g_is_main_thread = 0;

static void print_error(const char *message) {
  write(2, message, strlen(message));
}

/*
 * We use separate mutex so that nameservice initialization can be moved to
 * another file. In this case name_service_mutex would not be public.
 */
/* Mutex to guard manifest channel initialization. */
static pthread_mutex_t manifest_service_mutex = PTHREAD_MUTEX_INITIALIZER;
static int manifest_channel_initialized = 0;
static struct NaClSrpcChannel manifest_channel;

static int get_manifest_channel_locked(struct NaClSrpcChannel **result) {
  int status;
  int manifest;
  int manifest_conn;
  if (manifest_channel_initialized) {
    *result = &manifest_channel;
    return 0;
  }
  *result = NULL;

  status = irt_nameservice_lookup("ManifestNameService", O_RDWR, &manifest);
  if (NACL_NAME_SERVICE_SUCCESS != status) {
    print_error("ManifestNameService lookup failed\n");
    return EIO;
  }

  if (-1 == manifest) {
    print_error("Manifest descriptor is invalid\n");
    return EIO;
  }
  manifest_conn = imc_connect(manifest);
  close(manifest);
  if (-1 == manifest_conn) {
    print_error("Can't connect to manifest service\n");
    return EIO;
  }
  if (!NaClSrpcClientCtor(&manifest_channel, manifest_conn)) {
    print_error("Can't create manifest srpc channel\n");
    return EIO;
  }
  *result = &manifest_channel;
  manifest_channel_initialized = 1;
  return 0;
}

int get_manifest_channel(struct NaClSrpcChannel **result) {
  int error;
  pthread_mutex_lock(&manifest_service_mutex);
  error = get_manifest_channel_locked(result);
  pthread_mutex_unlock(&manifest_service_mutex);
  return error;
}

char kFiles[] = "files/";
int kFilesLen = 6;
/*
 * Returns file descriptor or error code.
 */
static int irt_open_resource(const char *file, int *fd) {
  int status;
  struct NaClSrpcChannel *manifest_channel;
  if (g_is_main_thread) {
    return EDEADLK;
  }
  status = get_manifest_channel(&manifest_channel);
  if (0 != status) {
    return status;
  }
  char* path = malloc(strlen(file) + 1 + kFilesLen);
  strcpy(path, kFiles);
  if ('/' == file[0]) {
    strcat(path, file + 1);
  } else {
    strcat(path, file);
  }
  pthread_mutex_lock(&manifest_service_mutex);
  if (NACL_SRPC_RESULT_OK == NaClSrpcInvokeBySignature(
      manifest_channel, NACL_NAME_SERVICE_LOOKUP, path, O_RDONLY,
      &status, fd)) {
    status = 0;
  }
  pthread_mutex_unlock(&manifest_service_mutex);
  free(path);
  if (-1 == *fd) {
    return ENOENT;
  }
  return status;
}

const struct nacl_irt_resource_open nacl_irt_resource_open = {
  irt_open_resource
};
