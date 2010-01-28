/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dlfcn.h>

#define DEFAULT_START   "NaClMain"

int verbosity = 0;

void usage(void) {
  fprintf(stderr, "Usage: nacl [-v?] target.so\n");
}

int LoadAndRun(char *dlfile,
               char const *entry_pt,
               int verbosity) {
  void *dlhandle;
  void (*fn)(void);
  char *error;

  if (verbosity >= 2) {
    fprintf(stderr, "Requested %s to be loaded and run\n", dlfile);
  }
  dlhandle = dlopen(dlfile, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);

  if (NULL == dlhandle) {
    if (verbosity >= 1) {
      fprintf(stderr, "Load of %s failed\n", dlfile);
      fprintf(stderr, "Error: %s\n", dlerror());
    }
    return 0;
  }

  *(void **) &fn = dlsym(dlhandle, entry_pt);
  if (NULL == fn || NULL != (error = dlerror())) {
    if (verbosity >= 1) {
      fprintf(stderr, "Entry point %s not found in %s",
              entry_pt, dlfile);
    }
    return 0;
  }

  if (verbosity >= 1) {
    fprintf(stderr, "Calling entry point %s\n", entry_pt);
  }
  (*fn)();
  if (verbosity >= 1) {
    fprintf(stderr, "Done, closing handle\n");
  }

  dlclose(dlhandle);
  return 1;
}

int main(int argc, char **argv) {
  int opt;
  int ix;
  char const  *entry_pt = DEFAULT_START;
  while ((opt = getopt(argc, argv, "s:v")) != -1) switch (opt) {
      case 's':
        entry_pt = optarg;
        break;
      case 'v':
        ++verbosity;
        break;
      default:
        usage();
        exit(1);
  }

  for (ix = optind; ix < argc; ++ix) {
    if (!LoadAndRun(argv[ix], entry_pt, verbosity)) {
      return 1;
    }
  }
  return 0;
}
