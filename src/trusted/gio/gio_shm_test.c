/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>

#include "native_client/src/include/portability.h"

#include "native_client/src/shared/platform/nacl_check.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_effector_trusted_mem.h"

#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/gio/gio_shm.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

#define NCHUNKS   3

#define MAX_CHECK (4096)

int           gVerbose = 0;
unsigned int  gRandomSeed = 0x31415926;
size_t        gNumSamples = 2048;

uint32_t patgen(uint32_t offset) {
  return (((offset + 3) << (3 * 8)) ^
          ((offset + 2) << (2 * 8)) ^
          ((offset + 1) << (1 * 8)) ^
          ((offset + 0) << (0 * 8)));
}


size_t ZeroFiller(uint32_t *mem, size_t offset) {
  UNREFERENCED_PARAMETER(offset);
  *mem = 0;
  return 0;
}

size_t ZeroChecker(uint32_t *mem, size_t offset) {
  UNREFERENCED_PARAMETER(offset);
  return *mem != 0;
}

size_t MemFiller(uint32_t *mem, size_t offset) {
  *mem = patgen((uint32_t) offset);
  return 0;
}

size_t MemChecker(uint32_t *mem, size_t offset) {
  uint32_t pat = patgen((uint32_t) offset);
  return *mem != pat;
}

size_t MemWalk(size_t (*visitor)(uint32_t *, uintptr_t),
               uint32_t *buf,
               size_t nwords) {
  size_t  ix = 0;
  size_t  err_count = 0;

  for (ix = 0; ix < nwords; ++ix) {
    err_count += (*visitor)(&buf[ix], ix);
  }
  return err_count;
}

struct Prober {
  /* inline vtbl */
  ssize_t  (*NextIx)(struct Prober  *self, size_t *nbytes);
};

struct LinearProber {
  struct Prober base;
  size_t        cur_ix;
  size_t        nbytes;
};

ssize_t LinearProberIndex(struct Prober *vself, size_t *checkbytes) {
  struct LinearProber *self = (struct LinearProber *) vself;
  if (self->cur_ix == self->nbytes) return -1;
  *checkbytes = 1;
  return self->cur_ix++;
}

void LinearProberCtor(struct LinearProber *self, size_t nbytes) {
  self->base.NextIx = LinearProberIndex;
  self->cur_ix = 0;
  self->nbytes = nbytes;
}

struct ReverseProber {
  struct Prober base;
  size_t        nbytes;
};

ssize_t ReverseProberIndex(struct Prober *vself, size_t *checkbytes) {
  struct ReverseProber *self = (struct ReverseProber *) vself;

  *checkbytes = 1;
  return --self->nbytes;
}

void ReverseProberCtor(struct ReverseProber *self, size_t nbytes) {
  self->base.NextIx = ReverseProberIndex;
  self->nbytes = nbytes;
}

struct RandomProber {
  struct Prober base;
  size_t        nbytes;
  /* windows does not have rand_r, so this test must be single threaded */
  size_t        count;
};

ssize_t RandomProberIndex(struct Prober *vself, size_t *checkbytes) {
  struct RandomProber *self = (struct RandomProber *) vself;
  int r;
  size_t remain;

  if (0 == self->count) {
    return -1;
  }
  --self->count;
  r = (int) ((size_t) rand() % self->nbytes);
  CHECK(r >= 0);
  remain = self->nbytes - r;
  if (remain > MAX_CHECK) {
    remain = MAX_CHECK;
  }
  *checkbytes = rand() % remain;
  CHECK(*checkbytes <= remain);
  return r;
}

void RandomProberCtor(struct RandomProber *self,
                      size_t nbytes,
                      size_t nsamples) {
  self->base.NextIx = RandomProberIndex;
  self->nbytes = nbytes;
  self->count = nsamples;
}

/*
 * Op is sort of a funny virtual function pointer -- it's not an
 * offset in the vtbl, so changing the object is bad (esp w/
 * subclasses that might have overridden the virtual function), but
 * the extracted function pointer.
 */
