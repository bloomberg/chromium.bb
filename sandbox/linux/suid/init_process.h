// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SUID_INIT_PROCESS_H_
#define SANDBOX_LINUX_SUID_INIT_PROCESS_H_

void SystemInitProcess(int init_fd, int child_pid, int proc_fd, int null_fd)
  __attribute__((noreturn));

#endif  // SANDBOX_LINUX_SUID_INIT_PROCESS_H_
