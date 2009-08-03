// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a simple helper app that disables Cleartype and does whatever
// else it can to get the system into the configuration the layout tests
// expect.

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

BOOL g_font_smoothing_enabled = FALSE;

static void SaveInitialSettings(void) {
  BOOL ret;
  ret = SystemParametersInfo(SPI_GETFONTSMOOTHING, 0,
                             (PVOID)&g_font_smoothing_enabled, 0);
}

// Technically, all we need to do is disable ClearType. However,
// for some reason, the call to SPI_SETFONTSMOOTHINGTYPE doesn't
// seem to work, so we just disable font smoothing all together
// (which works reliably)
static void InstallLayoutTestSettings(void) {
  BOOL ret;
  ret = SystemParametersInfo(SPI_SETFONTSMOOTHING, (UINT)FALSE, (PVOID)0, 0);
}

static void RestoreInitialSettings(void) {
  BOOL ret;
  ret = SystemParametersInfo(SPI_SETFONTSMOOTHING,
                             (UINT)g_font_smoothing_enabled, (PVOID)0, 0);
}

static void SimpleSignalHandler(int sig) {
  // Try to restore the settings and then go down cleanly
  RestoreInitialSettings();
  exit(128 + sig);
}

int main(int argc, char *argv[]) {
  // Hooks the ways we might get told to clean up...
  signal(SIGINT, SimpleSignalHandler);
  signal(SIGTERM, SimpleSignalHandler);

  SaveInitialSettings();

  InstallLayoutTestSettings();

  // Let the script know we're ready
  printf("ready\n");
  fflush(stdout);

  // Wait for any key (or signal)
  getchar();

  RestoreInitialSettings();

  return 0;
}
