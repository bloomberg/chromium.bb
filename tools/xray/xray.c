/*
 * Copyright 2008, Google Inc.
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

// XRay -- a simple profiler for Native Client

#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "xray_priv.h"

#if defined(XRAY)

#define RDTSC(_x)      __asm__ __volatile__ ("rdtsc" : "=A" (_x));
#define FORCE_INLINE  __attribute__((always_inline))

/* use a TLS variable for cheap thread uid */
volatile __thread int g_xray_thread_id;


struct XRayTraceStackEntry {
  uint32_t depth_addr;
  uint64_t tsc;
  uint32_t dest;
  uint32_t annotation_index;
};


struct XRayTraceBufferEntry {
  uint32_t depth_addr;
  uint32_t annotation_index;
  uint64_t ticks;
};


struct XRayTraceFrameEntry {
  /* indices into global tracebuffer */
  int start;
  int end;
  uint64_t start_tsc;
  uint64_t end_tsc;
  uint64_t total_ticks;
  int annotation_count;
  bool valid;
};


struct XRayTraceFrame {
  struct XRayTraceFrameEntry *entry;
  int head;
  int tail;
  int count;
};


struct XRayTotal {
  int index;
  int frame;
  uint64_t ticks;
};


struct XRayTraceCapture {
  /* common variables share cache line */
  volatile void *recording;
  uint32_t stack_depth;
  uint32_t max_stack_depth;
  int buffer_index;
  int buffer_size;
  int disabled;
  int annotation_count;
  struct XRaySymbolTable *symbols;
  bool initialized;
  uint32_t annotation_filter;
  uint32_t guard0;
  struct XRayTraceStackEntry stack[XRAY_TRACE_STACK_SIZE] XRAY_ALIGN64;
  uint32_t guard1;
  char annotation[XRAY_ANNOTATION_STACK_SIZE] XRAY_ALIGN64;
  uint32_t guard2;
  struct XRayTraceBufferEntry *buffer;
  struct XRayTraceFrame frame;
} XRAY_ALIGN64;


#ifdef __cplusplus
extern "C" {
#endif

XRAY_NO_INSTRUMENT void __cyg_profile_func_enter(void *this_fn,
                                                 void *call_site);
XRAY_NO_INSTRUMENT void __cyg_profile_func_exit(void *this_fn,
                                                void *call_site);
XRAY_NO_INSTRUMENT void __xray_profile_append_annotation(
    struct XRayTraceStackEntry *se, struct XRayTraceBufferEntry *be);
XRAY_NO_INSTRUMENT int XRayTraceBufferGetTraceCountForFrame(int frame);
XRAY_NO_INSTRUMENT int XRayTraceBufferIncrementIndex(int i);
XRAY_NO_INSTRUMENT int XRayTraceBufferDecrementIndex(int i);
XRAY_NO_INSTRUMENT bool XRayTraceBufferIsAnnotation(int index);
XRAY_NO_INSTRUMENT void XRayTraceBufferAppendString(char *src);
XRAY_NO_INSTRUMENT int XRayTraceBufferCopyToString(int index, char *dst);
XRAY_NO_INSTRUMENT int XRayTraceBufferSkipAnnotation(int index);
XRAY_NO_INSTRUMENT int XRayTraceBufferNextEntry(int index);
XRAY_NO_INSTRUMENT void XRayCheckGuards();
XRAY_NO_INSTRUMENT void XRayFrameMakeLabel(int counter, char *label);
XRAY_NO_INSTRUMENT int XRayFrameFindTail();

#ifdef __cplusplus
}
#endif


struct XRayTraceCapture g_xray = {
  NULL, 0, 0, 0, 0, 0, 0, NULL, false, 0xFFFFFFFF
};


/* increments the trace index, wrapping around if needed */
XRAY_FORCE_INLINE int XRayTraceBufferIncrementIndex(int index) {
  ++index;
  if (index >= g_xray.buffer_size)
    index = 0;
  return index;
}


