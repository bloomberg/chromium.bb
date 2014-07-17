// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The client dump tool for libheap_profiler. It attaches to a process (given
// its pid) and dumps all the libheap_profiler tracking information in JSON.
// The target process is frozen (SIGSTOP) while dumping, unless the -n opt.
// is passed (in which case the caller is responsible for un/freezing).
// The JSON output looks like this:
// {
//   "total_allocated": 908748493,   # Total bytes allocated and not freed.
//   "num_allocs":      37542,       # Number of allocations.
//   "num_stacks":      3723,        # Number of allocation call-sites.
//   "allocs":                       # Optional. Printed only with the -x arg.
//   {
//     "beef1234": {"l": 17, "f": 1, "s": "1a"},
//      ^            ^        ^       ^ Index of the corresponding entry in the
//      |            |        |         next "stacks" section. Essentially a ref
//      |            |        |         to the call site that created the alloc.
//      |            |        |
//      |            |        +-------> Flags (last arg of heap_profiler_alloc).
//      |            +----------------> Length of the Alloc.
//      +-----------------------------> Start address of the Alloc (hex).
//   },
//   "stacks":
//   {
//      "1a": {"l": 17, "f": [1074792772, 1100849864, 1100850688, ...]},
//       ^      ^        ^
//       |      |        +-----> Stack frames (absolute virtual addresses).
//       |      +--------------> Bytes allocated and not freed by the call site.
//       +---------------------> Index of the entry (as for "allocs" xref).
//                               Indexes are hex and might not be monotonic.

#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "tools/android/heap_profiler/heap_profiler.h"


static void lseek_abs(int fd, size_t off);
static void read_proc_cmdline(char* cmdline, int size);

static int pid;


static int dump_process_heap(
    int mem_fd,
    FILE* fmaps,
    bool dump_also_allocs,
    char* comment) {
  HeapStats stats;
  time_t tm;
  char cmdline[512];

  tm = time(NULL);
  read_proc_cmdline(cmdline, sizeof(cmdline));

  // Look for the mmap which contains the HeapStats in the target process vmem.
  // On Linux/Android, the libheap_profiler mmaps explicitly /dev/zero. The
  // region furthermore starts with a magic marker to disambiguate.
  bool stats_mmap_found = false;
  for (;;) {
    char line[1024];
    if (fgets(line, sizeof(line), fmaps) == NULL)
      break;

    uintptr_t start;
    uintptr_t end;
    char map_file[32];
    int ret = sscanf(line, "%"SCNxPTR"-%"SCNxPTR" rw-p %*s %*s %*s %31s",
                     &start, &end, map_file);
    const size_t size = end - start + 1;
    if (ret != 3 || strcmp(map_file, "/dev/zero") != 0 || size < sizeof(stats))
      continue;

    // The mmap looks promising. Let's check for the magic marker.
    lseek_abs(mem_fd, start);
    if (read(mem_fd, &stats, sizeof(stats)) < sizeof(stats))
      continue;

    if (stats.magic_start == HEAP_PROFILER_MAGIC_MARKER) {
      stats_mmap_found = true;
      break;
    }
  }

  if (!stats_mmap_found) {
    fprintf(stderr, "Could not find the HeapStats area. "
                    "It looks like libheap_profiler is not loaded.\n");
    return -1;
  }

  // Print JSON-formatted output.
  printf("{\n");
  printf("  \"pid\":             %d,\n", pid);
  printf("  \"time\":            %ld,\n", tm);
  printf("  \"comment\":         \"%s\",\n", comment);
  printf("  \"cmdline\":         \"%s\",\n", cmdline);
  printf("  \"pagesize\":        %d,\n", getpagesize());
  printf("  \"total_allocated\": %zu,\n", stats.total_alloc_bytes);
  printf("  \"num_allocs\":      %"PRIu32",\n", stats.num_allocs);
  printf("  \"num_stacks\":      %"PRIu32",\n", stats.num_stack_traces);

  uint32_t dbg_counted_allocs = 0;
  size_t dbg_counted_total_alloc_bytes = 0;
  bool prepend_trailing_comma = false;  // JSON syntax, I hate you.
  uint32_t i;

  // Dump the optional allocation table.
  if (dump_also_allocs) {
    printf("  \"allocs\": {");
    lseek_abs(mem_fd, (uintptr_t) stats.allocs);
    for (i = 0; i < stats.max_allocs; ++i) {
      Alloc alloc;
      if (read(mem_fd, &alloc, sizeof(alloc)) != sizeof(alloc)) {
        fprintf(stderr, "ERROR: cannot read allocation table\n");
        perror("read");
        return -1;
      }

      // Skip empty (i.e. freed) entries.
      if (alloc.start == 0 && alloc.end == 0)
        continue;

      if (alloc.end < alloc.start) {
        fprintf(stderr, "ERROR: found inconsistent alloc.\n");
        return -1;
      }

      size_t alloc_size = alloc.end - alloc.start + 1;
      size_t stack_idx = (
          (uintptr_t) alloc.st - (uintptr_t) stats.stack_traces) /
          sizeof(StacktraceEntry);
      dbg_counted_total_alloc_bytes += alloc_size;
      ++dbg_counted_allocs;

      if (prepend_trailing_comma)
        printf(",");
      prepend_trailing_comma = true;
      printf("\"%"PRIxPTR"\": {\"l\": %zu, \"f\": %"PRIu32", \"s\": \"%zx\"}",
             alloc.start, alloc_size, alloc.flags, stack_idx);
    }
    printf("},\n");

    if (dbg_counted_allocs != stats.num_allocs) {
      fprintf(stderr,
              "ERROR: inconsistent alloc count (%"PRIu32" vs %"PRIu32").\n",
              dbg_counted_allocs, stats.num_allocs);
      return -1;
    }

    if (dbg_counted_total_alloc_bytes != stats.total_alloc_bytes) {
      fprintf(stderr, "ERROR: inconsistent alloc totals (%zu vs %zu).\n",
              dbg_counted_total_alloc_bytes, stats.total_alloc_bytes);
      return -1;
    }
  }

  // Dump the distinct stack traces.
  printf("  \"stacks\": {");
  prepend_trailing_comma = false;
  dbg_counted_total_alloc_bytes = 0;
  lseek_abs(mem_fd, (uintptr_t) stats.stack_traces);
  for (i = 0; i < stats.max_stack_traces; ++i) {
    StacktraceEntry st;
    if (read(mem_fd, &st, sizeof(st)) != sizeof(st)) {
      fprintf(stderr, "ERROR: cannot read stack trace table\n");
      perror("read");
      return -1;
    }

    // Skip empty (i.e. freed) entries.
    if (st.alloc_bytes == 0)
      continue;

    dbg_counted_total_alloc_bytes += st.alloc_bytes;

    if (prepend_trailing_comma)
      printf(",");
    prepend_trailing_comma = true;

    printf("\"%"PRIx32"\":{\"l\": %zu, \"f\": [", i, st.alloc_bytes);
    size_t n = 0;
    for (;;) {
      printf("%" PRIuPTR, st.frames[n]);
      ++n;
      if (n == HEAP_PROFILER_MAX_DEPTH || st.frames[n] == 0)
        break;
      else
        printf(",");
    }
    printf("]}");
  }
  printf("}\n}\n");

  if (dbg_counted_total_alloc_bytes != stats.total_alloc_bytes) {
    fprintf(stderr, "ERROR: inconsistent stacks totals (%zu vs %zu).\n",
            dbg_counted_total_alloc_bytes, stats.total_alloc_bytes);
    return -1;
  }

  fflush(stdout);
  return 0;
}