size_t CheckGioOpWithProber(struct Prober *pp,
                            struct Gio    *gp,
                            uint8_t       *addr,
                            size_t        nbytes,
                            ssize_t       (*Op)(struct Gio *, void *, size_t)) {
  size_t    errs = 0;
  ssize_t   ix;
  size_t    checkbytes;
  uint8_t   val[MAX_CHECK];
  size_t    valix;

  while (-1 != (ix = (*pp->NextIx)(pp, &checkbytes))) {
    if (nbytes <= (size_t) ix) {
      break;
    }
    if (gVerbose > 1) {
      printf("0x%"NACL_PRIxS", 0x%"NACL_PRIxS"\n", ix, checkbytes);
    }
    if (checkbytes > sizeof val) {
      checkbytes = sizeof val;
    }

    /* fill with something we don't expect to be in real data */
    for (valix = 0; valix < checkbytes; ++valix) {
      val[valix] = (uint8_t) (~valix);
    }

    if (gVerbose > 2) {
      printf("Seeking to 0x%"NACL_PRIxS"\n", ix);
    }

    if (-1 == (*gp->vtbl->Seek)(gp, (off_t) ix, SEEK_SET)) {
      if (gVerbose) {
        printf("Seek to %"NACL_PRIdS" failed\n", ix);
      }
      ++errs;
      continue;
    }
    if (gVerbose > 2) {
      printf("Operating on 0x%"NACL_PRIxS" bytes\n", checkbytes);
    }

    if (-1 == (*Op)(gp, val, checkbytes)) {
      if (gVerbose) {
        printf("Apply Op at %"NACL_PRIdS" failed\n", ix);
      }
      ++errs;
      continue;
    }

    if (gVerbose > 2) {
      printf("Comparing against plain mmap view\n");
    }
    for (valix = 0; valix < checkbytes; ++valix) {
      if (1 < gVerbose) {
        printf(("Value from gio is 0x%08"NACL_PRIx8","
                " from memory is 0x%08"NACL_PRIx8"\n"),
               val[valix],
               addr[ix + valix]);
      }
      if (val[valix] != addr[ix + valix]) {
        ++errs;
      }
    }
  }
  return errs;
}

size_t CheckGioReadWithProber(struct Prober *pp,
                              struct Gio    *gp,
                              uint8_t       *addr,
                              size_t        nbytes) {
  return CheckGioOpWithProber(pp, gp, addr, nbytes, gp->vtbl->Read);
}

size_t CheckGioWriteWithProber(struct Prober *pp,
                               struct Gio    *gp,
                               uint8_t       *addr,
                               size_t        nbytes) {
  return CheckGioOpWithProber(pp, gp, addr, nbytes,
                              (ssize_t (*)(struct Gio *, void *, size_t))
                              gp->vtbl->Write);
}


size_t CheckGioOp(struct Gio  *gp,
                  uint8_t     *addr,
                  size_t      nbytes,
                  size_t      (*Op)(struct Prober *,
                                    struct Gio    *,
                                    uint8_t       *,
                                    size_t)) {
  struct LinearProber lp;
  struct ReverseProber rp;
  struct RandomProber rand_probe;
  ssize_t num_err = 0;

  LinearProberCtor(&lp, nbytes);
  printf("Testing w/ LinearProber\n");
  num_err += (*Op)((struct Prober *) &lp, gp, addr, nbytes);

  ReverseProberCtor(&rp, nbytes);
  printf("Testing w/ ReverseProber\n");
  num_err += (*Op)((struct Prober *) &rp, gp, addr, nbytes);

  RandomProberCtor(&rand_probe, nbytes, gNumSamples);
  printf("Testing w/ RandomProber\n");
  num_err += (*Op)((struct Prober *) &rand_probe, gp, addr, nbytes);

  return num_err;
}

size_t CheckGioRead(struct Gio  *gp,
                    uint8_t     *addr,
                    size_t      nbytes) {
  return CheckGioOp(gp, addr, nbytes, CheckGioReadWithProber);
}

size_t CheckGioWrite(struct Gio  *gp,
                     uint8_t     *addr,
                     size_t      nbytes) {
  return CheckGioOp(gp, addr, nbytes, CheckGioWriteWithProber);
}

size_t CheckGioZeros(struct Gio *gp,
                     size_t     nbytes) {
  unsigned char byte;
  ssize_t       rv;
  size_t        nerrors = 0;
  size_t        ix;
  uint64_t      temp_nbytes = nbytes;

  if (temp_nbytes > OFF_T_MAX) {
    return 1;
  }

  for (ix = 0; ix < nbytes; ++ix) {
    byte = 0xff;
    if (-1 == (*gp->vtbl->Seek)(gp, (off_t) ix, SEEK_SET)) {
      printf("Seek to byt %"NACL_PRIuS" failed\n", ix);
      ++nerrors;
      continue;
    }
    if (1 != (rv = (*gp->vtbl->Read)(gp, &byte, 1))) {
      printf("Read of byte %"NACL_PRIuS" failed\n", ix);
      ++nerrors;
      continue;
    }
    if (0 != byte) {
      printf("Byte %"NACL_PRIuS" not zero: 0x%02x\n", ix, 0xff & byte);
      ++nerrors;
    }
  }
  return nerrors;
}


