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


#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/nacl_imc_api.h>
#include <sys/nacl_syscalls.h>

int verbosity = 0;

void failed(char const *msg) {
  printf("TEST FAILED: %s\n", msg);
}

void passed(char const *msg) {
  printf("TEST PASSED: %s\n", msg);
}

/*
 * tests return error counts for a quick sum.
 */

int imc_shm_mmap(size_t   region_size,
                 int      prot,
                 int      flags,
                 int      off,
                 int      expect_creation_failure,
                 int      check_contents,
                 int      map_view)
{
  int   d;
  void  *addr;

  if (verbosity > 0) {
    printf("imc_shm_mmap(0x%x, 0x%x, 0x%x, 0x%x, %d, %d, %d)\n",
           region_size, prot, flags, off,
           expect_creation_failure, check_contents, map_view);
  }
  if (verbosity > 1) {
    printf("basic imc mem obj creation\n");
  }
  d = imc_mem_obj_create(region_size);
  if (expect_creation_failure) {
    if (-1 != d) {
      fprintf(stderr,
              ("imc_shm_mmap: expected failure to create"
               " IMC shm object, but got %d\n"),
              d);
      (void) close(d);
      return 1;
    }
    return 0;
  }
  if (-1 == d) {
    fprintf(stderr,
            ("imc_shm_mmap: could not create an IMC shm object"
             " of size %d (0x%x)\n"),
            region_size, region_size);
    return 1;
  }
  if (verbosity > 1) {
    printf("basic mmap functionality\n");
  }
  addr = mmap((void *) 0, region_size, prot, flags, d, off);
  if (MAP_FAILED == addr) {
    fprintf(stderr, "imc_shm_mmap: mmap failed, errno %d\n", errno);
    return 1;
  }
  if (check_contents) {
    char  *mem = (char *) addr;
    int   i;
    int   diff_count = 0;

    if (verbosity > 1) {
      printf("initial zero content check\n");
    }

    for (i = 0; i < region_size; ++i) {
      if (0 != mem[i]) {
        if (i >= verbosity) {
          printf("imc_shm_mmap: check_contents, byte %d not zero\n", i);
        }
        ++diff_count;
      }
    }
    if (0 != diff_count) {
      munmap(addr, region_size);
      (void) close(d);
      return diff_count;
    }
  }
  if (map_view && 0 != (prot & PROT_WRITE)) {
    void  *addr2;
    int   i;
    int   diff_count;
    char  *mem1;
    char  *mem2;

    if (verbosity > 1) {
      printf("coherent mapping test\n");
    }

    addr2 = mmap((void *) 0, region_size, prot, flags, d, off);
    if (MAP_FAILED == addr2) {
      fprintf(stderr,
              "imc_shm_mmap: 2nd map view mapping failed, errno %d\n", errno);
      return 1;
    }
    for (diff_count = 0, mem1 = (char *) addr, mem2 = (char *) addr2, i = 0;
         i < region_size; ++i) {
      mem1[i] = i;
      if ((0xff & i) != (0xff & mem2[i])) {
        if (i >= verbosity) {
          printf(("imc_shm_mmap: map_view: write to byte %d not coherent"
                  " wrote 0x%02x, got 0x%02x\n"), i, i, mem2[i]);
        }
        ++diff_count;
      }
    }
    if (0 != diff_count) {
      fprintf(stderr, "imc_shm_mmap: map_view: incoherent mapping!\n");
      return diff_count;
    }
  }

  if (0 != close(d)) {
    fprintf(stderr, "imc_shm_mmap: close on IMC shm desc failed\n");
    return 1;
  }
  return 0;
}

int main(int ac, char **av) {
  int opt;
  int fail_count = 0;

  while (EOF != (opt = getopt(ac, av, "v"))) {
    switch (opt) {
      case 'v':
        ++verbosity;
        break;
      default:
        fprintf(stderr, "Usage: sel_ldr -- imc_shm_mmap_test.nexe [-v]\n");
        return 1;
    }
  }

  fail_count += imc_shm_mmap(65536, PROT_READ|PROT_WRITE, MAP_SHARED,
                             0, 0, 0, 0);
  fail_count += imc_shm_mmap(65536, PROT_READ|PROT_WRITE, MAP_SHARED,
                             0, 0, 1, 0);
  fail_count += imc_shm_mmap(4096, PROT_READ|PROT_WRITE, MAP_SHARED,
                             0, 1, 1, 0);
  fail_count += imc_shm_mmap(1, PROT_READ|PROT_WRITE, MAP_SHARED,
                             0, 1, 1, 0);
  fail_count += imc_shm_mmap(0x20000, PROT_READ|PROT_WRITE, MAP_SHARED,
                             0, 0, 1, 0);
  fail_count += imc_shm_mmap(0x30000, PROT_READ, MAP_SHARED,
                             0, 0, 1, 0);
  fail_count += imc_shm_mmap(0x40000, PROT_READ, MAP_PRIVATE,
                             0, 0, 1, 0);

  fail_count += imc_shm_mmap(65536, PROT_READ|PROT_WRITE, MAP_SHARED,
                             0, 0, 1, 1);
  fail_count += imc_shm_mmap(0x20000, PROT_READ|PROT_WRITE, MAP_SHARED,
                             0, 0, 1, 1);
  fail_count += imc_shm_mmap(0x30000, PROT_READ, MAP_SHARED,
                             0, 0, 1, 1);
  fail_count += imc_shm_mmap(0x40000, PROT_READ, MAP_PRIVATE,
                             0, 0, 1, 1);
  fail_count += imc_shm_mmap(0x100000, PROT_READ|PROT_WRITE, MAP_PRIVATE,
                             0, 0, 1, 1);
  fail_count += imc_shm_mmap(0x100000, PROT_READ|PROT_WRITE, MAP_SHARED,
                             0, 0, 1, 1);

  printf("imc_shm_mmap: %d failures\n", fail_count);
  if (0 == fail_count) passed("imc_shm_mmap: all sizes\n");
  else failed("imc_shm_mmap: some test(s) failed\n");

  return fail_count;
}
