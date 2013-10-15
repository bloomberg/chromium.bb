/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/service_runtime/include/sys/unistd.h"
#include "native_client/src/untrusted/irt/irt.h"

/*
 * This is an implementation of NaCl's IRT interfaces that runs
 * outside of the NaCl sandbox.
 *
 * This allows PNaCl to be used as a portability layer without the
 * SFI-based sandboxing.  PNaCl pexes can be translated to
 * non-SFI-sandboxed native code and linked against this IRT
 * implementation.
 */


void _user_start(void *info);

static __thread void *g_tls_value;


static int irt_close(int fd) {
  if (close(fd) != 0)
    return errno;
  return 0;
}

static int irt_write(int fd, const void *buf, size_t count, size_t *nwrote) {
  int result = write(fd, buf, count);
  if (result < 0)
    return errno;
  *nwrote = result;
  return 0;
}

static int irt_fstat(int fd, struct stat *st) {
  /* TODO(mseaborn): Implement this and convert "struct stat". */
  return ENOSYS;
}

static void irt_exit(int status) {
  _exit(status);
}

static int irt_sysconf(int name, int *value) {
  switch (name) {
    case NACL_ABI__SC_PAGESIZE:
      /*
       * For now, return the host's page size (typically 4k) rather
       * than 64k (NaCl's usual page size), which pexes will usually
       * be tested with.  We could change this to 64k, but then the
       * mmap() we define here should round up requested sizes to
       * multiples of 64k.
       */
      *value = getpagesize();
      return 0;
    default:
      return EINVAL;
  }
}

static int irt_mmap(void **addr, size_t len, int prot, int flags,
                    int fd, off_t off) {
  void *result = mmap(*addr, len, prot, flags, fd, off);
  if (result == MAP_FAILED)
    return errno;
  *addr = result;
  return 0;
}

static int tls_init(void *ptr) {
  g_tls_value = ptr;
  return 0;
}

static void *tls_get(void) {
  return g_tls_value;
}

void *__nacl_read_tp(void) {
  return g_tls_value;
}

static void irt_stub_func(const char *name) {
  fprintf(stderr, "Error: Unimplemented IRT function: %s\n", name);
  abort();
}

#define DEFINE_STUB(name) \
    static void irt_stub_##name() { irt_stub_func(#name); }
#define USE_STUB(s, name) (typeof(s.name)) irt_stub_##name

DEFINE_STUB(gettod)
DEFINE_STUB(clock)
DEFINE_STUB(nanosleep)
DEFINE_STUB(sched_yield)
static const struct nacl_irt_basic irt_basic = {
  irt_exit,
  USE_STUB(irt_basic, gettod),
  USE_STUB(irt_basic, clock),
  USE_STUB(irt_basic, nanosleep),
  USE_STUB(irt_basic, sched_yield),
  irt_sysconf,
};

DEFINE_STUB(dup)
DEFINE_STUB(dup2)
DEFINE_STUB(read)
DEFINE_STUB(seek)
DEFINE_STUB(getdents)
static const struct nacl_irt_fdio irt_fdio = {
  irt_close,
  USE_STUB(irt_fdio, dup),
  USE_STUB(irt_fdio, dup2),
  USE_STUB(irt_fdio, read),
  irt_write,
  USE_STUB(irt_fdio, seek),
  irt_fstat,
  USE_STUB(irt_fdio, getdents),
};

DEFINE_STUB(munmap)
DEFINE_STUB(mprotect)
static const struct nacl_irt_memory irt_memory = {
  irt_mmap,
  USE_STUB(irt_memory, munmap),
  USE_STUB(irt_memory, mprotect),
};

static const struct nacl_irt_tls irt_tls = {
  tls_init,
  tls_get,
};

struct nacl_interface_table {
  const char *name;
  const void *table;
  size_t size;
};

static const struct nacl_interface_table irt_interfaces[] = {
  { NACL_IRT_BASIC_v0_1, &irt_basic, sizeof(irt_basic) },
  { NACL_IRT_FDIO_v0_1, &irt_fdio, sizeof(irt_fdio) },
  { NACL_IRT_MEMORY_v0_3, &irt_memory, sizeof(irt_memory) },
  { NACL_IRT_TLS_v0_1, &irt_tls, sizeof(irt_tls) },
};

static size_t irt_interface_query(const char *interface_ident,
                                  void *table, size_t tablesize) {
  unsigned i;
  for (i = 0; i < NACL_ARRAY_SIZE(irt_interfaces); ++i) {
    if (0 == strcmp(interface_ident, irt_interfaces[i].name)) {
      const size_t size = irt_interfaces[i].size;
      if (size <= tablesize) {
        memcpy(table, irt_interfaces[i].table, size);
        return size;
      }
      break;
    }
  }
  fprintf(stderr, "Warning: unavailable IRT interface queried: %s\n",
          interface_ident);
  return 0;
}

/* Layout for empty argv/env arrays. */
struct startup_info {
  void (*cleanup_func)();
  int envc;
  int argc;
  char *argv0;
  char *envp0;
  Elf32_auxv_t auxv[2];
};

int main(int argc, char **argv) {
  /* TODO(mseaborn): Copy across argv and environment arrays. */
  struct startup_info info;
  info.cleanup_func = NULL;
  info.envc = 0;
  info.argc = 0;
  info.argv0 = NULL;
  info.envp0 = NULL;
  info.auxv[0].a_type = AT_SYSINFO;
  info.auxv[0].a_un.a_val = (uintptr_t) irt_interface_query;
  info.auxv[1].a_type = 0;
  info.auxv[1].a_un.a_val = 0;

  _user_start(&info);
  return 1;
}
