/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <nacl/nacl_srpc.h>
#include <sys/nacl_name_service.h>
#include <sys/nacl_syscalls.h>

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_interfaces.h"

static void print_error(const char *message) {
  write(2, message, strlen(message));
}

/*
 * TODO(halyavin): move to separate file because name service channel can be
 * useful for other APIs.
 */
/* Mutex to guard name service channel initialization. */
static pthread_mutex_t name_service_mutex = PTHREAD_MUTEX_INITIALIZER;
static int ns_channel_initialized = 0;
static struct NaClSrpcChannel ns_channel;

static int get_nameservice_channel_locked(struct NaClSrpcChannel **result) {
  int ns;
  int connected_socket;
  if (ns_channel_initialized) {
    *result = &ns_channel;
    return 0;
  }
  *result = 0;
  ns = -1;
  nacl_nameservice(&ns);
  if (-1 == ns) {
    print_error("Can't get name service descriptor\n");
    return EIO;
  }
  connected_socket = imc_connect(ns);
  if (-1 == connected_socket) {
    print_error("Can't connect to name service\n");
    return EIO;
  }
  close(ns);
  if (!NaClSrpcClientCtor(&ns_channel, connected_socket)) {
    print_error("Srpc client channel ctor failed\n");
    return EIO;
  }
  *result = &ns_channel;
  ns_channel_initialized = 1;
  return 0;
}

/*
 * Get name service channel.
 * If successfull, function sets pointer to name service channel and returns 0.
 * In case of error, function sets pointer to zero and returns error code.
 */
int get_nameservice_channel(struct NaClSrpcChannel **result) {
  int error;
  pthread_mutex_lock(&name_service_mutex);
  error = get_nameservice_channel_locked(result);
  pthread_mutex_unlock(&name_service_mutex);
  return error;
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
  struct NaClSrpcChannel *ns_channel;
  if (manifest_channel_initialized) {
    *result = &manifest_channel;
    return 0;
  }
  *result = 0;
  status = get_nameservice_channel(&ns_channel);
  if (0 != status) {
    return status;
  }
  if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeBySignature(
      ns_channel, NACL_NAME_SERVICE_LOOKUP, "ManifestNameService", O_RDWR,
      &status, &manifest)) {
    print_error("Nameservice lookup failed, status\n");
    return EIO;
  }
  if (-1 == manifest) {
    print_error("Manifest descriptor is invalid\n");
    return EIO;
  }
  manifest_conn = imc_connect(manifest);
  if (-1 == manifest_conn) {
    print_error("Can't connect to manifest service\n");
    return EIO;
  }
  close(manifest);
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

/*
 * Name enumeration.
 */
static pthread_mutex_t names_mutex = PTHREAD_MUTEX_INITIALIZER;
char *names_data = NULL;
int names_data_size;
char **names;
int names_size;

static void construct_names_pointers(void) {
  int alloc_size;
  int i;
  int len;
  char **new_names;
  alloc_size = 8;
  names = malloc(sizeof(char*) * alloc_size);
  names_size = 0;
  for (i = 0; i < names_data_size; ) {
    if (names_size == alloc_size) {
      new_names = malloc(sizeof(char*) * 2 * alloc_size);
      memmove(new_names, names, sizeof(char*) * alloc_size);
      free(names);
      names = new_names;
    }
    names[names_size] = names_data + i;
    names_size++;
    len = strlen(names_data + i);
    i += len + 1;
  }
}

static int load_names_lock(void) {
  int nbytes;
  struct NaClSrpcChannel *manifest_channel;
  int error;
  if (names_data != NULL) {
    return 0;
  }
  error = get_manifest_channel(&manifest_channel);
  if (0 != error) {
    return error;
  }
  nbytes = 4096;
  names_data = malloc(nbytes);
  while (1) {
    names_data_size = nbytes;
    if (NACL_SRPC_RESULT_OK !=
        NaClSrpcInvokeBySignature(manifest_channel, NACL_NAME_SERVICE_LIST,
                                  &names_data_size, names_data)) {
      print_error("manifest list RPC failed\n");
      return EIO;
    }
    if (names_data_size < nbytes - 1024) {
      break;
    }
    free(names_data);
    nbytes *= 2;
    names_data = malloc(nbytes);
  }
  construct_names_pointers();
  return 0;
}

static int load_names(void) {
  int status;
  pthread_mutex_lock(&names_mutex);
  status = load_names_lock();
  pthread_mutex_unlock(&names_mutex);
  return status;
}

static int resource_exists(const char *file, int *result) {
  int i;
  int status;
  status = load_names();
  if (status != 0) {
    return status;
  }
  for (i = 0; i < names_size; i++) {
    if (0 == strcmp(file, names[i])) {
      *result = 1;
      return 0;
    }
  }
  *result = 0;
  return 0;
}

char kFiles[] = "files/";
int kFilesLen = 6;
/*
 * Returns file descriptor or error code.
 */
static int irt_open_resource(const char *file, int *fd) {
  int status;
  int file_exists;
  struct NaClSrpcChannel *manifest_channel;
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
  status = resource_exists(path + kFilesLen, &file_exists);
  if (0 != status) {
    return status;
  }
  if (0 == file_exists) {
    return ENOENT;
  }
  pthread_mutex_lock(&manifest_service_mutex);
  if (NACL_SRPC_RESULT_OK == NaClSrpcInvokeBySignature(
      manifest_channel, NACL_NAME_SERVICE_LOOKUP, path, O_RDONLY,
      &status, fd)) {
    status = 0;
  }
  pthread_mutex_unlock(&manifest_service_mutex);
  free(path);
  return status;
}

const struct nacl_irt_resource_open nacl_irt_resource_open = {
  irt_open_resource
};