/* decrements the trace index, wrapping around if needed */
XRAY_FORCE_INLINE int XRayTraceBufferDecrementIndex(int index) {
  --index;
  if (index < 0)
    index = g_xray.buffer_size - 1;
  return index;
}


/* returns true if the trace entry is an annotation string */
FORCE_INLINE bool XRayTraceBufferIsAnnotation(int index) {
  struct XRayTraceBufferEntry *be = &g_xray.buffer[index];
  char *dst = (char *)be;
  return 0 == *dst;
}


/* asserts that the guard values haven't changed */
void XRayCheckGuards() {
  assert(g_xray.guard0 == XRAY_GUARD_VALUE);
  assert(g_xray.guard1 == XRAY_GUARD_VALUE);
  assert(g_xray.guard2 == XRAY_GUARD_VALUE);
}


/* not very accurate, as the annotation strings will also
** be counted as "entries" */
int XRayTraceBufferGetTraceCountForFrame(int frame) {
  assert(true == g_xray.initialized);
  assert(frame >= 0);
  assert(frame < g_xray.frame.count);
  assert(NULL == g_xray.recording);
  int start = g_xray.frame.entry[frame].start;
  int end = g_xray.frame.entry[frame].end;
  int num;
  if (start < end)
    num = end - start;
  else
    num = g_xray.buffer_size - (start - end);
  return num;
}


/* Generic memory malloc for XRay */
/* validates pointer returned by malloc */
/* memsets memory block to zero */
void* XRayMalloc(size_t t) {
  void *data;
  data = malloc(t);
  if (NULL == data) {
    printf("XRay: malloc(%d) failed, panic shutdown!\n", t);
    exit(-1);
  }
  memset(data, 0, t);
  return data;
}


/* Generic memory free for XRay */
void XRayFree(void *data) {
  assert(NULL != data);
  free(data);
}


/* appends string to trace buffer */
void XRayTraceBufferAppendString(char *src) {
  int index = g_xray.buffer_index;
  bool done = false;
  int start_index = 1;
  int s = 0;
  int i;
  char *dst = (char *)&g_xray.buffer[index];
  const int num = sizeof(g_xray.buffer[index]);
  dst[0] = 0;
  while (!done) {
    for (i = start_index; i < num; ++i) {
      dst[i] = src[s];
      if (0 == src[s]) {
        dst[i] = 0;
        done = true;
        break;
      }
      ++s;
    }
    index = XRayTraceBufferIncrementIndex(index);
    dst = (char *)&g_xray.buffer[index];;
    start_index = 0;
  }
  g_xray.buffer_index = index;
}


/* copies annotation from trace buffer to output string */
int XRayTraceBufferCopyToString(int index, char *dst) {
  assert(XRayTraceBufferIsAnnotation(index));
  bool done = false;
  int i;
  int d = 0;
  int start_index = 1;
  while (!done) {
    char *src = (char *) &g_xray.buffer[index];
    const int num = sizeof(g_xray.buffer[index]);
    for (i = start_index; i < num; ++i) {
      dst[d] = src[i];
      if (0 == src[i]) {
        done = true;
        break;
      }
      ++d;
    }
    index = XRayTraceBufferIncrementIndex(index);
    start_index = 0;
  }
  return index;
}


/* Main profile capture function that is called at the start */
/* of every instrumented function.  This function is implicitly */
/* called when code is compilied with the -finstrument-functions option */
void __cyg_profile_func_enter(void *this_fn, void *call_site) {

  if (&g_xray_thread_id == g_xray.recording) {
    uint32_t depth = g_xray.stack_depth;
    if (depth < g_xray.max_stack_depth) {
      struct XRayTraceStackEntry *se = &g_xray.stack[depth];
      uint32_t addr = (uint32_t)this_fn;
      se->depth_addr = XRAY_PACK_DEPTH_ADDR(depth, addr);
      se->dest = g_xray.buffer_index;
      se->annotation_index = 0;
      RDTSC(se->tsc);
      g_xray.buffer_index =
        XRayTraceBufferIncrementIndex(g_xray.buffer_index);
    }
    ++g_xray.stack_depth;
  }
}


