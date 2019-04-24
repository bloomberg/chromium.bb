/*===-- CommonProfiling.c - Profiling support library support -------------===*\
|*
|*                     The LLVM Compiler Infrastructure
|*
|* This file is distributed under the University of Illinois Open Source
|* License. See LICENSE.TXT for details.
|*
|*===----------------------------------------------------------------------===*|
|*
|* This file implements functions used by the various different types of
|* profiling implementations.
|*
\*===----------------------------------------------------------------------===*/

#include "Profiling.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#if !defined(_MSC_VER) && !defined(__MINGW32__)
#include <unistd.h>
#else
#include <io.h>
#endif
#include <stdlib.h>

static char *SavedArgs = 0;
static unsigned SavedArgsLength = 0;

static const char *OutputFilename = "llvmprof.out";

/* save_arguments - Save argc and argv as passed into the program for the file
 * we output.
 */
int save_arguments(int argc, const char **argv) {
  unsigned Length, i;
  if (SavedArgs || !argv) return argc;  /* This can be called multiple times */

  /* Check to see if there are any arguments passed into the program for the
   * profiler.  If there are, strip them off and remember their settings.
   */
  while (argc > 1 && !strncmp(argv[1], "-llvmprof-", 10)) {
    /* Ok, we have an llvmprof argument.  Remove it from the arg list and decide
     * what to do with it.
     */
    const char *Arg = argv[1];
    memmove(&argv[1], &argv[2], (argc-1)*sizeof(char*));
    --argc;

    if (!strcmp(Arg, "-llvmprof-output")) {
      if (argc == 1)
        puts("-llvmprof-output requires a filename argument!");
      else {
        OutputFilename = strdup(argv[1]);
        memmove(&argv[1], &argv[2], (argc-1)*sizeof(char*));
        --argc;
      }
    } else {
      printf("Unknown option to the profiler runtime: '%s' - ignored.\n", Arg);
    }
  }

  for (Length = 0, i = 0; i != (unsigned)argc; ++i)
    Length += strlen(argv[i])+1;

  SavedArgs = (char*)malloc(Length);
  for (Length = 0, i = 0; i != (unsigned)argc; ++i) {
    unsigned Len = strlen(argv[i]);
    memcpy(SavedArgs+Length, argv[i], Len);
    Length += Len;
    SavedArgs[Length++] = ' ';
  }

  SavedArgsLength = Length;

  return argc;
}


/*
 * Retrieves the file descriptor for the profile file.
 */
int getOutFile() {
  static int OutFile = -1;

  /* If this is the first time this function is called, open the output file
   * for appending, creating it if it does not already exist.
   */
  if (OutFile == -1) {
    OutFile = open(OutputFilename, O_CREAT | O_WRONLY, 0666);
    lseek(OutFile, 0, SEEK_END); /* O_APPEND prevents seeking */
    if (OutFile == -1) {
      fprintf(stderr, "LLVM profiling runtime: while opening '%s': ",
              OutputFilename);
      perror("");
      return(OutFile);
    }

    /* Output the command line arguments to the file. */
    {
      int PTy = ArgumentInfo;
      int Zeros = 0;
      if (write(OutFile, &PTy, sizeof(int)) < 0 ||
          write(OutFile, &SavedArgsLength, sizeof(unsigned)) < 0 ||
          write(OutFile, SavedArgs, SavedArgsLength) < 0 ) {
        fprintf(stderr,"error: unable to write to output file.");
        exit(0);
      }
      /* Pad out to a multiple of four bytes */
      if (SavedArgsLength & 3) {
        if (write(OutFile, &Zeros, 4-(SavedArgsLength&3)) < 0) {
          fprintf(stderr,"error: unable to write to output file.");
          exit(0);
        }
      }
    }
  }
  return(OutFile);
}

/* write_profiling_data - Write a raw block of profiling counters out to the
 * llvmprof.out file.  Note that we allow programs to be instrumented with
 * multiple different kinds of instrumentation.  For this reason, this function
 * may be called more than once.
 */
void write_profiling_data(enum ProfilingType PT, unsigned *Start,
                          unsigned NumElements) {
  int PTy;
  int outFile = getOutFile();

  /* Write out this record! */
  PTy = PT;
  if( write(outFile, &PTy, sizeof(int)) < 0 ||
      write(outFile, &NumElements, sizeof(unsigned)) < 0 ||
      write(outFile, Start, NumElements*sizeof(unsigned)) < 0 ) {
    fprintf(stderr,"error: unable to write to output file.");
    exit(0);
  }
}
