#include <stdio.h>
#include <stdlib.h>

#include "native_client/src/include/portability.h"

#include "native_client/src/shared/platform/nacl_check.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_effector_trusted_mem.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/gio_shm.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

#define NCHUNKS   3

#define MAX_CHECK (4096)

#if defined(HAVE_SDL)
#include <SDL.h>
#endif

int           gVerbose = 0;
unsigned int  gRandomSeed = 0x31415926;
size_t        gNumSamples = 2000;

uint32_t patgen(uint32_t offset) {
  return (((offset + 3) << (3 * 8)) ^
          ((offset + 2) << (2 * 8)) ^
          ((offset + 1) << (1 * 8)) ^
          ((offset + 0) << (0 * 8)));
}

size_t MemFiller(uint32_t *mem, size_t offset) {
  *mem = patgen(offset);
  return 0;
}

size_t MemChecker(uint32_t *mem, size_t offset) {
  uint32_t pat = patgen(offset);
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
  r = rand() % self->nbytes;
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

size_t CheckGioWithProber(struct Prober *pp,
                          struct Gio    *gp,
                          uint8_t       *addr,
                          size_t        nbytes) {
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
    for (valix = 0; valix < sizeof val/sizeof val[0]; ++valix) {
      val[valix] = ~valix;
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
      printf("Reading 0x%"NACL_PRIxS" bytes\n", checkbytes);
    }

    if (-1 == (*gp->vtbl->Read)(gp, val, checkbytes)) {
      if (gVerbose) {
        printf("Read at %"NACL_PRIdS" failed\n", ix);
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

size_t CheckGio(struct Gio  *gp,
                uint8_t     *addr,
                size_t      nbytes) {
  struct LinearProber lp;
  struct ReverseProber rp;
  struct RandomProber rand_probe;
  ssize_t num_err = 0;

  LinearProberCtor(&lp, nbytes);
  printf("Testing w/ LinearProber\n");
  num_err += CheckGioWithProber((struct Prober *) &lp, gp, addr, nbytes);

  ReverseProberCtor(&rp, nbytes);
  printf("Testing w/ ReverseProber\n");
  num_err += CheckGioWithProber((struct Prober *) &rp, gp, addr, nbytes);

  RandomProberCtor(&rand_probe, nbytes, gNumSamples);
  printf("Testing w/ RandomProber\n");
  num_err += CheckGioWithProber((struct Prober *) &rand_probe,
                                gp,
                                addr,
                                nbytes);

  return num_err;
}

int main(int ac,
         char **av) {
  int                   opt;

  struct NaClDescImcShm *shmp;
  struct NaClDescEffectorTrustedMem eff;
  struct NaClDesc *dp;
  struct NaClDescEffector *effp;
  uintptr_t addr;
  uintptr_t addr2;
  size_t nbytes;
  size_t errs;
  int rv;
  struct NaClGioShm gio_shm;

  while (EOF != (opt = getopt(ac, av, "n:s:v"))) {
    switch (opt) {
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
        fprintf(stderr, "Usage: gio_shm_test [-v]\n");
        return EXIT_FAILURE;
    }
  }

  printf("Using seed %d (0x%x)\n", gRandomSeed, gRandomSeed);
  srand(gRandomSeed);

  NaClAllModulesInit();

  shmp = malloc(sizeof *shmp);
  if (NULL == shmp) {
    printf("No memory\n");
    printf("FAILED\n");
    return EXIT_FAILURE;
  }

  nbytes = NCHUNKS * NACL_MAP_PAGESIZE;
  if (!NaClDescImcShmAllocCtor(shmp, nbytes)) {
    printf("NaClDescImcShmAllocCtor failed\n");
    printf("FAILED\n");
    return EXIT_FAILURE;
  }
  if (!NaClDescEffectorTrustedMemCtor(&eff)) {
    printf("NaClDescEffectorTrustedMemCtor failed\n");
    printf("FAILED\n");
    return EXIT_FAILURE;
  }
  dp = &shmp->base;
  effp = &eff.base;

  addr = (*dp->vtbl->Map)(dp,
                          effp,
                          NULL,
                          nbytes,
                          NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                          NACL_ABI_MAP_SHARED,
                          0);
  if (NaClIsNegErrno(addr)) {
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

  addr2 = (*dp->vtbl->Map)(dp,
                           effp,
                           NULL,
                           nbytes,
                           NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                           NACL_ABI_MAP_SHARED,
                           0);
  if (NaClIsNegErrno(addr2)) {
    printf("Second Map failed\n");
    return EXIT_FAILURE;
  }
  printf("Checking second view consistency\n");
  if (0 != MemWalk(MemChecker, (uint32_t *) addr2, nbytes / sizeof(uint32_t))) {
    printf("Second view consistency check failed\n");
    printf("FAILED\n");
    return EXIT_FAILURE;
  }
  if (0 != (rv = (*dp->vtbl->UnmapUnsafe)(dp, effp, (void *) addr2, nbytes))) {
    printf("UnmapUnsafe failed, returned %d\n", rv);
    printf("FAILED\n");
    return EXIT_FAILURE;
  }

  if (!NaClGioShmCtor(&gio_shm, dp, nbytes)) {
    printf("NaClGioShmCtor failed\n");
    printf("FAILED\n");
    return EXIT_FAILURE;
  }

  printf("Checking Gio vs direct shm consistency\n");
  if (0 != (errs = CheckGio((struct Gio *) &gio_shm,
                            (uint8_t *) addr,
                            nbytes))) {
    printf("CheckGio failed, found %"NACL_PRIdS" errors\n", errs);
    printf("FAILED\n");
    return EXIT_FAILURE;
  }

  (*gio_shm.base.vtbl->Dtor)((struct Gio *) &gio_shm);

  NaClDescUnref(dp);
  (*effp->vtbl->Dtor)(effp);

  NaClAllModulesFini();
  printf("PASSED\n");
  return EXIT_SUCCESS;
}
