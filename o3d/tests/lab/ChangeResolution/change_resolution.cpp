// @@REWRITE(insert c-copyright)
// @@REWRITE(delete-start)
// Copyright 2008 Google Inc. All Rights Reserved.
// Author: thomaslewis@google.com (Thomas Lewis)
// @@REWRITE(delete-end)

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
