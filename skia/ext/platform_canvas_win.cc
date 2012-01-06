// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <psapi.h>

#include "base/debug/trace_event.h"
#include "skia/ext/bitmap_platform_device_win.h"
#include "skia/ext/platform_canvas.h"

namespace skia {

// Disable optimizations during crash analysis.
#pragma optimize("", off)

// Crash On Failure. |address| should be a number less than 4000.
#define COF(address, condition) if (!(condition)) *((int*) address) = 0

// This is called when a bitmap allocation fails, and this function tries to
// determine why it might have failed, and crash on different
// lines. This allows us to see in crash dumps the most likely reason for the
// failure. It takes the size of the bitmap we were trying to allocate as its
// arguments so we can check that as well.
//
// Note that in a sandboxed renderer this function crashes when trying to
// call GetProcessMemoryInfo() because it tries to load psapi.dll, which
// is fine but gives you a very hard to read crash dump.
void CrashForBitmapAllocationFailure(int w, int h, unsigned int error) {
  // Store the extended error info in a place easy to find at debug time.
  unsigned int diag_error = 0;
  // If the bitmap is ginormous, then we probably can't allocate it.
  // We use 32M pixels = 128MB @ 4 bytes per pixel.
  const LONG_PTR kGinormousBitmapPxl = 32000000;
  COF(1, LONG_PTR(w) * LONG_PTR(h) < kGinormousBitmapPxl);

  // The maximum number of GDI objects per process is 10K. If we're very close
  // to that, it's probably the problem.
  const unsigned int kLotsOfGDIObjects = 9990;
  unsigned int num_gdi_objects = GetGuiResources(GetCurrentProcess(),
                                                 GR_GDIOBJECTS);
  if (num_gdi_objects == 0) {
    diag_error = GetLastError();
    COF(2, false);
  }
  COF(3, num_gdi_objects < kLotsOfGDIObjects);

  // If we're using a crazy amount of virtual address space, then maybe there
  // isn't enough for our bitmap.
  const SIZE_T kLotsOfMem = 1500000000;  // 1.5GB.
  PROCESS_MEMORY_COUNTERS_EX pmc;
  pmc.cb = sizeof(pmc);
  if (!GetProcessMemoryInfo(GetCurrentProcess(),
                            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                            sizeof(pmc))) {
    diag_error = GetLastError();
    COF(4, false);
  }
  COF(5, pmc.PagefileUsage < kLotsOfMem);
  COF(6, pmc.PrivateUsage < kLotsOfMem);
  // Ok but we are somehow out of memory?
  COF(7, error != ERROR_NOT_ENOUGH_MEMORY);
}

// Crashes the process. This is called when a bitmap allocation fails but
// unlike its cousin CrashForBitmapAllocationFailure() it tries to detect if
// the issue was a non-valid shared bitmap handle.
void CrashIfInvalidSection(HANDLE shared_section) {
  DWORD handle_info = 0;
  COF(8, ::GetHandleInformation(shared_section, &handle_info) == TRUE);
}

// Restore the optimization options.
#pragma optimize("", on)

PlatformCanvas::PlatformCanvas(int width, int height, bool is_opaque) {
  TRACE_EVENT2("skia", "PlatformCanvas::PlatformCanvas",
               "width", width, "height", height);
  initialize(width, height, is_opaque, NULL);
}

PlatformCanvas::PlatformCanvas(int width,
                               int height,
                               bool is_opaque,
                               HANDLE shared_section) {
  TRACE_EVENT2("skia", "PlatformCanvas::PlatformCanvas",
               "width", width, "height", height);
  initialize(width, height, is_opaque, shared_section);
}

PlatformCanvas::~PlatformCanvas() {
}

bool PlatformCanvas::initialize(int width,
                               int height,
                               bool is_opaque,
                               HANDLE shared_section) {
  if (initializeWithDevice(BitmapPlatformDevice::create(width,
                                                        height,
                                                        is_opaque,
                                                        shared_section)))
    return true;
  // Investigate we failed. If we know the reason, crash in a specific place.
  unsigned int error = GetLastError();
  if (shared_section)
    CrashIfInvalidSection(shared_section);
  CrashForBitmapAllocationFailure(width, height, error);
  return false;
}

}  // namespace skia