/* Main profile capture function that is called at the exit of */
/* every instrumented function.  This function is implicity called */
/* when the code is compiled with the -finstrument-functions option */
void __cyg_profile_func_exit(void *this_fn, void *call_site) {
  if (&g_xray_thread_id == g_xray.recording) {
    --g_xray.stack_depth;
    if (g_xray.stack_depth < g_xray.max_stack_depth) {
      uint32_t depth = g_xray.stack_depth;
      struct XRayTraceStackEntry *se = &g_xray.stack[depth];
      uint32_t buffer_index = se->dest;
      uint64_t tsc;
      struct XRayTraceBufferEntry *be = &g_xray.buffer[buffer_index];
      RDTSC(tsc);
      be->depth_addr = se->depth_addr;
      be->ticks = tsc - se->tsc;
      be->annotation_index = 0;
      if (0 != se->annotation_index)
        __xray_profile_append_annotation(se, be);
    }
  }
}


/* special case appending annotation string to trace buffer */
/* this function should only ever be called from __cyg_profile_func_exit() */
void __xray_profile_append_annotation(struct XRayTraceStackEntry *se,
                                      struct XRayTraceBufferEntry *be) {
  struct XRayTraceStackEntry *parent = se - 1;
  int start = parent->annotation_index;
  be->annotation_index = g_xray.buffer_index;
  char *str = &g_xray.annotation[start];
  XRayTraceBufferAppendString(str);
  *str = 0;
  ++g_xray.annotation_count;
}



/* annotates the trace buffer. no filtering. */
void __XRayAnnotate(const char *fmt, ...) {
  va_list args;
  /* only annotate functions recorded in the trace buffer */
  if (true == g_xray.initialized) {
    if (0 == g_xray.disabled) {
      if (&g_xray_thread_id == g_xray.recording) {
        char buffer[1024];
        int r;
        va_start(args, fmt);
        r = vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        {
          /* get current string ptr */
          int depth = g_xray.stack_depth - 1;
          struct XRayTraceStackEntry *se = &g_xray.stack[depth];
          if (0 == se->annotation_index) {
            struct XRayTraceStackEntry *parent = se - 1;
            se->annotation_index = parent->annotation_index;
          }
          char *dst = &g_xray.annotation[se->annotation_index];
          strcpy(dst, buffer);
          int len = strlen(dst);
          se->annotation_index += len;
        }
      }
    }
  }
}


/* annotates the trace buffer with user strings.  can be filtered. */
void __XRayAnnotateFiltered(const uint32_t filter, const char *fmt, ...) {
  va_list args;
  if (true == g_xray.initialized) {
    if (0 != (filter & g_xray.annotation_filter)) {
      if (0 == g_xray.disabled) {
        if (&g_xray_thread_id == g_xray.recording) {
          char buffer[1024];
          int r;
          va_start(args, fmt);
          r = vsnprintf(buffer, sizeof(buffer), fmt, args);
          va_end(args);
          {
            /* get current string ptr */
            int depth = g_xray.stack_depth - 1;
            struct XRayTraceStackEntry *se = &g_xray.stack[depth];
            if (0 == se->annotation_index) {
              struct XRayTraceStackEntry *parent = se - 1;
              se->annotation_index = parent->annotation_index;
            }
            char *dst = &g_xray.annotation[se->annotation_index];
            strcpy(dst, buffer);
            int len = strlen(dst);
            se->annotation_index += len;
          }
        }
      }
    }
  }
}


/* allows user to specify annotation filter value, a 32 bit mask */
void XRaySetAnnotationFilter(uint32_t filter) {
  g_xray.annotation_filter = filter;
}


