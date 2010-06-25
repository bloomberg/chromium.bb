// -*- mode: c++ -*-

// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// dump_syms_tool.mm: Command line tool that uses the DumpSymbols class.
// TODO(waylonis): accept stdin

#include <mach-o/arch.h>
#include <unistd.h>

#include "common/mac/dump_syms.h"
#include "common/mac/macho_utilities.h"

struct Options {
  Options() : srcPath(), arch() { }
  NSString *srcPath;
  const NXArchInfo *arch;
};

//=============================================================================
static bool Start(const Options &options) {
  DumpSymbols *dump = [[DumpSymbols alloc]
                        initWithContentsOfFile:options.srcPath];
  
  if (!dump) {
    fprintf(stderr, "%s is not a valid Mach-o file\n",
            [options.srcPath fileSystemRepresentation]);
    return false;
  }

  if (![dump setArchitecture:[NSString
                              stringWithUTF8String:options.arch->name]]) {
    fprintf(stderr, "Architecture: %s not available in %s\n", 
            options.arch->name,
            [options.srcPath fileSystemRepresentation]);
    return false;
  }
  
  return [dump writeSymbolFile:@"-"];
}

//=============================================================================
static void Usage(int argc, const char *argv[]) {
  fprintf(stderr, "Output a Breakpad symbol file from a Mach-o file.\n");
  fprintf(stderr, "Usage: %s [-a ARCHITECTURE] <Mach-o file>\n",
          argv[0]);
  fprintf(stderr, "\t-a: Architecture type [default: native]\n");
  fprintf(stderr, "\t-h: Usage\n");
  fprintf(stderr, "\t-?: Usage\n");
}

//=============================================================================
static void SetupOptions(int argc, const char *argv[], Options *options) {
  extern int optind;
  signed char ch;

  options->arch = NXGetLocalArchInfo();

  while ((ch = getopt(argc, (char * const *)argv, "a:h?")) != -1) {
    switch (ch) {
      case 'a': {
        const NXArchInfo *arch_info = NXGetArchInfoFromName(optarg);
        if (!arch_info) {
          fprintf(stderr, "%s: Invalid architecture: %s\n", argv[0], optarg);
          Usage(argc, argv);
          exit(1);
        }
        options->arch = arch_info;
        break;
      }
      case '?':
      case 'h':
        Usage(argc, argv);
        exit(0);
        break;
    }
  }
  
  if ((argc - optind) != 1) {
    fprintf(stderr, "Must specify Mach-o file\n");
    Usage(argc, argv);
    exit(1);
  }

  options->srcPath = [[NSFileManager defaultManager]
                       stringWithFileSystemRepresentation:argv[optind]
                       length:strlen(argv[optind])];
}

//=============================================================================
int main (int argc, const char * argv[]) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  Options options;
  bool result; 

  SetupOptions(argc, argv, &options);
  result = Start(options);

  [pool release];

  return !result;
}
