// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a small program that tries to connect to the X server.  It
// continually retries until it connects or 30 seconds pass.  If it fails
// to connect to the X server or fails to find needed functiona, it returns
// an error code of -1.
//
// This is to help verify that a useful X server is available before we start
// start running tests on the build bots.

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>

#if defined(USE_AURA)
#include <X11/extensions/XInput2.h>
#endif

void Sleep(int duration_ms) {
  struct timespec sleep_time, remaining;

  // Contains the portion of duration_ms >= 1 sec.
  sleep_time.tv_sec = duration_ms / 1000;
  duration_ms -= sleep_time.tv_sec * 1000;

  // Contains the portion of duration_ms < 1 sec.
  sleep_time.tv_nsec = duration_ms * 1000 * 1000;  // nanoseconds.

  while (nanosleep(&sleep_time, &remaining) == -1 && errno == EINTR)
    sleep_time = remaining;
}

int main(int argc, char* argv[]) {
  Display* display = NULL;
  if (argv[1] && strcmp(argv[1], "--noserver") == 0) {
    display = XOpenDisplay(NULL);
    if (display) {
      fprintf(stderr, "Found unexpected connectable display %s\n",
              XDisplayName(NULL));
    }
    // Return success when we got an unexpected display so that the code
    // without the --noserver is the same, but slow, rather than inverted.
    return !display;
  }

  int kNumTries = 78;  // 78*77/2 * 10 = 30s of waiting
  int tries;
  for (tries = 0; tries < kNumTries; ++tries) {
    display = XOpenDisplay(NULL);
    if (display)
      break;
    Sleep(10 * tries);
  }

  if (!display) {
    fprintf(stderr, "Failed to connect to %s\n", XDisplayName(NULL));
    return -1;
  }

  fprintf(stderr, "Connected after %d retries\n", tries);

#if defined(USE_AURA)
  // Check for XInput2
  int opcode, event, err;
  if (!XQueryExtension(display, "XInputExtension", &opcode, &event, &err)) {
    fprintf(stderr,
        "Failed to get XInputExtension on %s.\n", XDisplayName(NULL));
    return -1;
  }

  int major = 2, minor = 0;
  if (XIQueryVersion(display, &major, &minor) == BadRequest) {
    fprintf(stderr,
        "Server does not have XInput2 on %s.\n", XDisplayName(NULL));
    return -1;
  }

  // Ask for the list of devices. This can cause some Xvfb to crash.
  int count = 0;
  XIDeviceInfo* devices = XIQueryDevice(display, XIAllDevices, &count);
  if (devices)
    XIFreeDeviceInfo(devices);

  fprintf(stderr,
      "XInput2 verified initially sane on %s.\n", XDisplayName(NULL));
#endif

  return 0;
}
