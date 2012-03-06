/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Exercise object proxy mapping.  Don't actually use real objects,
 * but just made-up values.
 *
 * Use a simple array to hold object / name tuples to check that the
 * object proxy can do lookups properly of all entered values as well
 * as negative examples of objects that have never been entered /
 * names that had never been issued.  This array is unsorted, so
 * searching it is inefficient, but should be obviously correct.
 */

#include "native_client/src/include/portability.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_secure_random.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"

#include "native_client/src/trusted/service_runtime/fs/obj_proxy.h"

#define SEED_BYTES  16

int                   verbose;

struct TestObj {
  void          *obj;
  uint8_t const *name;
};

struct TestData {
  struct TestObj  *obj_tbl;
  int             obj_tbl_size;
  int             obj_tbl_used;
  int             namelen;
};

void TestDataCtor(struct TestData *self, int namelen) {
  self->obj_tbl = NULL;
  self->obj_tbl_size = 0;
  self->obj_tbl_used = 0;
  self->namelen = namelen;
}

void TestDataDtor(struct TestData *self) {
  free(self->obj_tbl);
}

void TestDataEnterObj(struct TestData *self, void *obj, uint8_t const *name) {
  if (self->obj_tbl_used >= self->obj_tbl_size) {
    if (self->obj_tbl_size) {
      self->obj_tbl_size *= 2;
    } else {
      self->obj_tbl_size = 16;
    }
    self->obj_tbl = realloc(self->obj_tbl,
                            self->obj_tbl_size * sizeof *self->obj_tbl);
    if (NULL == self->obj_tbl) {
      fprintf(stderr, "obj_tbl_enter: out of memory\n");
      exit(1);
    }
  }
  self->obj_tbl[self->obj_tbl_used].obj = obj;
  self->obj_tbl[self->obj_tbl_used].name = name;
  ++self->obj_tbl_used;
}

uint8_t const *TestDataFindName(struct TestData *self, void *obj) {
  int i;

  for (i = 0; i < self->obj_tbl_used; ++i) {
    if (self->obj_tbl[i].obj == obj) {
      return self->obj_tbl[i].name;
    }
  }
  return NULL;
}

void *TestDataFindObj(struct TestData *self, uint8_t const *name) {
  int i;

  /* the dirt simple, slow, but obviously correct algorithm */
  for (i = 0; i < self->obj_tbl_used; ++i) {
    if (!memcmp(self->obj_tbl[i].name, name, self->namelen)) {
      return self->obj_tbl[i].obj;
    }
  }
  return NULL;
}

void print_name(FILE *ostr, uint8_t const *name, int namelen) {
  int i;
  int v;

  for (i = 0; i < namelen; ++i) {
    v = name[i] & 0xff;
    if (0 == (i & 0x7)) {
      putc(' ', ostr);
    }
    fprintf(ostr, "%02x", v);
  }
}


