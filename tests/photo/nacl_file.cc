/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This is a very basic file interface for Native Client.  It allows
// Javascript to register a file which can then be read in C code via
// the standard file interface; for example fopen(), fread(), fclose().
//
// Note: At some point in the future, Native Client will have a more
// robust way of dealing with files.  In the meantime, this interface
// is intended to be used only for simple demos.
//
// To load local files, the simple python file server should also be
// running in the background.  See the native_client/tools/httpd.py script.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <nacl/nacl_srpc.h>

#define MAX_NACL_FILES  16

extern "C" int srpc_get_fd();
extern "C" int __real_open(char const *pathname, int flags, int perms);
extern "C" int __wrap_open(char const *pathname, int flags, int perms);
extern "C" int __real_close(int dd);
extern "C" int __wrap_close(int dd);
extern "C" int __real_read(int, void *, size_t);
extern "C" int __wrap_read(int, void *, size_t);
extern "C" off_t __real_lseek(int, off_t, int);
extern "C" off_t __wrap_lseek(int dd, off_t offset, int whence);

static pthread_mutex_t nacl_file_mu = PTHREAD_MUTEX_INITIALIZER;
#if defined(PTHREAD_COND_INITIALIZER)
static pthread_cond_t nacl_file_cv = PTHREAD_COND_INITIALIZER;
#elif defined(PTHREAD_COND_INITIALIZER_NP)
static pthread_cond_t nacl_file_cv = PTHREAD_COND_INITIALIZER_NP;
#else
# error "typo workaround failed"
#endif

struct nacl_file_map {
  struct nacl_file_map  *next;
  char                  *filename;
  int                   fd;
};
static struct nacl_file_map *nacl_file_map_list = NULL;
static int nacl_file_done = 0;
static int nacl_file_embedded = -1;


#define NACL_SRPC_HANDLER(signature, name) \
  extern "C" NaClSrpcError name( \
      NaClSrpcChannel* channel, \
      NaClSrpcArg **in_args, \
      NaClSrpcArg **out_args); \
  NACL_SRPC_METHOD(signature, name); \
  NaClSrpcError name( \
      NaClSrpcChannel* channel, \
      NaClSrpcArg **in_args, \
      NaClSrpcArg **out_args)

NACL_SRPC_HANDLER("file:shi:", NaClFile) {
  char *pathname;
  int fd;
  int last;
  struct nacl_file_map *entry;

  pathname = in_args[0]->u.sval;
  fd = in_args[1]->u.ival;
  last = in_args[2]->u.hval;

  printf("NaClFile(%s,%d,%d)\n", pathname, fd, last);

  entry = reinterpret_cast<nacl_file_map*>(malloc(sizeof *entry));

  if (NULL == entry) {
    fprintf(stderr, "No memory for file map for %s\n", pathname);
    exit(1);
  }
  if (NULL == (entry->filename = strdup(pathname))) {
    fprintf(stderr, "No memory for file path %s\n", pathname);
    exit(1);
  }
  entry->fd = fd;

  printf("Locking\n");
  pthread_mutex_lock(&nacl_file_mu);
  entry->next = nacl_file_map_list;
  nacl_file_map_list = entry;
  nacl_file_done = last;
  pthread_cond_broadcast(&nacl_file_cv);
  pthread_mutex_unlock(&nacl_file_mu);
  printf("Unlocked, exit check\n");

  return NACL_SRPC_RESULT_OK;
}

static pthread_mutex_t nacl_fake_mu = PTHREAD_MUTEX_INITIALIZER;
static int nacl_fake_initialized = 0;

struct NaCl_fake_file {
  int real_fd;
  off_t pos;
  pthread_mutex_t mu;
};

static struct NaCl_fake_file nacl_files[MAX_NACL_FILES];

