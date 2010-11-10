/* Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.

 * This file provides wrappers to lseek(2), read(2), etc. that read bytes from
 * an mmap()'ed buffer.  There are three steps required:
 *    1. Use linker aliasing to wrap lseek(), etc.  This is done in the
 *       Makefile using the "-XLinker --wrap -Xlinker lseek" arguments to
 *       nacl-gcc.  Note that this makes *all* calls to things like read() go
 *       through these wrappers, so if you also need to read() from, say, a
 *       socket, this code will not work as-is.
 *    2. Use lseek(), read() etc as you normally would for a file.
 *
 * Note: This code is very temporary and will disappear when the Pepper 2 API
 * is available in Native Client.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/nacl_syscalls.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <nacl/nacl_srpc.h>

#define MAX_NACL_FILES 256
#define MMAP_PAGE_SIZE 64 * 1024

extern "C" int __real_close(int dd);
extern "C" int __wrap_close(int dd);
extern "C" int __real_read(int dd, void *, size_t);
extern "C" int __wrap_read(int dd, void *, size_t);
extern "C" int __real_write(int dd, const void *, size_t);
extern "C" int __wrap_write(int dd, const void *, size_t);
extern "C" off_t __real_lseek(int dd, off_t offset, int whence);
extern "C" off_t __wrap_lseek(int dd, off_t offset, int whence);

static int nacl_file_initialized = 0;
static int used_nacl_fds[MAX_NACL_FILES];
static pthread_mutex_t nacl_file_mu = PTHREAD_MUTEX_INITIALIZER;

struct NaCl_file {
  int mode;
  int real_fd;
  off_t pos;
  pthread_mutex_t mu;
  size_t size;
};

static struct NaCl_file nacl_files[MAX_NACL_FILES];

/* Check to see the |dd| is a valid NaCl file descriptor */
static int IsValidDescriptor(int dd) {
  return nacl_file_initialized && (dd >= 0) && (dd < MAX_NACL_FILES);
}

/* Create a new entry representing the file descriptor.
   Returns a non-negative nacl_file descriptor on success. */
int NaClFile_fd(int fd, int mode) {
  int dd = -1;
  int i;
  struct stat stb;

  if (0 != fstat(fd, &stb)) {
    errno = EBADF;
    return -1;
  }

  if (S_IFSHM != (stb.st_mode & S_IFMT)) {
    printf("nacl_file: normal file?!\n");
    return -1;
  }

  pthread_mutex_lock(&nacl_file_mu);

  if (!nacl_file_initialized) {
    for (i = 0; i < MAX_NACL_FILES; ++i) {
      used_nacl_fds[i] = -1;
      nacl_files[i].real_fd = -1;
      pthread_mutex_init(&nacl_files[i].mu, NULL);
    }
    nacl_file_initialized = 1;
  }

  for (i = 3; i < MAX_NACL_FILES; ++i) {
    if (dd == used_nacl_fds[i]) {
      used_nacl_fds[i] = i;
      dd = i;
      break;
    }
  }

  if (-1 == dd) {
    fprintf(stderr, "nacl_file: Max open file count has been reached\n");
    return -1;
  }

  pthread_mutex_lock(&nacl_files[dd].mu);

  nacl_files[dd].real_fd = fd;
  nacl_files[dd].pos = 0;
  nacl_files[dd].mode = mode;
  nacl_files[dd].size = stb.st_size;

  pthread_mutex_unlock(&nacl_files[dd].mu);

  pthread_mutex_unlock(&nacl_file_mu);

  return dd;
}

/* Create a new file and return the fd for it.
   Returns non-negative nacl_file descriptor on success. */
int NaClFile_new(int mode) {
  int fd = imc_mem_obj_create(MMAP_PAGE_SIZE);
  if (fd < 0) {
    printf("nacl_file: imc_mem_obj_create failed %d\n", fd);
    return -1;
  }
  return NaClFile_fd(fd, mode);
}

/* Create a new file for the specified size and return the fd for it.
   Returns non-negative fake nacl_file descriptor on success. */
int NaClFile_new_of_size(int mode, size_t size) {
  int fd;
  size_t count_up;

  if (size < 0) {
    printf("nacl_file: file size can not be negative\n");
    return -1;
  }

  count_up = size + 0xFFFF;
  count_up = count_up & (~0xFFFF);

  fd = imc_mem_obj_create(count_up);
  if (fd < 0) {
    printf("nacl_file: imc_mem_obj_create failed %d\n", fd);
    return -1;
  }

  return NaClFile_fd(fd, mode);
}

