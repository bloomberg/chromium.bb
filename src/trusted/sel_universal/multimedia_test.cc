/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include <unistd.h>
// NOTE: important: header hijacks "main()"
#include <SDL.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/sel_universal/multimedia.h"

// TODO(robertm): add proper usage string
static const char* kUsage =
  "Usage:\n"
  "\n";


int main(int  argc, char *argv[]) {
  NaClLogModuleInit();

  // command line parsing
  int num_rounds = 10;
  int start_value = 0;
  int final_value = 255;
  int width = 320;
  int height = 240;
  int sleep_duration = 1;
  const char* title = "Multimedia Test";
  int opt;
  while ((opt = getopt(argc, argv, "n:v")) != -1) {
    switch (opt) {
      case 'n':
       num_rounds = atoi(optarg);
        break;
      case 'v':
        NaClLogIncrVerbosity();
        break;
      default:
        fputs(kUsage, stderr);
        return -1;
    }
  }

  NaClLog(1, "starting multimedia subsystem\n");
  IMultimedia* mm = MakeMultimediaSDL(width, height, title);
  const int video_mem_size = mm->VideoBufferSize();
  char* video_mem = reinterpret_cast<char *>(malloc(video_mem_size));
  memset(video_mem, start_value, video_mem_size);
  NaClLog(1, "video_mem: start %p size 0x%x\n", video_mem, video_mem_size);

  for (int i = 0; i < num_rounds; ++i) {
    NaClLog(2, "iteration %d\n", i);
    const int slize_size = video_mem_size / num_rounds;
    memset(video_mem + (i *  slize_size), final_value, slize_size);
    mm->VideoUpdate(video_mem);
    sleep(sleep_duration);
  }
  return 0;
}
