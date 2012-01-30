// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _GNU_SOURCE
#include "init_process.h"

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


static int getProcessStatus(int proc_fd, const char *process,
                            const char *field) {
  int ret = -1;

  // Open "/proc/${process}/status"
  char *buf = malloc(strlen(process) + 80);
  sprintf(buf, "%s/status", process);
  int fd = openat(proc_fd, buf, O_RDONLY);
  if (fd >= 0) {
    // Only bother to read the first 4kB. All of the fields that we
    // are interested in will show up much earlier.
    buf = realloc(buf, 4097);
    size_t sz = read(fd, buf, 4096);
    if (sz > 0) {
      // Find a matching "field"
      buf[sz] = '\000';
      char *f = malloc(strlen(field) + 4);
      sprintf(f, "\n%s:\t", field);
      char *ptr = strstr(buf, f);
      if (ptr) {
        // Extract the numerical value of the "field"
        ret = atoi(ptr + strlen(f));
      }
      free(f);
    }
    close(fd);
  }
  free(buf);
  return ret;
}

static bool hasChildren(int proc_fd, int pid) {
  bool ret = false;

  // Open "/proc"
  int fd = dup(proc_fd);
  lseek(fd, SEEK_SET, 0);
  DIR *dir = fd >= 0 ? fdopendir(fd) : NULL;
  struct dirent de, *res;
  while (dir && !readdir_r(dir, &de, &res) && res) {
    // Find numerical entries. Those are processes.
    if (res->d_name[0] <= '0' || res->d_name[0] > '9') {
      continue;
    }

    // For each process, check the parent's pid
    int ppid = getProcessStatus(proc_fd, res->d_name, "PPid");

    if (ppid == pid) {
      // We found a child process. We can stop searching, now
      ret = true;
      break;
    }
  }
  closedir(dir);
  return ret;
}

void SystemInitProcess(int init_fd, int child_pid, int proc_fd, int null_fd) {
  int ret = 0;

  // CLONE_NEWPID doesn't adjust the contents of the "/proc" file system.
  // This is very confusing. And it is even possible the kernel developers
  // will consider this a bug and fix it at some point in the future.
  // So, to be on the safe side, we explicitly retrieve our process id
  // from the "/proc" file system. This should continue to work, even if
  // the kernel eventually gets fixed so that "/proc" shows the view from
  // inside of the new pid namespace.
  pid_t init_pid = getProcessStatus(proc_fd, "self", "Pid");
  if (init_pid <= 0) {
    fprintf(stderr,
            "Failed to determine real process id of new \"init\" process\n");
    _exit(1);
  }

  // Redirect stdio to /dev/null
  if (null_fd < 0 ||
      dup2(null_fd, 0) != 0 ||
      dup2(null_fd, 1) != 1 ||
      dup2(null_fd, 2) != 2) {
    fprintf(stderr, "Failed to point stdio to a safe place\n");
    _exit(1);
  }
  close(null_fd);

  // Close all file handles
  int fds_fd = openat(proc_fd, "self/fd", O_RDONLY | O_DIRECTORY);
  DIR *dir = fds_fd >= 0 ? fdopendir(fds_fd) : NULL;
  if (dir == NULL) {
    // If we don't know the list of our open file handles, just try closing
    // all valid ones.
    for (int fd = sysconf(_SC_OPEN_MAX); --fd > 2; ) {
      if (fd != init_fd && fd != proc_fd) {
        close(fd);
      }
    }
  } else {
    // If available, it is much more efficient to just close the file
    // handles that show up in "/proc/self/fd/"
    struct dirent de, *res;
    while (!readdir_r(dir, &de, &res) && res) {
      if (res->d_name[0] < '0')
        continue;
      int fd = atoi(res->d_name);
      if (fd > 2 && fd != init_fd && fd != proc_fd && fd != dirfd(dir)) {
        close(fd);
      }
    }
    closedir(dir);
  }

  // Set up signal handler to catch SIGCHLD, but mask the signal for now
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &mask, NULL);

  // Notify other processes that we are done initializing
  if (write(init_fd, " ", 1)) { }
  close(init_fd);

  // Handle dying processes that have been re-parented to the "init" process
  for (;;) {
    // Wait until we receive a SIGCHLD signal. Our signal handler doesn't
    // actually need to do anything, though
    sigwaitinfo(&mask, NULL);

    bool retry = false;
    do {
      for (;;) {
        // Reap all exit codes of our child processes. This includes both
        // processes that originally were our immediate children, and processes
        // that have since been re-parented to be our children.
        int status;
        pid_t pid = waitpid(0, &status, __WALL | WNOHANG);
        if (pid <= 0) {
          break;
        } else {
          // We found some newly deceased child processes. Better schedule
          // another very thorough inspection of our state.
          retry = false;
        }
        if (pid == child_pid) {
          // If our first immediate child died, remember its exit code. That's
          // the exit code that we should be reporting to our parent process
          if (WIFEXITED(status)) {
            ret = WEXITSTATUS(status);
          } else if (WIFSIGNALED(status)) {
            ret = -WTERMSIG(status);
          }
        }
      }
      if (hasChildren(proc_fd, init_pid)) {
        // As long as we still have child processes, continue waiting for
        // their ultimate demise.
        retry = false;
      } else {
        if (retry) {
          // No more child processes. We can exit now.
          if (ret < 0) {
            // Try to exit with the same signal that our child terminated with
            signal(-ret, SIG_DFL);
            kill(1, -ret);
            ret = 1;
          }
          // Exit with the same exit code that our child exited with
          _exit(ret);
        } else {
          // There is a little bit of a race condition between getting
          // notifications and scanning the "/proc" file system. This is
          // particularly true, because scanning "/proc" cannot possibly be
          // an atomic operation.
          // If we find that we no longer appear to have any children, we check
          // one more time whether there are any children we can now reap.
          // They might have died while we were scanning "/proc" and if so,
          // they should now show up.
          retry = true;
        }
      }
    } while (retry);
  }
}