/* resets xray profiler */
void XRayReset() {
  assert(true == g_xray.initialized);
  assert(NULL == g_xray.recording);
  g_xray.buffer_index = 0;
  g_xray.stack_depth = 0;
  g_xray.disabled = 0;
  g_xray.frame.head = 0;
  g_xray.frame.tail = 0;
  memset(g_xray.frame.entry, 0,
    sizeof(g_xray.frame.entry[0]) * g_xray.frame.count);
  memset(&g_xray.stack, 0,
    sizeof(g_xray.stack[0]) * XRAY_TRACE_STACK_SIZE);
  XRayCheckGuards();
}


/* change the maximum stack depth captures are made to */
void XRaySetMaxStackDepth(int stack_depth) {
  assert(true == g_xray.initialized);
  assert(NULL == g_xray.recording);
  if (stack_depth < 1)
    stack_depth = 1;
  if (stack_depth >= XRAY_TRACE_STACK_SIZE)
    stack_depth = (XRAY_TRACE_STACK_SIZE - 1);
  g_xray.max_stack_depth = stack_depth;
}


int XRayFramePrev(int i) {
  i = i - 1;
  if (i < 0)
    i = g_xray.frame.count - 1;
  return i;
}


int XRayFrameNext(int i) {
  i = i + 1;
  if (i >= g_xray.frame.count)
    i = 0;
  return i;
}


void XRayFrameMakeLabel(int counter, char *label) {
  sprintf(label, "@@@frame%d", counter);
}


/* scans the ring buffer going backwards to find last valid complete frame */
int XRayFrameFindTail() {
  int head = g_xray.frame.head;
  int index = XRayFramePrev(head);
  int total_capture = 0;
  int last_valid_frame = index;
  /* check for no captures */
  if (g_xray.frame.head == g_xray.frame.tail)
    return g_xray.frame.head;
  /* go back and invalidate all captures that have been stomped */
  while (index != head) {
    bool valid = g_xray.frame.entry[index].valid;
    if (valid) {
      total_capture += XRayTraceBufferGetTraceCountForFrame(index) + 1;
      if (total_capture < g_xray.buffer_size) {
        last_valid_frame = index;
        g_xray.frame.entry[index].valid = true;
      } else {
        g_xray.frame.entry[index].valid = false;
      }
    }
    index = XRayFramePrev(index);
  }
  return last_valid_frame;
}


/* starts a new frame and enables capturing */
/* must be paired with XRayEndFrame() */
void XRayStartFrame() {
  int i = g_xray.frame.head;
  assert(true == g_xray.initialized);
  assert(NULL == g_xray.recording);
  XRayCheckGuards();
  /* add a trace entry marker so we can detect wrap around stomping */
  struct XRayTraceBufferEntry *be = &g_xray.buffer[g_xray.buffer_index];
  be->depth_addr = XRAY_FRAME_MARKER;
  g_xray.buffer_index = XRayTraceBufferIncrementIndex(g_xray.buffer_index);
  /* set start of the frame we're about to trace */
  g_xray.frame.entry[i].start = g_xray.buffer_index;
  g_xray.disabled = 0;
  g_xray.stack_depth = 1;
  /* the trace stack[0] is reserved */
  memset(&g_xray.stack[0], 0, sizeof(g_xray.stack[0]));
  /* annotation index 0 is reserved to indicate no annotation */
  g_xray.stack[0].annotation_index = 1;
  g_xray.annotation[0] = 0;
  g_xray.annotation[1] = 0;
  g_xray.annotation_count = 0;
  g_xray.recording = &g_xray_thread_id;
  RDTSC(g_xray.frame.entry[i].start_tsc);
}