int test_for_size(int namelen, int num_samples_init, int num_bogus_init,
                  struct NaClSecureRngIf *rng) {
  struct TestData       td;
  struct NaClObjProxy   nop;
  uintptr_t             samp;
  void                  *obj;
  uint8_t const         *name;
  uint8_t const         *true_name;
  void                  *found_obj;
  int                   passed = 0;
  uint8_t               *bogus_name;
  uintptr_t num_samples = (uintptr_t)num_samples_init;
  uintptr_t num_bogus = (uintptr_t)num_bogus_init;

  bogus_name = malloc(namelen);
  printf("Testing for name length %d, %"NACL_PRIxPTR" legit samples, "
         "%"NACL_PRIxPTR" bogus lookups\n",
         namelen, num_samples, num_bogus);
  TestDataCtor(&td, namelen);

  NaClObjProxyCtor(&nop, rng, namelen);

  printf("insertion\n");
  for (samp = 0; samp < num_samples; ++samp) {
    obj = (void *) samp;
    name = NaClObjProxyInsert(&nop, obj);

    if (verbose) {
      printf("%4"NACL_PRIxPTR": ", samp);
      print_name(stdout, name, namelen);
      putchar('\n');
    }

    TestDataEnterObj(&td, obj, name);
  }

  printf("lookup\n");
  /* now look up every one */
  for (samp = 0; samp < num_samples; ++samp) {
    obj = (void *) samp;

    true_name = TestDataFindName(&td, obj);
    if (NULL == true_name) {
      printf("`True name' for object %"NACL_PRIxPTR" not found\n", samp);
      goto cleanup;
    }
    if (verbose) {
      printf("[ %4"NACL_PRIxPTR": ", samp);
      print_name(stdout, true_name, namelen);
      printf(" ]\n");
    }

    name = NaClObjProxyFindNameByObj(&nop, obj);
    if (NULL == name) {
      printf("Object %"NACL_PRIxPTR" not found!\n", samp);
      goto cleanup;
    }

    if (memcmp(name, true_name, namelen)) {
      printf("Name mismatch!  Expected ");
      print_name(stdout, true_name, namelen);
      printf(" got ");
      print_name(stdout, name, namelen);
      printf("\n");
      goto cleanup;
    }
  }

  /* now look up every one, backwards */
  printf("order reversed lookup\n");
  for (samp = num_samples; --samp > 0; ) {
    obj = (void *) samp;

    true_name = TestDataFindName(&td, obj);
    if (NULL == true_name) {
      printf("`True name' for object %"NACL_PRIxPTR" not found\n", samp);
      goto cleanup;
    }
    if (verbose) {
      printf("[ %4"NACL_PRIxPTR": ", samp);
      print_name(stdout, true_name, namelen);
      printf(" ]\n");
    }

    name = NaClObjProxyFindNameByObj(&nop, obj);
    if (NULL == name) {
      printf("Object %"NACL_PRIxPTR" not found!\n", samp);
      goto cleanup;
    }

    if (memcmp(name, true_name, namelen)) {
      printf("Name mismatch!  Expected ");
      print_name(stdout, true_name, namelen);
      printf(" got ");
      print_name(stdout, name, namelen);
      printf("\n");
      goto cleanup;
    }
  }

  printf("lookup by name\n");
  /* now look up every one */
  for (samp = 0; samp < num_samples; ++samp) {
    obj = (void *) samp;

    true_name = TestDataFindName(&td, obj);
    if (NULL == true_name) {
      printf("`True name' for object %"NACL_PRIxPTR" not found\n", samp);
      goto cleanup;
    }
    if (verbose) {
      printf("[ %4"NACL_PRIxPTR": ", samp);
      print_name(stdout, true_name, namelen);
      printf(" ]\n");
    }

    found_obj = (void *) 0xdeadbeef;
    if (!NaClObjProxyFindObjByName(&nop, true_name, &found_obj)) {
      printf("Object %"NACL_PRIxPTR" not found, name ", samp);
      print_name(stdout, true_name, namelen);
      printf("!\n");
      goto cleanup;
    }

    if (found_obj != obj) {
      printf("Object mismatch!  Expected 0x%08"
             NACL_PRIxPTR" got 0x%08"NACL_PRIxPTR"\n",
             (uintptr_t) obj, (uintptr_t) found_obj);
      goto cleanup;
    }
  }

  printf("lookup by name, rev order\n");
  /* now look up every one */
  for (samp = num_samples; --samp > 0; ) {
    obj = (void *) samp;

    true_name = TestDataFindName(&td, obj);
    if (NULL == true_name) {
      printf("`True name' for object %"NACL_PRIxPTR" not found\n", samp);
      goto cleanup;
    }
    if (verbose) {
      printf("[ %4"NACL_PRIxPTR": ", samp);
      print_name(stdout, true_name, namelen);
      printf(" ]\n");
    }

    if (!NaClObjProxyFindObjByName(&nop, true_name, &found_obj)) {
      printf("Object %"NACL_PRIxPTR" not found!\n", samp);
      goto cleanup;
    }

    if (found_obj != obj) {
      printf("Object mismatch!  Expected 0x%08"NACL_PRIxPTR
             " got 0x%08"NACL_PRIxPTR,
             (uintptr_t) obj, (uintptr_t) found_obj);
      goto cleanup;
    }
  }

  printf("Bogus objects\n");
  for (samp = num_samples; samp < num_samples + num_bogus; ++samp) {
    obj = (void *) samp;
    name = NaClObjProxyFindNameByObj(&nop, obj);
    if (NULL != name) {
      printf("Bogus object %"NACL_PRIxPTR" FOUND\n", samp);
      goto cleanup;
    }
  }

  printf("Bogus names\n");
  for (samp = 0; samp < num_bogus; ++samp) {
    (*rng->vtbl->GenBytes)(rng, bogus_name, namelen);
    if (NaClObjProxyFindObjByName(&nop, bogus_name, &obj)) {
      printf("Bogus name");
      print_name(stdout, bogus_name, namelen);
      printf(" FOUND!\n");
      goto cleanup;
    }
  }

  printf("OK\n");
  passed = 1;

cleanup:
  NaClObjProxyDtor(&nop);
  TestDataDtor(&td);
  free(bogus_name);
  return passed;
}