int main(int ac,
         char **av) {
  int                   opt;

  struct NaClDescImcShm *shmp;
  struct NaClDesc *dp;
  uintptr_t addr;
  uintptr_t addr2;
  size_t nbytes;
  size_t errs;
  struct NaClGioShm gio_shm;
  size_t map_chunks = NCHUNKS;

  while (EOF != (opt = getopt(ac, av, "m:n:s:v"))) {
    switch (opt) {
      case 'm':
        map_chunks = strtoul(optarg, (char **) NULL, 0);
        break;
      case 'n':
        gNumSamples = strtoul(optarg, (char **) NULL, 0);
        break;
      case 's':
        gRandomSeed = strtoul(optarg, (char **) NULL, 0);
        break;
      case 'v':
        ++gVerbose;
        break;
      default:
        fprintf(stderr,
                ("Usage: gio_shm_test [-v] [-m map_chunks]"
                 " [-n num_samples] [-s seed]\n"));
        return EXIT_FAILURE;
    }
  }

  printf("Using seed %d (0x%x)\n", gRandomSeed, gRandomSeed);
  srand(gRandomSeed);

  NaClNrdAllModulesInit();

  shmp = malloc(sizeof *shmp);
  if (NULL == shmp) {
    printf("No memory\n");
    printf("FAILED\n");
    return EXIT_FAILURE;
  }

  nbytes = map_chunks * NACL_MAP_PAGESIZE;
  if (!NaClDescImcShmAllocCtor(shmp, nbytes, /* executable= */ 0)) {
    printf("NaClDescImcShmAllocCtor failed\n");
    printf("FAILED\n");
    return EXIT_FAILURE;
  }
  dp = &shmp->base;

  addr = (*((struct NaClDescVtbl const *) dp->base.vtbl)->
          Map)(dp,
               NaClDescEffectorTrustedMem(),
               NULL,
               nbytes,
               NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
               NACL_ABI_MAP_SHARED,
               0);
  if (NaClPtrIsNegErrno(&addr)) {
    printf("Map failed\n");
    return EXIT_FAILURE;
  }

  MemWalk(MemFiller, (uint32_t *) addr, nbytes / sizeof(uint32_t));
  printf("Checking basic consistency\n");
  if (0 != MemWalk(MemChecker, (uint32_t *) addr, nbytes / sizeof(uint32_t))) {
    printf("Initial consistency check failed\n");
    printf("FAILED\n");
    return EXIT_FAILURE;
  }

  addr2 = (*((struct NaClDescVtbl const *) dp->base.vtbl)->
           Map)(dp,
                NaClDescEffectorTrustedMem(),
                NULL,
                nbytes,
                NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                NACL_ABI_MAP_SHARED,
                0);
  if (NaClPtrIsNegErrno(&addr2)) {
    printf("Second Map failed\n");
    return EXIT_FAILURE;
  }
  printf("Checking second view consistency\n");
  if (0 != MemWalk(MemChecker, (uint32_t *) addr2, nbytes / sizeof(uint32_t))) {
    printf("Second view consistency check failed\n");
    printf("FAILED\n");
    return EXIT_FAILURE;
  }
  NaClDescUnmapUnsafe(dp, (void *) addr2, nbytes);

  if (!NaClGioShmCtor(&gio_shm, dp, nbytes)) {
    printf("NaClGioShmCtor failed\n");
    printf("FAILED\n");
    return EXIT_FAILURE;
  }

  printf("Checking Gio vs direct shm consistency, read\n");
  if (0 != (errs = CheckGioRead((struct Gio *) &gio_shm,
                                (uint8_t *) addr,
                                nbytes))) {
    printf("ERROR: CheckGioRead failed, found %"NACL_PRIdS" errors\n", errs);
  }

  printf("Zeroing shared memory\n");
  MemWalk(ZeroFiller, (uint32_t *) addr, nbytes / sizeof(uint32_t));
  printf("Reading for zeros\n");
  if (0 != (errs = CheckGioZeros((struct Gio *) &gio_shm,
                                 nbytes))) {
    printf("ERROR: Gio found non-zero bytes!\n");
  }

  printf("Checking Gio vs direct shm consistency, write\n");
  if (0 != (errs = CheckGioWrite((struct Gio *) &gio_shm,
                                 (uint8_t *) addr,
                                 nbytes))) {
    printf("ERROR: CheckGioWrite failed, found %"NACL_PRIdS" errors\n", errs);
  }

  (*gio_shm.base.vtbl->Dtor)((struct Gio *) &gio_shm);

  NaClDescUnref(dp);

  NaClNrdAllModulesFini();
  if (0 != errs) {
    printf("FAILED\n");
    return EXIT_FAILURE;
  } else {
    printf("PASSED\n");
    return EXIT_SUCCESS;
  }
}
