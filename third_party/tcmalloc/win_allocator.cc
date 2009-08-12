// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple allocator based on the windows heap.

extern "C" {

HANDLE win_heap;

bool win_heap_init(bool use_lfh) {
  win_heap = HeapCreate(0, 0, 0);
  if (win_heap == NULL)
    return false;

  if (use_lfh) {
    ULONG enable_lfh = 2;
    HeapSetInformation(win_heap, HeapCompatibilityInformation,
                       &enable_lfh, sizeof(enable_lfh));
    // NOTE: Setting LFH may fail.  Vista already has it enabled.
    //       And under the debugger, it won't use LFH.  So we
    //       ignore any errors.
  }

  return true;
}

void* win_heap_malloc(size_t s) {
  return HeapAlloc(win_heap, 0, s);
}

void* win_heap_realloc(void* p, size_t s) {
  if (!p)
    return win_heap_malloc(s);
  return HeapReAlloc(win_heap, 0, p, s);
}

void win_heap_free(void* s) {
  HeapFree(win_heap, 0, s);
}

size_t win_heap_msize(void* p) {
  return HeapSize(win_heap, 0, p);
}

}  // extern "C"
