// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/embedder/set_process_title.h"

#include <stddef.h>

#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_SOLARIS)
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "base/command_line.h"
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_SOLARIS)

#if defined(OS_LINUX)
#include <sys/prctl.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
// Linux/glibc doesn't natively have setproctitle().
#include "services/service_manager/embedder/set_process_title_linux.h"
#endif  // defined(OS_LINUX)

namespace service_manager {

// TODO(jrg): Find out if setproctitle or equivalent is available on Android.
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_SOLARIS) && \
    !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

void SetProcessTitleFromCommandLine(const char** main_argv) {
  // Build a single string which consists of all the arguments separated
  // by spaces. We can't actually keep them separate due to the way the
  // setproctitle() function works.
  std::string title;
  bool have_argv0 = false;

#if defined(OS_LINUX)
  DCHECK_EQ(base::PlatformThread::CurrentId(), getpid());

  if (main_argv)
    setproctitle_init(main_argv);

  // In Linux we sometimes exec ourselves from /proc/self/exe, but this makes us
  // show up as "exe" in process listings. Read the symlink /proc/self/exe and
  // use the path it points at for our process title. Note that this is only for
  // display purposes and has no TOCTTOU security implications.
  base::FilePath target;
  base::FilePath self_exe(base::kProcSelfExe);
  if (base::ReadSymbolicLink(self_exe, &target)) {
    have_argv0 = true;
    title = target.value();
    // If the binary has since been deleted, Linux appends " (deleted)" to the
    // symlink target. Remove it, since this is not really part of our name.
    const std::string kDeletedSuffix = " (deleted)";
    if (base::EndsWith(title, kDeletedSuffix, base::CompareCase::SENSITIVE))
      title.resize(title.size() - kDeletedSuffix.size());

    // PR_SET_NAME is available in Linux 2.6.9 and newer.
    // When available at run time, this sets the short process name that shows
    // when the full command line is not being displayed in most process
    // listings.
    prctl(PR_SET_NAME, base::FilePath(title).BaseName().value().c_str());
  }
#endif  // defined(OS_LINUX)

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  for (size_t i = 1; i < command_line->argv().size(); ++i) {
    if (!title.empty())
      title += " ";
    title += command_line->argv()[i];
  }
  // Disable prepending argv[0] with '-' if we prepended it ourselves above.
  setproctitle(have_argv0 ? "-%s" : "%s", title.c_str());
}

#else

// All other systems (basically Windows & Mac) have no need or way to implement
// this function.
void SetProcessTitleFromCommandLine(const char** /* main_argv */) {}

#endif

}  // namespace service_manager