/* ends a frame and disables capturing */
/* must be paired with XRayStartFrame() */
/* advances to the next frame */
void XRayEndFrame() {
  int i = g_xray.frame.head;
  assert(true == g_xray.initialized);
  assert(NULL != g_xray.recording);
  assert(0 == g_xray.disabled);
  assert(1 == g_xray.stack_depth);
  RDTSC(g_xray.frame.entry[i].end_tsc);
  g_xray.frame.entry[i].total_ticks =
    g_xray.frame.entry[i].end_tsc - g_xray.frame.entry[i].start_tsc;
  g_xray.recording = NULL;
  g_xray.frame.entry[i].end = g_xray.buffer_index;
  g_xray.frame.entry[i].valid = true;
  g_xray.frame.entry[i].annotation_count = g_xray.annotation_count;
  g_xray.frame.head = XRayFrameNext(g_xray.frame.head);
  /* if we've filled the table, bump the tail */
  if (g_xray.frame.head == g_xray.frame.tail)
    g_xray.frame.tail = XRayFrameNext(g_xray.frame.tail);
  g_xray.frame.tail = XRayFrameFindTail();
  /* check that we didn't stomp over trace entry marker */
  int marker = XRayTraceBufferDecrementIndex(g_xray.frame.entry[i].start);
  struct XRayTraceBufferEntry *be = &g_xray.buffer[marker];
  if (be->depth_addr != XRAY_FRAME_MARKER) {
    fprintf(stderr,
      "XRay: XRayStopFrame() detects insufficient trace buffer size!\n");
    XRayReset();
  } else {
    /* replace marker with an empty annotation string */
    be->depth_addr = XRAY_NULL_ANNOTATION;
    XRayCheckGuards();
  }
}


/* get the last frame captured.  Do not call while capturing   */
/* (ie call outside of XRayStartFrame() / XRayStopFrame() pair */
int XRayGetLastFrame() {
  assert(true == g_xray.initialized);
  assert(NULL == g_xray.recording);
  assert(0 == g_xray.disabled);
  assert(1 == g_xray.stack_depth);
  int last_frame = XRayFramePrev(g_xray.frame.head);
  return last_frame;
}


/* Disables capturing until a paired XRayEnableCapture() is called */
/* This call can be nested, but must be paired with an enable */
/* (If you need to just exclude a specific function and not its */
/* children, the XRAY_NO_INSTRUMENT modifier might be better) */
void XRayDisableCapture() {
  assert(true == g_xray.initialized);
  assert(NULL != g_xray.recording);
  ++g_xray.disabled;
  g_xray.recording = NULL;
}


/* Re-enables capture.  Must be paired with XRayDisableCapture() */
void XRayEnableCapture() {
  assert(true == g_xray.initialized);
  assert(NULL == g_xray.recording);
  assert(0 < g_xray.disabled);
  --g_xray.disabled;
  if (0 == g_xray.disabled) {
    g_xray.recording = &g_xray_thread_id;
  }
}





/* the entry in the tracebuffer at index is an annotation string */
/* calculate the next index value representing the next trace entry */
int XRayTraceBufferSkipAnnotation(int index) {
  /* Annotations are strings embedded in the trace buffer. */
  /* An annotation string can span multiple trace entries. */
  /* Skip over the string by looking for zero termination. */
  assert(XRayTraceBufferIsAnnotation(index));
  bool done = false;
  int start_index = 1;
  int i;
  while (!done) {
    char *str = (char *) &g_xray.buffer[index];
    const int num = sizeof(g_xray.buffer[index]);
    for (i = start_index; i < num; ++i) {
      if (0 == str[i]) {
        done = true;
        break;
      }
    }
    index = XRayTraceBufferIncrementIndex(index);
    start_index = 0;
  }
  return index;
}


/* starting at index, return the index into the trace buffer */
/* for the next trace entry.  index can wrap (ringbuffer) */
int XRayTraceBufferNextEntry(int index) {
  if (XRayTraceBufferIsAnnotation(index))
    index = XRayTraceBufferSkipAnnotation(index);
  else
    index = XRayTraceBufferIncrementIndex(index);
  return index;
}


