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