/*
 * A very weak linear congruential generator.  Only use for testing.
 */
struct WeakRng {
  struct NaClSecureRngIf  base;
  uint32_t                x;
};

static struct NaClSecureRngIfVtbl const kWeakRngVtbl;

int WeakRngCtor(struct WeakRng *self) {
  self->base.vtbl = &kWeakRngVtbl;
  self->x = 263;
  return 1;
}

int WeakRngTestingCtor(struct WeakRng *self,
                       uint8_t        *seed_material,
                       size_t         seed_bytes) {
  self->x = 0;
  while (seed_bytes > 0) {
    self->x = (self->x * 263) ^ seed_material[--seed_bytes];
  }
  if (0 == self->x) {
    self->x = 263;
  }
  self->base.vtbl = &kWeakRngVtbl;
  return 1;
}

void WeakRngDtor(struct NaClSecureRngIf *vself) {
  vself->vtbl = NULL;
}

uint8_t WeakRngGenByte(struct NaClSecureRngIf *vself) {
  struct WeakRng  *self = (struct WeakRng *) vself;
  uint64_t        y;

  y = (uint64_t) self->x;
  y *= 16807;
  y = y + (y >> 31);
  y = y & ((1u << 31) - 1);
  self->x = (uint32_t) y;
  return self->x;
}

static struct NaClSecureRngIfVtbl const kWeakRngVtbl = {
  WeakRngDtor,
  WeakRngGenByte,
  NaClSecureRngDefaultGenUint32,
  NaClSecureRngDefaultGenBytes,
  NaClSecureRngDefaultUniform,
};


int RunTests(int namelen_start, int namelen_end, int namelen_inc,
             int num_samples, int num_bogus,
             struct NaClSecureRngIf *rng) {
  int pass = 1;
  int namelen;

  for (namelen = namelen_start; namelen < namelen_end; namelen += namelen_inc) {
    pass = pass && test_for_size(namelen, num_samples, num_bogus, rng);
  }
  return pass;
}

