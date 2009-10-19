/*
 * Copyright 2009, Google Inc.
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


// Tool to change the screen resolution on Windows.

#include <stdio.h>
#include <wchar.h>
#include "Windows.h"


int wmain(int argc, wchar_t* argv[]) {
  if (argc == 1) {
    // Output current resolution and bits per pixel.
    DEVMODE dmode;
    memset(&dmode, 0, sizeof(DEVMODE));
    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dmode)) {
      printf("Current settings (X resolution, Y resolution, bits per "
             "pixel):\n");
      printf("%lu\n%lu\n%lu\n", dmode.dmPelsWidth, dmode.dmPelsHeight,
             dmode.dmBitsPerPel);
      return 0;
    } else {
      printf("Failed to retrieve current resolution.\n");
      return 1;
    }
  } else if (argc != 4) {
    printf("Pass 3 args: X resolution, Y resolution, and bits per pixel to "
           "change to or no args to view current settings.");
    return 1;
  }

  // Read and validate arguments.
  int width = 0;
  if (!swscanf_s((const wchar_t *)argv[1], L"%d", &width)) {
    printf("Invalid value for width.  It must be a number.");
    return 1;
  }
  int height = 0;
  if (!swscanf_s((const wchar_t *)argv[2], L"%d", &height)) {
    printf("Invalid value for height.  It must be a number.");
    return 1;
  }
  int depth = 0;
   if (!swscanf_s((const wchar_t *)argv[3], L"%d", &depth)) {
     printf("Invalid value for bits per pixel.  It must be a number.");
     return 1;
   }

  // Find matching mode.
  DEVMODE dmode;
  memset(&dmode, 0, sizeof(DEVMODE));
  dmode.dmSize = sizeof(dmode);

  bool foundMode = false;
  for (int i = 0; !foundMode && EnumDisplaySettings(NULL, i, &dmode); ++i) {
    foundMode = (dmode.dmPelsWidth == (DWORD)width) &&
                (dmode.dmPelsHeight == (DWORD)height) &&
                (dmode.dmBitsPerPel == (DWORD)depth);
  }
  if (!foundMode) {
    printf("No screen mode found with given resolution and bits per pixel.\n");
    return 1;
  }

  // Change to matched mode.
  dmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

  LONG result = ChangeDisplaySettings(&dmode, CDS_UPDATEREGISTRY);
  if (result == DISP_CHANGE_SUCCESSFUL) {
    printf("Resolution changes successfully.\n");
    return 0;
  } else if (result == DISP_CHANGE_BADDUALVIEW) {
    printf("Windows XP: The settings change was unsuccessful because system "
           "is DualView capable.\n");
  } else if (result == DISP_CHANGE_BADFLAGS) {
    printf("An invalid set of flags was passed in.\n");
  } else if (result == DISP_CHANGE_BADMODE) {
    printf("The graphics mode is not supported.\n");
  } else if (result == DISP_CHANGE_BADPARAM) {
    printf("An invalid parameter was passed in. This can include an invalid "
           "flag or combination of flags.\n");
  } else if (result == DISP_CHANGE_FAILED) {
    printf("The display driver failed the specified graphics mode.\n");
  } else if (result == DISP_CHANGE_NOTUPDATED) {
    printf("Windows NT/2000/XP: Unable to write settings to the registry.\n");
  } else if (result == DISP_CHANGE_RESTART) {
    printf("The computer must be restarted in order for the graphics mode to "
           "work.\n");
  } else {
    printf("Unknown error.\n");
  }

  return 1;
}
