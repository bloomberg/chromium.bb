// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_HANDLER_HOST_LINUX_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_HANDLER_HOST_LINUX_H_

#include <sys/types.h>

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/process/process_handle.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"

#if !defined(OS_ANDROID)
#include "components/crash/core/app/breakpad_linux_impl.h"
#endif

namespace base {
class SequencedTaskRunner;
class Thread;
}

#if !defined(OS_ANDROID)

namespace breakpad {

struct BreakpadInfo;

// This is the host for processes which run breakpad inside the sandbox on
// Linux or Android. We perform the crash dump from the browser because it
// allows us to be outside the sandbox.
//
// Processes signal that they need to be dumped by sending a datagram over a
// UNIX domain socket. All processes of the same type share the client end of
// this socket which is installed in their descriptor table before exec.
class CrashHandlerHostLinux
    : public base::MessagePumpForIO::FdWatcher,
      public base::MessageLoopCurrent::DestructionObserver {
 public:
  CrashHandlerHostLinux(const std::string& process_type,
                        const base::FilePath& dumps_path,
                        bool upload);
  ~CrashHandlerHostLinux() override;

  // Starts the uploader thread. Must be called immediately after creating the
  // class.
  void StartUploaderThread();

  // Get the file descriptor which processes should be given in order to signal
  // crashes to the browser.
  int GetDeathSignalSocket() const {
    return process_socket_;
  }

  // MessagePumbLibevent::Watcher impl:
  void OnFileCanWriteWithoutBlocking(int fd) override;
  void OnFileCanReadWithoutBlocking(int fd) override;

  // MessageLoopCurrent::DestructionObserver impl:
  void WillDestroyCurrentMessageLoop() override;

  // Whether we are shutting down or not.
  bool IsShuttingDown() const;

 private:
  void Init();

  // Do work on |blocking_task_runner_| for OnFileCanReadWithoutBlocking().
  void WriteDumpFile(BreakpadInfo* info,
                     std::unique_ptr<char[]> crash_context,
                     pid_t crashing_pid);

  // Continue OnFileCanReadWithoutBlocking()'s work on the IO thread.
  void QueueCrashDumpTask(std::unique_ptr<BreakpadInfo> info, int signal_fd);

  // Find crashing thread (may delay and retry) and dump on IPC thread.
  void FindCrashingThreadAndDump(
      pid_t crashing_pid,
      const std::string& expected_syscall_data,
      std::unique_ptr<char[]> crash_context,
      std::unique_ptr<crash_reporter::internal::TransitionalCrashKeyStorage>
          crash_keys,
#if defined(ADDRESS_SANITIZER)
      std::unique_ptr<char[]> asan_report,
#endif
      uint64_t uptime,
      size_t oom_size,
      int signal_fd,
      int attempt);

  const std::string process_type_;
  const base::FilePath dumps_path_;
#if !defined(OS_ANDROID)
  const bool upload_;
#endif

  int process_socket_;
  int browser_socket_;

  base::MessagePumpForIO::FdWatchController fd_watch_controller_;
  std::unique_ptr<base::Thread> uploader_thread_;
  base::AtomicFlag shutting_down_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CrashHandlerHostLinux);
};

}  // namespace breakpad

#endif  // !defined(OS_ANDROID)

#if !defined(OS_CHROMEOS)

namespace crashpad {

class CrashHandlerHost : public base::MessagePumpForIO::FdWatcher,
                         public base::MessageLoopCurrent::DestructionObserver {
 public:
  // An interface for observers to be notified when a child process is crashing.
  class Observer {
   public:
    // Called when a child process is crashing. pid is the child's process ID in
    // the CrashHandlerHost's PID namespace. signo is the signal the child
    // received. Observers are notified synchronously while the child process
    // is blocked in its signal handler. Observers may not call AddObserver()
    // or RemoveObserver() in this method.
    virtual void ChildReceivedCrashSignal(base::ProcessId pid, int signo) = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Return a pointer to the global CrashHandlerHost instance, which is created
  // by the first call to this method.
  static CrashHandlerHost* Get();

  // Get the file descriptor which processes should be given in order to signal
  // crashes to the browser.
  int GetDeathSignalSocket();

 protected:
  ~CrashHandlerHost() override;

 private:
  CrashHandlerHost();

  void Init();
  bool ReceiveClientMessage(int client_fd,
                            base::ScopedFD* handler_fd,
                            bool* write_minidump_to_database);
  void NotifyCrashSignalObservers(base::ProcessId pid, int signo);

  // MessagePumbLibevent::Watcher impl:
  void OnFileCanWriteWithoutBlocking(int fd) override;
  void OnFileCanReadWithoutBlocking(int fd) override;

  // MessageLoopCurrent::DestructionObserver impl:
  void WillDestroyCurrentMessageLoop() override;

  base::Lock observers_lock_;
  std::set<Observer*> observers_;
  base::MessagePumpForIO::FdWatchController fd_watch_controller_;
  base::ScopedFD process_socket_;
  base::ScopedFD browser_socket_;

  DISALLOW_COPY_AND_ASSIGN(CrashHandlerHost);
};

}  // namespace crashpad

#endif  // !defined(OS_CHROMEOS)

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_HANDLER_HOST_LINUX_H_