int main(int ac, char **av) {
  int opt;
  int namelen_start = 8;
  int namelen_end = 32;
  int namelen_inc = 1;
  int num_samples = 1 << 16;
  int num_bogus = 1024;
  int pass = 1;

  int           seed_provided = 0;
  uint8_t       seed_material[SEED_BYTES];
  size_t        ix;
  unsigned int  seed_byte;

  struct NaClSecureRng    rng;
  struct NaClSecureRngIf  *rp;
  int                     use_weak_rng = 0;
  struct WeakRng          weak_rng;
  int                     fallback_to_weak = 1;

  memset(seed_material, 0, sizeof seed_material);

  while (-1 != (opt = getopt(ac, av, "s:e:i:n:b:vS:fFwW"))) {
    switch (opt) {
      case 's':
        namelen_start = strtol(optarg, (char **) 0, 0);
        break;
      case 'e':
        namelen_end = strtol(optarg, (char **) 0, 0);
        break;
      case 'i':
        namelen_inc = strtol(optarg, (char **) 0, 0);
        break;
      case 'n':
        num_samples = strtol(optarg, (char **) 0, 0);
        break;
      case 'b':
        num_bogus = strtol(optarg, (char **) 0, 0);
        break;
      case 'v':
        ++verbose;
        break;
      case 'S':
        for (ix = 0;
             (1 == sscanf(optarg + 2*ix, "%2x", &seed_byte))
                 && ix < sizeof seed_material;
             ++ix) {
          seed_material[ix] = seed_byte;
        }
        seed_provided = 1;
        break;
      case 'f':
        fallback_to_weak = 1;
        break;
      case 'F':
        fallback_to_weak = 0;
        break;
      case 'w':
        use_weak_rng = 1;
        break;
      case 'W':
        use_weak_rng = 0;
        break;
      default:
        fprintf(stderr,
                "Usage: obj_proxy_test [-s start_namelen] [-e end_namelen]\n"
                "                      [-i incr_namelen] [-n num_samples]\n"
                "                      [-b num_bogus_samples]\n"
                "                      [-v]\n"
                "                      [-S] hex-seed-material\n"
                "                      [-fFwW]\n"
                " -v verbose mode\n"
                " -f fall back to a weak generator if deterministic seeding\n"
                "    is not possible (default) \n"
                " -F fall back to non-deterministic seeding if\n"
                "    deterministic seeding is not possible\n"
                " -w use a weak generator\n"
                " -W use the standard secure generator (default)\n"
                "\n"
                " NB: it is bad if num_samples >= 2**(4*start_namelen)\n"
                " since the birthday paradox will cause name collisions\n"
                );
        exit(1);
    }
  }
  NaClAllModulesInit();
  if (namelen_start > namelen_end || namelen_inc <= 0) {
    fprintf(stderr, "obj_proxy_test: name length parameters are bogus\n");
    exit(1);
  }

  if (!seed_provided) {
    NaClSecureRngCtor(&rng);

    for (ix = 0; ix < sizeof seed_material; ++ix) {
      seed_material[ix] = (*rng.base.vtbl->GenByte)(&rng.base);
    }

    (*rng.base.vtbl->Dtor)(&rng.base);
  }

  printf("TEST RNG SEED: ");
  for (ix = 0; ix < sizeof seed_material; ++ix) {
    printf("%02x", seed_material[ix]);
  }
  printf("\n");

  if (use_weak_rng) {
    if (verbose) {
      printf("Using weak generator\n");
    }
    WeakRngTestingCtor(&weak_rng, seed_material, sizeof seed_material);
    rp = &weak_rng.base;
  } else {
    if (!NaClSecureRngTestingCtor(&rng, seed_material, sizeof seed_material)) {
      if (fallback_to_weak) {
        printf("Cannot set seed.  Reverting to weak generator.\n");
        WeakRngTestingCtor(&weak_rng, seed_material, sizeof seed_material);
        rp = &weak_rng.base;
      } else {
        printf("Cannot set seed.  Reverting to nondeterministic generator.\n");
        NaClSecureRngCtor(&rng);
        rp = &rng.base;
      }
    } else {
      if (verbose) {
        printf("Using standard generator\n");
      }
      rp = &rng.base;
    }
  }

  pass = pass && RunTests(namelen_start, namelen_end, namelen_inc,
                          num_samples, num_bogus, rp);

  (*rp->vtbl->Dtor)(rp);

  NaClAllModulesFini();

  printf("%s\n", pass ? "PASSED" : "FAILED");

  return !pass;
}