/* Adjust the size of a nacl file.
   Changes the real_fd of a file.
   Returns 0 on success. */
int adjust_file_size(int dd, size_t new_size) {
  int new_fd = -1;
  off_t base_pos;
  size_t count;
  size_t final_base;
  uint8_t *new_data;
  uint8_t *old_data;

  if (!IsValidDescriptor(dd)) {
    errno = EBADF;
    return -1;
  }

  /* TODO(abetul): check if caller has already acquired the mutex for file */

  if (-1 == nacl_files[dd].real_fd) {
    errno = EBADF;
    return -1;
  }

  new_fd = imc_mem_obj_create(new_size);
  if (new_fd < 0) {
    printf("nacl_file: imc_mem_obj_create failed %d\n", new_fd);
    return -1;
  }

  /* copy contents over */
  final_base = nacl_files[dd].size & (~0xFFFF);
  for (base_pos = 0; (size_t) base_pos < final_base;
       base_pos += MMAP_PAGE_SIZE) {
    old_data = (uint8_t *) mmap(NULL, MMAP_PAGE_SIZE, PROT_READ, MAP_SHARED,
                               nacl_files[dd].real_fd, base_pos);
    new_data = (uint8_t *) mmap(NULL, MMAP_PAGE_SIZE, PROT_WRITE, MAP_SHARED,
                               new_fd, base_pos);
    if (NULL != old_data && NULL != new_data) {
      memcpy(new_data, old_data, MMAP_PAGE_SIZE);
      munmap(old_data, MMAP_PAGE_SIZE);
      munmap(new_data, MMAP_PAGE_SIZE);
    } else {
      printf("nacl_file: mmap call failed!\n");
      return -1;
    }
  }

  count = nacl_files[dd].size - final_base;

  if (count > 0) {
    old_data = (uint8_t *) mmap(NULL, MMAP_PAGE_SIZE, PROT_READ, MAP_SHARED,
                               nacl_files[dd].real_fd, base_pos);
    new_data = (uint8_t *) mmap(NULL, MMAP_PAGE_SIZE, PROT_WRITE, MAP_SHARED,
                               new_fd, base_pos);
    if (NULL != old_data && NULL != new_data) {
      memcpy(new_data, old_data, count);
      munmap(old_data, MMAP_PAGE_SIZE);
      munmap(new_data, MMAP_PAGE_SIZE);
    } else {
      printf("nacl_file: mmap call failed!\n");
      return -1;
    }
  }

  if (__real_close(nacl_files[dd].real_fd) < 0) {
    printf("nacl_file: close in size adjust failed!\n");
    return -1;
  }

  nacl_files[dd].real_fd = new_fd;
  nacl_files[dd].size = new_size;

  return 0;
}

int get_real_fd(int dd) {
  int fd = -1;

  if (!IsValidDescriptor(dd)) {
    errno = EBADF;
    return -1;
  }
  pthread_mutex_lock(&nacl_files[dd].mu);
  fd = nacl_files[dd].real_fd;
  pthread_mutex_unlock(&nacl_files[dd].mu);

  return fd;
}

int __wrap_close(int dd) {
  if (!IsValidDescriptor(dd)) {
    return __real_close(dd);
  }

  pthread_mutex_lock(&nacl_file_mu);

  if (-1 == used_nacl_fds[dd]) {
    pthread_mutex_unlock(&nacl_file_mu);
    return __real_close(dd);
  }

  pthread_mutex_lock(&nacl_files[dd].mu);

  nacl_files[dd].real_fd = -1;
  used_nacl_fds[dd] = -1;

  pthread_mutex_unlock(&nacl_files[dd].mu);
  pthread_mutex_unlock(&nacl_file_mu);

  return 0;
}

