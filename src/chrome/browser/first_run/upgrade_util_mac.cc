// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/upgrade_util_mac.h"

#include <libproc.h>
#include <unistd.h>

#include "base/command_line.h"
#include "chrome/browser/mac/relauncher.h"

namespace upgrade_util {

namespace {

// Get the uid and executable path for a pid. Returns true iff successful.
// |path_buffer| must be of PROC_PIDPATHINFO_MAXSIZE length.
bool GetUIDAndPathOfPID(pid_t pid, char* path_buffer, uid_t* out_uid) {
  struct proc_bsdshortinfo info;
  int error = proc_pidinfo(pid, PROC_PIDT_SHORTBSDINFO, 0, &info, sizeof(info));
  if (error <= 0)
    return false;

  error = proc_pidpath(pid, path_buffer, PROC_PIDPATHINFO_MAXSIZE);
  if (error <= 0)
    return false;

  *out_uid = info.pbsi_uid;
  return true;
}

}  // namespace

ThisAndOtherUserCounts GetCountOfOtherInstancesOfThisBinary() {
  ThisAndOtherUserCounts counts{0, 0};

  // Get list of all processes.

  int pid_array_size_needed = proc_listallpids(nullptr, 0);
  if (pid_array_size_needed <= 0)
    return counts;
  std::vector<pid_t> pid_array(pid_array_size_needed * 4);  // slack
  int pid_count = proc_listallpids(pid_array.data(),
                                   pid_array.size() * sizeof(pid_array[0]));
  if (pid_count <= 0)
    return counts;

  pid_array.resize(pid_count);

  // Get info about this process.

  const pid_t this_pid = getpid();
  uid_t this_uid;
  char this_path[PROC_PIDPATHINFO_MAXSIZE];
  if (!GetUIDAndPathOfPID(this_pid, this_path, &this_uid))
    return counts;

  // Compare all other processes to this one.

  for (pid_t pid : pid_array) {
    if (pid == this_pid)
      continue;

    uid_t uid;
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (!GetUIDAndPathOfPID(pid, path, &uid))
      continue;

    if (strcmp(path, this_path) != 0)
      continue;

    if (uid == this_uid)
      ++counts.this_user_count;
    else
      ++counts.other_user_count;
  }

  return counts;
}

bool RelaunchChromeBrowser(const base::CommandLine& command_line) {
  return mac_relauncher::RelaunchApp(command_line.argv());
}

}  // namespace upgrade_util