int __wrap_open(char const *pathname, int mode, int perms) {
  int found = 0;
  int d = -1;
  struct nacl_file_map *entry;
  unsigned int i;
  int dd;

  if (-1 == nacl_file_embedded) {
    nacl_file_embedded = srpc_get_fd() != -1;
  }

  if (!nacl_file_embedded) {
    return __real_open(pathname, mode, perms);
  }

  if (mode != O_RDONLY) {
    return -1;
  }
  pthread_mutex_lock(&nacl_file_mu);
  while (!found) {
    for (entry = nacl_file_map_list; NULL != entry; entry = entry->next) {
      if (!strcmp(pathname, entry->filename)) {
        found = 1;
        d = entry->fd;
        break;
      }
    }
    if (!found) {
      if (!nacl_file_done) {
        pthread_cond_wait(&nacl_file_cv, &nacl_file_mu);
      } else {
        break;
      }
    }
  }
  pthread_mutex_unlock(&nacl_file_mu);

  if (!found) {
    return -1;
  }
  pthread_mutex_lock(&nacl_fake_mu);
  dd = -1;
  if (!nacl_fake_initialized) {
    for (i = 0; i < sizeof(nacl_files)/sizeof(nacl_files[0]); ++i) {
      nacl_files[i].real_fd = -1;
      pthread_mutex_init(&nacl_files[i].mu, NULL);
    }
    nacl_fake_initialized = 1;
  }
  for (i = 0; i < sizeof(nacl_files)/sizeof(nacl_files[0]); ++i) {
    if (-1 == nacl_files[i].real_fd) {
      nacl_files[i].real_fd = d;
      nacl_files[i].pos = 0;
      dd = i;
      break;
    }
  }
  pthread_mutex_unlock(&nacl_fake_mu);
  return dd;
}

int __wrap_close(int dd) {
  // struct nacl_file_map *entry;

  if (-1 == nacl_file_embedded) {
    nacl_file_embedded = srpc_get_fd() != -1;
  }
  if (!nacl_file_embedded) {
    return __real_close(dd);
  }
  pthread_mutex_lock(&nacl_file_mu);
  nacl_files[dd].real_fd = -1;
  pthread_mutex_unlock(&nacl_file_mu);
  return 0;
}

int __wrap_read(int dd, void *buf, size_t count) {
  int got;
  if (-1 == nacl_file_embedded) {
    nacl_file_embedded = srpc_get_fd() != -1;
  }
  if (!nacl_file_embedded) {
    return __real_read(dd, buf, count);
  }
  if ((dd < 0) ||
      (dd >= static_cast<int>(sizeof(nacl_files) / sizeof(nacl_files[0]))) ||
      (-1 == nacl_files[dd].real_fd)) {
    errno = EBADF;
    return -1;
  }
  pthread_mutex_lock(&nacl_files[dd].mu);
  (void) __real_lseek(nacl_files[dd].real_fd, nacl_files[dd].pos, SEEK_SET);
  got = __real_read(nacl_files[dd].real_fd, buf, count);
  if (got > 0) {
    nacl_files[dd].pos += got;
  }
  pthread_mutex_unlock(&nacl_files[dd].mu);
  return got;
}

off_t __wrap_lseek(int dd, off_t offset, int whence) {
  if (-1 == nacl_file_embedded) {
    nacl_file_embedded = srpc_get_fd() != -1;
  }
  if (!nacl_file_embedded) {
    return __real_lseek(dd, offset, whence);
  }
  if ((dd < 0) ||
      (dd >= static_cast<int>(sizeof(nacl_files) / sizeof(nacl_files[0]))) ||
      (-1 == nacl_files[dd].real_fd)) {
    errno = EBADF;
    return -1;
  }
  pthread_mutex_lock(&nacl_files[dd].mu);
  switch (whence) {
    case SEEK_SET:
      offset = __real_lseek(nacl_files[dd].real_fd, offset, SEEK_SET);
      break;
    case SEEK_CUR:
      offset = nacl_files[dd].pos + offset;
      offset = __real_lseek(nacl_files[dd].real_fd, offset, SEEK_SET);
      break;
    case SEEK_END:
      offset = __real_lseek(nacl_files[dd].real_fd, offset, SEEK_END);
      break;
  }
  if (-1 != offset) {
    nacl_files[dd].pos = offset;
  }
  pthread_mutex_unlock(&nacl_files[dd].mu);
  return offset;
}