int __wrap_read(int dd, void *buf, size_t count) {
  int got = 0;
  uint8_t *data;
  off_t base_pos;
  off_t adj;
  size_t count_up;

  if (!IsValidDescriptor(dd)) {
    return __real_read(dd, buf, count);
  }

  pthread_mutex_lock(&nacl_files[dd].mu);

  if (-1 == nacl_files[dd].real_fd) {
    pthread_mutex_unlock(&nacl_files[dd].mu);
    return __real_read(dd, buf, count);
  }

  if ((nacl_files[dd].mode & O_RDONLY) != O_RDONLY) {
    printf("nacl_file: invalid mode %d\n", nacl_files[dd].mode);
    pthread_mutex_unlock(&nacl_files[dd].mu);
    return -1;
  }

  /* make sure we don't read beyond end of file */
  if ((nacl_files[dd].pos + count) > nacl_files[dd].size) {
    if ((nacl_files[dd].size - nacl_files[dd].pos) < 0)
      count = 0;
    else
      count = nacl_files[dd].size - nacl_files[dd].pos;
    printf("nacl_file: warning, attempting read outside of file!\n");
  }

  /* use mmap to read data */
  base_pos = nacl_files[dd].pos & (~0xFFFF);
  adj = nacl_files[dd].pos - base_pos;
  /* round count value to next 64KB */
  count_up = count + adj;
  count_up += 0xFFFF;
  count_up = count_up & (~0xFFFF);
  data = (uint8_t *) mmap(NULL, count_up, PROT_READ, MAP_SHARED,
                         nacl_files[dd].real_fd, base_pos);
  if (NULL != data) {
    memcpy(buf, data + adj, count);
    munmap(data, count_up);
    got = count;
  } else {
    printf("nacl_file: mmap call failed!\n");
  }

  if (got > 0) {
    nacl_files[dd].pos += got;
  }
  pthread_mutex_unlock(&nacl_files[dd].mu);
  return got;
}

int __wrap_write(int dd, const void *buf, size_t count) {
  int got = 0;
  uint8_t *data;
  off_t base_pos;
  off_t adj;
  size_t count_up;
  size_t new_size;

  if (!IsValidDescriptor(dd)) {
    return __real_write(dd, buf, count);
  }

  pthread_mutex_lock(&nacl_files[dd].mu);

  if (-1 == nacl_files[dd].real_fd) {
    pthread_mutex_unlock(&nacl_files[dd].mu);
    return __real_write(dd, buf, count);
  }

  if ((nacl_files[dd].mode & O_WRONLY) != O_WRONLY) {
    printf("nacl_file: invalid mode %d\n", nacl_files[dd].mode);
    pthread_mutex_unlock(&nacl_files[dd].mu);
    return -1;
  }

  /* adjust file size if writing past the current end */
  new_size = nacl_files[dd].size;
  while ((nacl_files[dd].pos + count) > new_size) {
    /* double the file size */
    new_size <<= 1;
  }

  if (new_size > nacl_files[dd].size) {
    if (adjust_file_size(dd, new_size) != 0) {
      pthread_mutex_unlock(&nacl_files[dd].mu);
      printf("nacl_file: failed to adjust file size %d\n", dd);
      return -1;
    }
  }

  /* use mmap to write data */
  base_pos = nacl_files[dd].pos & (~0xFFFF);
  adj = nacl_files[dd].pos - base_pos;
  /* round count value to next 64KB */
  count_up = count + adj;
  count_up += 0xFFFF;
  count_up = count_up & (~0xFFFF);
  data = (uint8_t *) mmap(NULL, count_up, PROT_WRITE, MAP_SHARED,
                         nacl_files[dd].real_fd, base_pos);
  if (NULL != data) {
    memcpy(data + adj, buf, count);
    munmap(data, count_up);
    got = count;
  } else {
    printf("nacl_file: mmap call failed!\n");
  }

  if (got > 0) {
    nacl_files[dd].pos += got;
  }
  pthread_mutex_unlock(&nacl_files[dd].mu);
  return got;
}

off_t __wrap_lseek(int dd, off_t offset, int whence) {
  if (!IsValidDescriptor(dd)) {
    return __real_lseek(dd, offset, whence);
  }
  pthread_mutex_lock(&nacl_files[dd].mu);

  if (-1 == nacl_files[dd].real_fd) {
    pthread_mutex_unlock(&nacl_files[dd].mu);
    return __real_lseek(dd, offset, whence);
  }

  switch (whence) {
    case SEEK_SET:
      break;
    case SEEK_CUR:
      offset = nacl_files[dd].pos + offset;
      break;
    case SEEK_END:
      offset = nacl_files[dd].size + offset;
      break;
  }
  if (offset < 0) {
    offset = -1;
  }
  if (-1 != offset) {
    nacl_files[dd].pos = offset;
  }
  pthread_mutex_unlock(&nacl_files[dd].mu);
  return offset;
}

