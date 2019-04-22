// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_

#include "base/environment.h"
#include "base/memory/shared_memory.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/process_type.h"
#include "ipc/ipc_sender.h"
#include "services/service_manager/public/mojom/service.mojom.h"

#if defined(OS_MACOSX)
#include "base/process/port_provider_mac.h"
#endif

namespace base {
class CommandLine;
class PersistentMemoryAllocator;
}

namespace content {

class BrowserChildProcessHostDelegate;
class ChildProcessHost;
class SandboxedProcessLauncherDelegate;
struct ChildProcessData;

// This represents child processes of the browser process, i.e. plugins. They
// will get terminated at browser shutdown.
class CONTENT_EXPORT BrowserChildProcessHost : public IPC::Sender {
 public:
  // Used to create a child process host. The delegate must outlive this object.
  // |process_type| needs to be either an enum value from ProcessType or an
  // embedder-defined value.
  static BrowserChildProcessHost* Create(
      content::ProcessType process_type,
      BrowserChildProcessHostDelegate* delegate);

  // Used to create a child process host, connecting the process to the
  // Service Manager as a new service instance identified by |service_name| and
  // (optional) |instance_id|.
  static BrowserChildProcessHost* Create(
      content::ProcessType process_type,
      BrowserChildProcessHostDelegate* delegate,
      const std::string& service_name);

  // Returns the child process host with unique id |child_process_id|, or
  // nullptr if it doesn't exist. |child_process_id| is NOT the process ID, but
  // is the same unique ID as |ChildProcessData::id|.
  static BrowserChildProcessHost* FromID(int child_process_id);

  ~BrowserChildProcessHost() override {}

  // Derived classes call this to launch the child process asynchronously.
  virtual void Launch(
      std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
      std::unique_ptr<base::CommandLine> cmd_line,
      bool terminate_on_shutdown) = 0;

  virtual const ChildProcessData& GetData() const = 0;

  // Returns the ChildProcessHost object used by this object.
  virtual ChildProcessHost* GetHost() const = 0;

  // Returns the termination info of a child.
  // |known_dead| indicates that the child is already dead. On Linux, this
  // information is necessary to retrieve accurate information. See
  // ChildProcessLauncher::GetChildTerminationInfo() for more details.
  virtual ChildProcessTerminationInfo GetTerminationInfo(bool known_dead) = 0;

  // Take ownership of a "shared" metrics allocator (if one exists).
  virtual std::unique_ptr<base::PersistentMemoryAllocator>
  TakeMetricsAllocator() = 0;

  // Sets the user-visible name of the process.
  virtual void SetName(const base::string16& name) = 0;

  // Sets the name of the process used for metrics reporting.
  virtual void SetMetricsName(const std::string& metrics_name) = 0;

  // Set the process. BrowserChildProcessHost will do this when the Launch
  // method is used to start the process. However if the owner of this object
  // doesn't call Launch and starts the process in another way, they need to
  // call this method so that the process is associated with this object.
  virtual void SetProcess(base::Process process) = 0;

  // Takes the ServiceRequest pipe away from this host. Use this only if you
  // intend to forego process launch and use the ServiceRequest in-process
  // instead. Calling Launch() after this is called will result in the child
  // process having no Service Manager connection.
  virtual service_manager::mojom::ServiceRequest
  TakeInProcessServiceRequest() = 0;

#if defined(OS_MACOSX)
  // Returns a PortProvider used to get the task port for child processes.
  static base::PortProvider* GetPortProvider();
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_
