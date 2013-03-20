// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char ** argv) {
  int i = fork();
  if (i < 0) {
    printf("fork error");
    return 1;
  }
  if (i > 0)
    return 0;

  int j;
  for (j = 0; j < getdtablesize(); j++)
    close(j);

  /* child (daemon) continues */
  setsid(); /* obtain a new process group */

  while (1) {
    system("touch /sdcard/host_heartbeat");
    sleep(120);
    if (fopen("/sdcard/host_heartbeat", "r") != NULL) {
      // File exists, was not removed from host.
      system("reboot");
    }
  }

  return 0;
}
