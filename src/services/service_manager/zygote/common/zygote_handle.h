// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_ZYGOTE_COMMON_ZYGOTE_HANDLE_H_
#define SERVICES_SERVICE_MANAGER_ZYGOTE_COMMON_ZYGOTE_HANDLE_H_

#include "base/callback.h"
#include "base/command_line.h"
#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "build/build_config.h"
#include "services/service_manager/zygote/common/zygote_buildflags.h"

#if !BUILDFLAG(USE_ZYGOTE_HANDLE)
#error "Can not use zygote handles without USE_ZYGOTE_HANDLE"
#endif

namespace service_manager {

#if defined(OS_POSIX)
class ZygoteCommunication;
using ZygoteHandle = ZygoteCommunication*;
#else
// Perhaps other ports may USE_ZYGOTE_HANDLE here somdeday.
#error "Can not use zygote handles on this platform"
#endif  // defined(OS_POSIX)

using ZygoteLaunchCallback =
    base::OnceCallback<pid_t(base::CommandLine*, base::ScopedFD*)>;

// Allocates and initializes the global generic zygote process, and returns the
// ZygoteHandle used to communicate with it. |launch_cb| is a callback that
// should actually launch the process, after adding additional command line
// switches to the ones composed by this function. It returns the pid created,
// and provides a control fd for it.
COMPONENT_EXPORT(SERVICE_MANAGER_ZYGOTE)
ZygoteHandle CreateGenericZygote(ZygoteLaunchCallback launch_cb);
COMPONENT_EXPORT(SERVICE_MANAGER_ZYGOTE) ZygoteHandle GetGenericZygote();

// Similar to the above but for creating an unsandboxed zygote from which
// processes which need non-generic sandboxes can be derived.
COMPONENT_EXPORT(SERVICE_MANAGER_ZYGOTE)
ZygoteHandle CreateUnsandboxedZygote(ZygoteLaunchCallback launch_cb);
COMPONENT_EXPORT(SERVICE_MANAGER_ZYGOTE) ZygoteHandle GetUnsandboxedZygote();

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_ZYGOTE_COMMON_ZYGOTE_HANDLE_H_
