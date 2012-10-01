/*
 * Copyright (c) 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>

#include "native_client/src/include/portability.h"

#include "native_client/src/shared/platform/nacl_check.h"

#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/gio/gio_shm_unbounded.h"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"


size_t    gNumBytes = 10 * NACL_MAP_PAGESIZE;
uint32_t  gLinearGeneratorSeed = 0xdeadbeef;

struct DataGenerator {
  void          (*Dtor)(struct DataGenerator *self);
  uint8_t       (*Next)(struct DataGenerator *self);
};

struct LinearGenerator {
  struct DataGenerator  base;
  uint32_t              state;
};

uint8_t LinearGeneratorNext(struct DataGenerator *vself) {
  struct LinearGenerator *self = (struct LinearGenerator *) vself;
  uint8_t rv;

  rv = (uint8_t) (self->state ^ (self->state >> 8) ^ (self->state >> 16));
  self->state = 1664525 * self->state + 1013904223;  /* numerical recipies */
  return rv;
}

void LinearGeneratorDtor(struct DataGenerator *vself) {
  struct LinearGenerator *self = (struct LinearGenerator *) vself;
  self->base.Dtor = NULL;
  self->base.Next = NULL;
}

void LinearGeneratorCtor(struct LinearGenerator *self) {
  self->base.Dtor = LinearGeneratorDtor;
  self->base.Next = LinearGeneratorNext;
  self->state = gLinearGeneratorSeed;
}

size_t FillGioWithGenerator(struct NaClGioShmUnbounded  *ngsup,
                            struct DataGenerator        *genp,
                            size_t                      nbytes) {
  size_t  nerrs = 0;
  size_t  ix;
  uint8_t buf[256];
  size_t  jx;

  (*ngsup->base.vtbl->Seek)(&ngsup->base, 0, SEEK_SET);
  for (ix = 0; ix < nbytes; ) {
    size_t  ask;
    ssize_t got;

    ask = (*genp->Next)(genp) + 1;
    if (ask > (nbytes - ix)) {
      ask = nbytes - ix;
    }
    for (jx = 0; jx < ask; ++jx) {
      buf[jx] = (*genp->Next)(genp);
      NaClLog(5, " %02x\n", buf[jx]);
    }
    NaClLog(1, "gio_shm_unbounded_test: writing %"NACL_PRIdS" bytes\n", ask);
    if ((size_t) (got = (*ngsup->base.vtbl->Write)(&ngsup->base, buf, ask))
        != ask) {
      ++nerrs;
      NaClLog(LOG_FATAL,
              ("gio_shm_unbounded_test: unexpected write return.  ask"
               " %"NACL_PRIdS" bytes, got %"NACL_PRIdS"\n"),
              ask,
              got);
    }
    ix += ask;
  }
  return nerrs;
}

size_t CheckGioWithGenerator(struct NaClGioShmUnbounded  *ngsup,
                             struct DataGenerator        *genp,
                             size_t                      nbytes) {
  size_t  nerrs = 0;
  size_t  ix;
  uint8_t buf[256];
  uint8_t actual[256];
  size_t  jx;

  (*ngsup->base.vtbl->Seek)(&ngsup->base, 0, SEEK_SET);
  for (ix = 0; ix < nbytes; ) {
    size_t  ask;
    ssize_t got;

    ask = (*genp->Next)(genp) + 1;

    if (ask > (nbytes - ix)) {
      ask = nbytes - ix;
    }
    for (jx = 0; jx < ask; ++jx) {
      buf[jx] = (*genp->Next)(genp);
      NaClLog(5, " %02x\n", buf[jx]);
    }
    NaClLog(1,
            ("gio_shm_unbounded_test: reading %"NACL_PRIdS
             " bytes, %"NACL_PRIdS" remains\n"),
            ask,
            (nbytes - ix));
    if ((size_t) (got = (*ngsup->base.vtbl->Read)(&ngsup->base, actual, ask))
        != ask) {
      ++nerrs;
      NaClLog(LOG_FATAL,
              ("gio_shm_unbounded_test: unexpected read return.  ask"
               " %"NACL_PRIdS" bytes, got %"NACL_PRIdS"\n"),
              ask,
              got);
    }
    for (jx = 0; jx < ask; ++jx) {
      if (buf[jx] != actual[jx]) {
        ++nerrs;
        NaClLog(1,
                ("gio_shm_unbounded_test: byte %"NACL_PRIdS" differs:"
                 " expected 0x%02x, got 0x%02x\n"),
                jx,
                buf[jx],
                actual[jx]);
      }
    }
    ix += ask;
  }
  return nerrs;
}

size_t TestWithDataGenerators(struct NaClGioShmUnbounded *ngsup) {
  size_t                  nerrs = 0;
  struct LinearGenerator  lg;

  LinearGeneratorCtor(&lg);
  nerrs += FillGioWithGenerator(ngsup, (struct DataGenerator *) &lg, gNumBytes);
  (*lg.base.Dtor)((struct DataGenerator *) &lg);

  LinearGeneratorCtor(&lg);
  nerrs += CheckGioWithGenerator(ngsup, (struct DataGenerator *) &lg,
                                 gNumBytes);
  (*lg.base.Dtor)((struct DataGenerator *) &lg);

  return nerrs;
}


int main(int ac, char **av) {
  int                         opt;
  size_t                      nerrs = 0;
  struct NaClGioShmUnbounded  ngsu;

  NaClNrdAllModulesInit();

  while (EOF != (opt = getopt(ac, av, "n:s:"))) {
    switch (opt) {
      case 'n':
        gNumBytes = (size_t) strtoul(optarg, (char **) NULL, 0);
        break;
      case 's':
        gLinearGeneratorSeed = (uint32_t) strtoul(optarg, (char **) NULL, 0);
        break;
      default:
        fprintf(stderr,
                "Usage: gio_shm_unbounded_test [-n nbytes] [-s seed]\n");
        return EXIT_FAILURE;
    }
  }

  printf("Writing %"NACL_PRIdS" bytes.\n", gNumBytes);
  printf("Seed %"NACL_PRIu32".\n", gLinearGeneratorSeed);

  if (!NaClGioShmUnboundedCtor(&ngsu)) {
    fprintf(stderr, "NaClGioShmUnboundedCtor failed\n");
    ++nerrs;
    goto unrecoverable;
  }

  nerrs += TestWithDataGenerators(&ngsu);

 unrecoverable:
  NaClNrdAllModulesFini();

  printf("%s\n", (0 == nerrs) ? "PASSED" : "FAILED");
  return (0 == nerrs) ?  EXIT_SUCCESS : EXIT_FAILURE;
}
