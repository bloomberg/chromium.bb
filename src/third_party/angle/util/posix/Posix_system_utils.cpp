//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Posix_system_utils.cpp: Implementation of OS-specific functions for Posix systems

#include "system_utils.h"

#include <sys/resource.h>
#include <dlfcn.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

namespace angle
{

void Sleep(unsigned int milliseconds)
{
    // On Windows Sleep(0) yields while it isn't guaranteed by Posix's sleep
    // so we replicate Windows' behavior with an explicit yield.
    if (milliseconds == 0)
    {
        sched_yield();
    }
    else
    {
        timespec sleepTime =
        {
            .tv_sec = milliseconds / 1000,
            .tv_nsec = (milliseconds % 1000) * 1000000,
        };

        nanosleep(&sleepTime, nullptr);
    }
}

void SetLowPriorityProcess()
{
    setpriority(PRIO_PROCESS, getpid(), 10);
}

void WriteDebugMessage(const char *format, ...)
{
    va_list vararg;
    va_start(vararg, format);
    vfprintf(stderr, format, vararg);
    va_end(vararg);
}

bool StabilizeCPUForBenchmarking()
{
    bool success = true;
    errno        = 0;
    setpriority(PRIO_PROCESS, getpid(), -20);
    if (errno)
    {
        // A friendly warning in case the test was run without appropriate permission.
        perror(
            "Warning: setpriority failed in StabilizeCPUForBenchmarking. Process will retain "
            "default priority");
        success = false;
    }
#if ANGLE_PLATFORM_LINUX
    cpu_set_t affinity;
    CPU_SET(0, &affinity);
    errno = 0;
    if (sched_setaffinity(getpid(), sizeof(affinity), &affinity))
    {
        perror(
            "Warning: sched_setaffinity failed in StabilizeCPUForBenchmarking. Process will retain "
            "default affinity");
        success = false;
    }
#else
    // TODO(jmadill): Implement for non-linux. http://anglebug.com/2923
#endif

    return success;
}

} // namespace angle