/* Dumps the trace report for a given frame */
void XRayTraceReport(FILE *f, int frame, char *label, float cutoff) {
  int index;
  int start;
  int end;
  float total;
  int bad_depth = 0;
  char space[257];
  memset(space, ' ', 256);
  space[256] = 0;
  if (NULL == f) {
    f = stdout;
  }
  fprintf(f,
  "======================================================================\n");
  if (NULL != label)
    fprintf(f, "label %s\n", label);
  fprintf(f, "\n");
  fprintf(f,
  "   Address        Ticks   Percent      Function  <optional annotation>\n");
  fprintf(f,
  "----------------------------------------------------------------------\n");
  start = g_xray.frame.entry[frame].start;
  end = g_xray.frame.entry[frame].end;
  total = (float)g_xray.frame.entry[frame].total_ticks;
  index = start;
  while (index != end) {
    if (!XRayTraceBufferIsAnnotation(index)) {
      const char *symbol_name;
      char annotation[1024];
      struct XRayTraceBufferEntry *e = &g_xray.buffer[index];
      uint32_t depth = XRAY_EXTRACT_DEPTH(e->depth_addr);
      uint32_t addr = XRAY_EXTRACT_ADDR(e->depth_addr);
      uint32_t ticks = (e->ticks);
      uint32_t annotation_index = (e->annotation_index);
      float percent = 100.0f * (float)ticks / total;
      if (percent >= cutoff) {
        if (depth > 250) bad_depth++;
        struct XRaySymbol *symbol;
        symbol = XRaySymbolTableLookup(g_xray.symbols, addr);
        symbol_name = XRaySymbolGetName(symbol);
        if (0 != annotation_index) {
          XRayTraceBufferCopyToString(annotation_index, annotation);
        } else {
          strcpy(annotation, "");
        }
        fprintf(f, "0x%08X   %10ld     %5.1f     %s%s %s\n",
                (unsigned int)addr, (long int)ticks, percent,
                &space[256 - depth], symbol_name, annotation);
      }
    }
    index = XRayTraceBufferNextEntry(index);
  }
}


int qcompare(const void *a, const void *b) {
  struct XRayTotal *ia = (struct XRayTotal *)a;
  struct XRayTotal *ib = (struct XRayTotal *)b;
  return ia->ticks - ib->ticks;
}


/* dumps a frame report */
void XRayFrameReport(FILE *f) {
  int i;
  int head;
  int frame;
  int counter = 0;
  int total_capture = 0;
  struct XRayTotal *totals;
  totals = (struct XRayTotal*)
    alloca(g_xray.frame.count * sizeof(struct XRayTotal));
  frame = g_xray.frame.tail;
  head = g_xray.frame.head;
  fprintf(f, "\n");
  fprintf(f,
  "Frame#      Total Ticks      Capture size    Annotations   Label\n");
  fprintf(f,
  "----------------------------------------------------------------------\n");
  while (frame != head) {
    int64_t total_ticks = g_xray.frame.entry[frame].total_ticks;
    int capture_size = XRayTraceBufferGetTraceCountForFrame(frame);
    int annotation_count = g_xray.frame.entry[frame].annotation_count;
    bool valid = g_xray.frame.entry[frame].valid;
    char label[XRAY_MAX_LABEL];
    XRayFrameMakeLabel(counter, label);
    fprintf(f, "   %3d %s     %10ld        %10d     %10d   %s\n",
      counter,
      valid ? " " : "*",
      (long int)total_ticks,
      capture_size,
      annotation_count,
      label);
    totals[counter].index = counter;
    totals[counter].frame = frame;
    totals[counter].ticks = total_ticks;
    total_capture += capture_size;
    frame = XRayFrameNext(frame);
    ++counter;
  }
  fprintf(f,
  "----------------------------------------------------------------------\n");
  fprintf(f,
  "XRay: %d frame(s)    %d total capture(s)\n", counter, total_capture);
  fprintf(f, "\n");
  /* sort and take average of the median cut */
  qsort(totals, counter, sizeof(struct XRayTotal), qcompare);
  fprintf(f, "\n");
  fprintf(f, "Sorted by total ticks:\n");
  fprintf(f, "\n");
  fprintf(f,
  "Frame#      Total Ticks      Capture size    Annotations   Label\n");
  fprintf(f,
  "----------------------------------------------------------------------\n");
  for (i = 0; i < counter; ++i) {
    int index = totals[i].index;
    int frame = totals[i].frame;
    int64_t total_ticks = g_xray.frame.entry[frame].total_ticks;
    int capture_size = XRayTraceBufferGetTraceCountForFrame(frame);
    int annotation_count = g_xray.frame.entry[frame].annotation_count;
    char label[XRAY_MAX_LABEL];
    XRayFrameMakeLabel(index, label);
    fprintf(f, "   %3d       %10ld        %10d     %10d   %s\n",
        index,
        (long int)total_ticks,
        capture_size,
        annotation_count,
        label);
  }
}