// If the dump is interrupted, resume the target process before exiting.
static void exit_handler() {
  kill(pid, SIGCONT);
  waitpid(pid, NULL, 0);
  exit(-1);
}

static bool freeze_process() {
  if (kill(pid, SIGSTOP) != 0) {
    fprintf(stderr, "Could not freeze the target process.\n");
    perror("kill");
    return false;
  }

  signal(SIGPIPE, exit_handler);
  signal(SIGINT, exit_handler);
  return true;
}

// Unfortunately lseek takes a *signed* offset, which is unsuitable for large
// files like /proc/X/mem on 64-bit.
static void lseek_abs(int fd, size_t off) {
#define OFF_T_MAX ((off_t) ~(((uint64_t) 1) << (8 * sizeof(off_t) - 1)))
  if (off <= OFF_T_MAX) {
    lseek(fd, (off_t) off, SEEK_SET);
    return;
  }
  lseek(fd, (off_t) OFF_T_MAX, SEEK_SET);
  lseek(fd, (off_t) (off - OFF_T_MAX), SEEK_CUR);
}

static int open_proc_mem_fd() {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/mem", pid);
  int mem_fd = open(path, O_RDONLY);
  if (mem_fd < 0) {
    fprintf(stderr, "Could not attach to target process virtual memory.\n");
    perror("open");
  }
  return mem_fd;
}

static FILE* open_proc_maps() {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/maps", pid);
  FILE* fmaps = fopen(path, "r");
  if (fmaps == NULL) {
    fprintf(stderr, "Could not open %s.\n", path);
    perror("fopen");
  }
  return fmaps;
}

static void read_proc_cmdline(char* cmdline, int size) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
  int cmdline_fd = open(path, O_RDONLY);
  if (cmdline_fd < 0) {
    fprintf(stderr, "Could not open %s.\n", path);
    perror("open");
    cmdline[0] = '\0';
    return;
  }
  int length = read(cmdline_fd, cmdline, size);
  if (length < 0) {
    fprintf(stderr, "Could not read %s.\n", path);
    perror("read");
    length = 0;
  }
  close(cmdline_fd);
  cmdline[length] = '\0';
}

int main(int argc, char** argv) {
  char c;
  int ret = 0;
  bool should_freeze_process = true;
  bool dump_also_allocs = false;
  char comment[1024] = { '\0' };

  while (((c = getopt(argc, argv, "nxc:")) & 0x80) == 0) {
   switch (c) {
      case 'n':
        should_freeze_process = false;
        break;
      case 'x':
        dump_also_allocs = true;
        break;
      case 'c':
        strlcpy(comment, optarg, sizeof(comment));
        break;
     }
  }

  if (optind >= argc) {
    printf("Usage: %s [-n] [-x] [-c comment] pid\n", argv[0]);
    return -1;
  }

  pid = atoi(argv[optind]);

  if (should_freeze_process && !freeze_process())
    return -1;

  // Wait for the process to actually freeze.
  waitpid(pid, NULL, 0);

  int mem_fd = open_proc_mem_fd();
  if (mem_fd < 0)
    ret = -1;

  FILE* fmaps = open_proc_maps();
  if (fmaps == NULL)
    ret = -1;

  if (ret == 0)
    ret = dump_process_heap(mem_fd, fmaps, dump_also_allocs, comment);

  if (should_freeze_process)
    kill(pid, SIGCONT);

  // Cleanup.
  fflush(stdout);
  close(mem_fd);
  fclose(fmaps);
  return ret;
}
