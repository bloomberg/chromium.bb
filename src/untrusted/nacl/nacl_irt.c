/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include <unistd.h>

#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/include/elf32.h"
#include "native_client/src/untrusted/irt/irt_interfaces.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"

static int stub_open(const char *pathname, int oflag, mode_t cmode,
                     int *newfd) {
  return ENOSYS;
}

static int stub_stat(const char *pathname, struct stat *st) {
  return ENOSYS;
}

static int stub_close(int fd) {
  return ENOSYS;
}

static int stub_dup(int fd, int *newfd) {
  return ENOSYS;
}

static int stub_dup2(int fd, int newfd) {
  return ENOSYS;
}

static int stub_read(int fd, void *buf, size_t count, size_t *nread) {
  return ENOSYS;
}

static int stub_write(int fd, const void *buf, size_t count, size_t *nwrote) {
  return ENOSYS;
}

static int stub_seek(int fd, off_t offset, int whence, off_t *new_offset) {
  return ENOSYS;
}

static int stub_fstat(int fd, struct stat *st) {
  return ENOSYS;
}

static int stub_getdents(int fd, struct dirent *dirent, size_t count,
                         size_t *nread) {
  return ENOSYS;
}

struct nacl_irt_basic __libnacl_irt_basic;
struct nacl_irt_memory __libnacl_irt_memory;
struct nacl_irt_tls __libnacl_irt_tls;
struct nacl_irt_clock __libnacl_irt_clock;
struct nacl_irt_dev_getpid __libnacl_irt_dev_getpid;

struct nacl_irt_filename __libnacl_irt_filename = {
  stub_open,
  stub_stat,
};

struct nacl_irt_fdio __libnacl_irt_fdio = {
  stub_close,
  stub_dup,
  stub_dup2,
  stub_read,
  stub_write,
  stub_seek,
  stub_fstat,
  stub_getdents,
};

TYPE_nacl_irt_query __nacl_irt_query;

static int __libnacl_irt_mprotect(void *addr, size_t len, int prot) {
  return ENOSYS;
}

/*
 * Avoid a dependency on libc's strlen function.
 */
static size_t my_strlen(const char *s) {
  size_t len = 0;
  while (*s++) ++len;
  return len;
}

/*
 * TODO(robertm): make the helper below globally accessible.
 */
static void __libnacl_abort(void) {
  while (1) *(volatile int *) 0;   /* null pointer dereference. */
}

/*
 * TODO(robertm): make the helper below globally accessible.
 */
static void __libnacl_message(const char *message) {
   /*
    * Skip write if fdio is not available.
    */
  if (__libnacl_irt_fdio.write == NULL) {
    return;
  }
  size_t dummy_bytes_written;
  size_t len = my_strlen(message);
  __libnacl_irt_fdio.write(STDERR_FILENO, message, len, &dummy_bytes_written);
}

/*
 * TODO(robertm): make the helper below globally accessible.
 */
static void __libnacl_fatal(const char* message) {
  __libnacl_message(message);
  __libnacl_abort();

}

/*
 * Scan the auxv for AT_SYSINFO, which is the pointer to the IRT query function.
 * Stash that for later use.
 */
static void grok_auxv(const Elf32_auxv_t *auxv) {
  const Elf32_auxv_t *av;
  for (av = auxv; av->a_type != AT_NULL; ++av) {
    if (av->a_type == AT_SYSINFO) {
      __nacl_irt_query = (TYPE_nacl_irt_query) av->a_un.a_val;
    }
  }
}

int __libnacl_irt_query(const char *interface,
                        void *table, size_t table_size) {
  if (NULL == __nacl_irt_query) {
    __libnacl_fatal("No IRT interface query routine!\n");
  }
  if (__nacl_irt_query(interface, table, table_size) != table_size) {
    return 0;
  }
  return 1;
}

void __libnacl_mandatory_irt_query(const char *interface,
                                   void *table, size_t table_size) {
  if (!__libnacl_irt_query(interface, table, table_size)) {
    __libnacl_fatal("IRT interface query failed for essential interface\n");
  }
}

#define DO_QUERY(ident, name)                                   \
  __libnacl_mandatory_irt_query(ident, &__libnacl_irt_##name,   \
                                sizeof(__libnacl_irt_##name))

/*
 * Initialize all our IRT function tables using the query function.
 * The query function's address is passed via AT_SYSINFO in auxv.
 */
void __libnacl_irt_init(Elf32_auxv_t *auxv) {
  grok_auxv(auxv);

  /*
   * The "fdio" and "filename" interfaces don't do anything useful in
   * Chromium (with the exception that write() sometimes produces
   * useful debugging output for stdout/stderr), so don't abort if
   * they are not available.
   *
   * We query for "fdio" early on so that write() can produce a useful
   * debugging message if any other IRT queries fail.
   */
  __libnacl_irt_query(NACL_IRT_FDIO_v0_1, &__libnacl_irt_fdio,
                      sizeof(__libnacl_irt_fdio));
  __libnacl_irt_query(NACL_IRT_FILENAME_v0_1, &__libnacl_irt_filename,
                      sizeof(__libnacl_irt_filename));

  DO_QUERY(NACL_IRT_BASIC_v0_1, basic);

  if (!__libnacl_irt_query(NACL_IRT_MEMORY_v0_3,
                           &__libnacl_irt_memory,
                           sizeof(__libnacl_irt_memory))) {
    /* Fall back to trying the old version, before sysbrk() was removed. */
    struct nacl_irt_memory_v0_2 old_irt_memory;
    if (!__libnacl_irt_query(NACL_IRT_MEMORY_v0_2,
                             &old_irt_memory,
                             sizeof(old_irt_memory))) {
      /* Fall back to trying an older version, before mprotect() was added. */
      __libnacl_mandatory_irt_query(NACL_IRT_MEMORY_v0_1,
                                    &old_irt_memory,
                                    sizeof(struct nacl_irt_memory_v0_1));
      __libnacl_irt_memory.mprotect = __libnacl_irt_mprotect;
    }
    __libnacl_irt_memory.mmap = old_irt_memory.mmap;
    __libnacl_irt_memory.munmap = old_irt_memory.munmap;
  }

  DO_QUERY(NACL_IRT_TLS_v0_1, tls);
}
