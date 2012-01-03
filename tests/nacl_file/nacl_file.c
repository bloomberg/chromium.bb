/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This file provides wrappers to open(2), read(2), etc. that read bytes from
 * an mmap()'ed buffer.  There are three steps required:
 *    1. Use linker aliasing to wrap open(), etc.  This is done in the
 *       Makefile using the "-XLinker --wrap -Xlinker open" arguments to
 *       nacl-gcc.  Note that this makes *all* calls to things like read() go
 *       through these wrappers, so if you also need to read() from, say, a
 *       socket, this code will not work as-is.
 *    2. When you have a file descriptor for the file, create a NaClFile()
 *       like this:
 *         NaCLFile("http://www.mywebsite.com/images/photo.jpg", fd,
 *                  size_of_file, 1);
 *    3. Use fopen(), open(), etc as you normally would for a file.
 * To see how this ties into NPN_GetURL(), see NPP_StreamAsFile() in
 * npp_gate.cc.
 *
 * Note: This code is very temporary and will disappear when the Pepper 2 API
 * is available in Native Client.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"

#define MAX_NACL_FILES  256

extern int srpc_get_fd(void);
extern int __real_open(char const *pathname, int flags, int perms);
extern int __real_close(int dd);
extern int __real_read(int, void *, size_t);
extern off_t __real_lseek(int, off_t, int);

static pthread_mutex_t nacl_file_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t nacl_file_cv = PTHREAD_COND_INITIALIZER;

struct nacl_file_map {
  struct nacl_file_map  *next;
  char                  *filename;
  int                   fd;
  uint32_t              size;
};

#define DBG_NULL (NULL)

static struct nacl_file_map *nacl_file_map_list = DBG_NULL;
static int nacl_file_done = 0;
static int nacl_file_embedded = -1;

static pthread_mutex_t nacl_fake_mu = PTHREAD_MUTEX_INITIALIZER;
static int nacl_fake_initialized = 0;

struct NaCl_fake_file {
  int real_fd;
  off_t pos;
  pthread_mutex_t mu;
  int mmap;
  size_t mmap_size;
};

static struct NaCl_fake_file nacl_files[MAX_NACL_FILES];

/* Check to see the |dd| is a valid NaCl file descriptor. */
static int IsValidDescriptor(int dd) {
  return (dd >= 0) &&
      (dd < (int)(sizeof(nacl_files) / sizeof(nacl_files[0]))) &&
      (-1 != nacl_files[dd].real_fd);
}

// Create a new entry representing the pathname andits corresponding file
// descriptor.  This entry gets added to an internal linked list, and the
// file handling thread gets notified.  It is assumed that file notifications
// from things like NPN_GetURL() come in on a different thread than the one
// where the file contents are read.  Returns 0 on success.
int NaClFile(const char *pathname, int fd, int size, int total) {
  struct nacl_file_map *entry;
  static int num_loaded = 0;

  entry = malloc(sizeof(*entry));

  if (NULL == entry) {
    fprintf(stderr, "No memory for file map for %s\n", pathname);
    return -1;
  }
  if (NULL == (entry->filename = strdup(pathname))) {
    fprintf(stderr, "No memory for file path %s\n", pathname);
    return -1;
  }

  entry->fd = fd;

  pthread_mutex_lock(&nacl_file_mu);
  num_loaded++;
  entry->next = nacl_file_map_list;
  entry->size = size;
  nacl_file_map_list = entry;
  nacl_file_done = (num_loaded == total);
  pthread_cond_broadcast(&nacl_file_cv);
  pthread_mutex_unlock(&nacl_file_mu);
  return 0;
}