/* dumps a frame report followed by trace report(s) for each frame */
void XRayReport(FILE *f, float cutoff) {
  int head;
  int index;
  int counter = 0;
  fprintf(f, "Number of symbols: %d\n", XRaySymbolCount(g_xray.symbols));
  XRayFrameReport(f);
  fprintf(f, "\n");
  head = g_xray.frame.head;
  index = g_xray.frame.tail;
  while (index != head) {
    char label[XRAY_MAX_LABEL];
    fprintf(f, "\n");
    XRayFrameMakeLabel(counter, label);
    XRayTraceReport(f, index, label, cutoff);
    index = XRayFrameNext(index);
    ++counter;
  }
  fprintf(f,
  "======================================================================\n");
#if defined(XRAY_OUTPUT_HASH_COLLISIONS)
  XRayHashTableHisto(f);
#endif
}


/* writes a profile report to text file */
void XRaySaveReport(const char *filename, float cutoff) {
  FILE *f;
  f = fopen(filename, "wt");
  if (NULL != f) {
    XRayReport(f, cutoff);
    fclose(f);
  }
}


/* initializes XRay */
void XRayInit(int stack_depth, int buffer_size, int frame_count,
              const char *mapfilename) {
  int adj_frame_count = frame_count + 1;
  assert(false == g_xray.initialized);
  size_t buffer_size_in_bytes =
      sizeof(g_xray.buffer[0]) * buffer_size;
  size_t frame_size_in_bytes =
      sizeof(g_xray.frame.entry[0]) * adj_frame_count;
  g_xray.buffer =
      (struct XRayTraceBufferEntry *)XRayMalloc(buffer_size_in_bytes);
  g_xray.frame.entry =
      (struct XRayTraceFrameEntry *)XRayMalloc(frame_size_in_bytes);

  g_xray.buffer_size = buffer_size;
  g_xray.frame.count = adj_frame_count;
  g_xray.frame.head = 0;
  g_xray.frame.tail = 0;
  g_xray.disabled = 0;
  g_xray.annotation_filter = 0xFFFFFFFF;
  g_xray.guard0 = XRAY_GUARD_VALUE;
  g_xray.guard1 = XRAY_GUARD_VALUE;
  g_xray.guard2 = XRAY_GUARD_VALUE;
  g_xray.initialized = true;
  XRaySetMaxStackDepth(stack_depth);
  XRayReset();

  /* mapfile is optional; we don't need it for captures, only for reports */
  if (NULL != mapfilename) {
    g_xray.symbols = XRaySymbolTableCreate(XRAY_DEFAULT_SYMBOL_TABLE_SIZE);
    XRaySymbolTableParseMapfile(g_xray.symbols, mapfilename);
  }
}


/* shuts down and frees memory used by XRay */
void XRayShutdown() {
  assert(true == g_xray.initialized);
  assert(NULL == g_xray.recording);
  XRayCheckGuards();
  if (NULL != g_xray.symbols) {
    XRaySymbolTableFree(g_xray.symbols);
  }
  XRayFree(g_xray.frame.entry);
  XRayFree(g_xray.buffer);
  g_xray.initialized = false;
}

#endif  /* defined(XRAY) */