int __wrap_open(char const *pathname, int mode, int perms) {
  int found = 0;
  int d = -1;
  struct nacl_file_map *entry;
  unsigned int i;
  int dd = -1;
  uint32_t size = 0;

  if (-1 == nacl_file_embedded) {
    nacl_file_embedded = srpc_get_fd() != -1;
  }
  if (!nacl_file_embedded) {
    return __real_open(pathname, mode, perms);
  }

  if (mode != O_RDONLY) {
    printf("nacl_file: invalid mode %d\n", mode);
    return -1;
  }

  pthread_mutex_lock(&nacl_file_mu);
  while (!found) {
    for (entry = nacl_file_map_list; DBG_NULL != entry; entry = entry->next) {
      // TODO(nfulagar): Not a great solution to locate the file.
      // The entries are full http://localhost:5103/... paths
      // The pathname argument to __wrap_open is likely a subset.
      // At the moment, not sure how to push the subset path all the way though
      // to NPN_StreamAsFile()
      char *f;
      f = strstr(entry->filename, pathname);
      if (NULL != f) {
        found = 1;
        d = entry->fd;
        size = entry->size;
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
    printf("nacl_file: not found %s\n", pathname);
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
      struct stat buffer;
      nacl_files[i].real_fd = d;
      nacl_files[i].pos = 0;
      if (0 == fstat(d, &buffer)) {
        if (S_IFSHM == (buffer.st_mode & S_IFMT)) {
          nacl_files[i].mmap = 1;
          nacl_files[i].mmap_size = (size_t)size;
          // printf("nacl_file: memory map of file size 0x%0X\n", (int)size);
        } else {
          nacl_files[i].mmap = 0;
          nacl_files[i].mmap_size = 0;
          // printf("nacl_file: normal file\n");
        }
      } else {
        nacl_files[i].mmap = 0;
        nacl_files[i].mmap_size = 0;
      }
      dd = i;
      break;
    }
  }
  pthread_mutex_unlock(&nacl_fake_mu);

  return dd;
}


int __wrap_close(int dd) {
  if (-1 == nacl_file_embedded) {
    nacl_file_embedded = srpc_get_fd() != -1;
  }
  if (!nacl_file_embedded) {
    return __real_close(dd);
  }
  if (!IsValidDescriptor(dd)) {
    errno = EBADF;
    return -1;
  }
  pthread_mutex_lock(&nacl_file_mu);
  nacl_files[dd].real_fd = -1;
  pthread_mutex_unlock(&nacl_file_mu);

  return 0;
}


int __wrap_read(int dd, void *buf, size_t count) {
  int got = 0;

  if (-1 == nacl_file_embedded) {
    nacl_file_embedded = srpc_get_fd() != -1;
  }
  if (!nacl_file_embedded) {
    return __real_read(dd, buf, count);
  }

  if (!IsValidDescriptor(dd)) {
    errno = EBADF;
    return -1;
  }
  pthread_mutex_lock(&nacl_files[dd].mu);
  if (nacl_files[dd].mmap) {
    /* use mmap to read data */
    uint8_t *data;
    off_t base_pos;
    off_t adj;
    size_t count_up;
    base_pos = nacl_files[dd].pos & (~0xFFFF);
    adj = nacl_files[dd].pos - base_pos;
    /* make sure we don't read beyond end of file */
    if ((nacl_files[dd].pos + count) > nacl_files[dd].mmap_size) {
      count = nacl_files[dd].mmap_size - nacl_files[dd].pos;
      if (count < 0)
        count = 0;
      printf("nacl_file: warning, attempting read outside of file!\n");
    }
    /* round count value to next 64KB */
    count_up = count + adj;
    count_up += 0xFFFF;
    count_up = count_up & (~0xFFFF);
    data = (uint8_t *)mmap(NULL, count_up, PROT_READ, MAP_SHARED,
                           nacl_files[dd].real_fd, base_pos);
    if (NULL != data) {
      memcpy(buf, data + adj, count);
      munmap(data, count_up);
      got = count;
    } else {
      printf("nacl_file: mmap call failed!\n");
    }
  } else {
    (void) __real_lseek(nacl_files[dd].real_fd, nacl_files[dd].pos, SEEK_SET);
    got = __real_read(nacl_files[dd].real_fd, buf, count);
  }
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
  if (!IsValidDescriptor(dd)) {
    errno = EBADF;
    return -1;
  }
  pthread_mutex_lock(&nacl_files[dd].mu);
  if (0 != nacl_files[dd].mmap) {
    switch (whence) {
      case SEEK_SET:
        break;
      case SEEK_CUR:
        offset = nacl_files[dd].pos + offset;
        break;
      case SEEK_END:
        offset = nacl_files[dd].mmap_size + offset;
        break;
    }
    if (offset < 0) {
      offset = -1;
    }
  } else {
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
  }
  if (-1 != offset) {
    nacl_files[dd].pos = offset;
  }
  pthread_mutex_unlock(&nacl_files[dd].mu);
  return offset;
}
