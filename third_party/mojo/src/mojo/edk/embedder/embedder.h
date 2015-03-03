// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_EMBEDDER_H_
#define MOJO_EDK_EMBEDDER_EMBEDDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/channel_info_forward.h"
#include "mojo/edk/embedder/process_type.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace embedder {

struct Configuration;
class PlatformSupport;
class ProcessDelegate;
typedef void* SlaveInfo;

// Basic configuration/initialization ------------------------------------------

// |Init()| sets up the basic Mojo system environment, making the |Mojo...()|
// functions available and functional. This is never shut down (except in tests
// -- see test_embedder.h).

// Returns the global configuration. In general, you should not need to change
// the configuration, but if you do you must do it before calling |Init()|.
MOJO_SYSTEM_IMPL_EXPORT Configuration* GetConfiguration();

// Must be called first, or just after setting configuration parameters, to
// initialize the (global, singleton) system.
MOJO_SYSTEM_IMPL_EXPORT void Init(scoped_ptr<PlatformSupport> platform_support);

// Basic functions -------------------------------------------------------------

// The functions in this section are available once |Init()| has been called.

// Start waiting on the handle asynchronously. On success, |callback| will be
// called exactly once, when |handle| satisfies a signal in |signals| or it
// becomes known that it will never do so. |callback| will be executed on an
// arbitrary thread, so it must not call any Mojo system or embedder functions.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
AsyncWait(MojoHandle handle,
          MojoHandleSignals signals,
          const base::Callback<void(MojoResult)>& callback);

// Creates a |MojoHandle| that wraps the given |PlatformHandle| (taking
// ownership of it). This |MojoHandle| can then, e.g., be passed through message
// pipes. Note: This takes ownership (and thus closes) |platform_handle| even on
// failure, which is different from what you'd expect from a Mojo API, but it
// makes for a more convenient embedder API.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
CreatePlatformHandleWrapper(ScopedPlatformHandle platform_handle,
                            MojoHandle* platform_handle_wrapper_handle);

// Retrieves the |PlatformHandle| that was wrapped into a |MojoHandle| (using
// |CreatePlatformHandleWrapper()| above). Note that the |MojoHandle| must still
// be closed separately.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
PassWrappedPlatformHandle(MojoHandle platform_handle_wrapper_handle,
                          ScopedPlatformHandle* platform_handle);

// Initialialization/shutdown for interprocess communication (IPC) -------------

// |InitIPCSupport()| sets up the subsystem for interprocess communication,
// making the IPC functions (in the following section) available and functional.
// (This may only be done after |Init()|.)
//
// This subsystem may be shut down, using |ShutdownIPCSupportOnIOThread()| or
// |ShutdownIPCSupport()|. None of the IPC functions may be called while or
// after either of these is called.

// Initializes a process of the given type; to be called after |Init()|.
//   - |process_delegate| must be a process delegate of the appropriate type
//     corresponding to |process_type|; its methods will be called on
//     |delegate_thread_task_runner|.
//   - |delegate_thread_task_runner|, |process_delegate|, and
//     |io_thread_task_runner| should live at least until
//     |ShutdownIPCSupport()|'s callback has been run or
//     |ShutdownIPCSupportOnIOThread()| has completed.
//   - For slave processes (i.e., |process_type| is |ProcessType::SLAVE|),
//     |platform_handle| should be connected to the handle passed to
//     |ConnectToSlave()| (in the master process). For other processes,
//     |platform_handle| is ignored (and should not be valid).
MOJO_SYSTEM_IMPL_EXPORT void InitIPCSupport(
    ProcessType process_type,
    scoped_refptr<base::TaskRunner> delegate_thread_task_runner,
    ProcessDelegate* process_delegate,
    scoped_refptr<base::TaskRunner> io_thread_task_runner,
    ScopedPlatformHandle platform_handle);

// Shuts down the subsystem initialized by |InitIPCSupport()|. This must be
// called on the I/O thread (given to |InitIPCSupport()|). This completes
// synchronously and does not result in a call to the process delegate's
// |OnShutdownComplete()|.
MOJO_SYSTEM_IMPL_EXPORT void ShutdownIPCSupportOnIOThread();

// Like |ShutdownIPCSupportOnIOThread()|, but may be called from any thread,
// signalling shutdown completion via the process delegate's
// |OnShutdownComplete()|.
MOJO_SYSTEM_IMPL_EXPORT void ShutdownIPCSupport();

// Interprocess communication (IPC) functions ----------------------------------

// Connects to a slave process to the IPC system. This should only be called in
// a process initialized (using |InitIPCSupport()|) with process type
// |ProcessType::MASTER|. |slave_info| is caller-dependent slave information,
// which should remain alive until the master process delegate's
// |OnSlaveDisconnect()| is called. |platform_handle| should be a handle to one
// end of an OS "pipe"; the slave process should |InitIPCSupport()| with
// |ProcessType::SLAVE| and the handle to the other end of this OS "pipe".
MOJO_SYSTEM_IMPL_EXPORT void ConnectToSlave(
    SlaveInfo slave_info,
    ScopedPlatformHandle platform_handle);

// A "channel" is a connection on top of an OS "pipe", on top of which Mojo
// message pipes (etc.) can be multiplexed. It must "live" on some I/O thread.
//
// There are two channel creation APIs: |CreateChannelOnIOThread()| creates a
// channel synchronously and must be called from the I/O thread, while
// |CreateChannel()| is asynchronous and may be called from any thread.
// |DestroyChannel()| is used to destroy the channel in either case and may be
// called from any thread, but completes synchronously when called from the I/O
// thread.
//
// Both creation functions have a |platform_handle| argument, which should be an
// OS-dependent handle to one side of a suitable bidirectional OS "pipe" (e.g.,
// a file descriptor to a socket on POSIX, a handle to a named pipe on Windows);
// this "pipe" should be connected and ready for operation (e.g., to be written
// to or read from).
//
// Both (synchronously) return a handle to the bootstrap message pipe on the
// channel that was (or is to be) created, or |MOJO_HANDLE_INVALID| on error
// (but note that this will happen only if, e.g., the handle table is full).
// This message pipe may be used immediately, but since channel operation
// actually begins asynchronously, other errors may still occur (e.g., if the
// other end of the "pipe" is closed) and be reported in the usual way to the
// returned handle.
//
// (E.g., a message written immediately to the returned handle will be queued
// and the handle immediately closed, before the channel begins operation. In
// this case, the channel should connect as usual, send the queued message, and
// report that the handle was closed to the other side. The message sent may
// have other handles, so there may still be message pipes "on" this channel.)
//
// Both also produce a |ChannelInfo*| (a pointer to an opaque object) -- the
// first synchronously and second asynchronously.
//
// The destruction functions are similarly synchronous and asynchronous,
// respectively, and take the |ChannelInfo*| produced by the creation functions.

// Creates a channel; must only be called from the I/O thread. |platform_handle|
// should be a handle to a connected OS "pipe". Eventually (even on failure),
// the "out" value |*channel_info| should be passed to |DestoryChannel()| to
// tear down the channel. Returns a handle to the bootstrap message pipe.
MOJO_SYSTEM_IMPL_EXPORT ScopedMessagePipeHandle
CreateChannelOnIOThread(ScopedPlatformHandle platform_handle,
                        ChannelInfo** channel_info);

typedef base::Callback<void(ChannelInfo*)> DidCreateChannelCallback;
// Creates a channel asynchronously; may be called from any thread.
// |platform_handle| should be a handle to a connected OS "pipe".
// |io_thread_task_runner| should be the |TaskRunner| for the I/O thread.
// |callback| should be the callback to call with the |ChannelInfo*|, which
// should eventually be passed to |DestroyChannel()| to tear down the channel;
// the callback will be called using |callback_thread_task_runner| if that is
// non-null, or otherwise it will be called using |io_thread_task_runner|.
// Returns a handle to the bootstrap message pipe.
MOJO_SYSTEM_IMPL_EXPORT ScopedMessagePipeHandle
CreateChannel(ScopedPlatformHandle platform_handle,
              scoped_refptr<base::TaskRunner> io_thread_task_runner,
              const DidCreateChannelCallback& callback,
              scoped_refptr<base::TaskRunner> callback_thread_task_runner);

// Destroys a channel that was created using |CreateChannel()| (or
// |CreateChannelOnIOThread()|); must be called from the channel's I'O thread.
// |channel_info| should be the value provided to the callback to
// |CreateChannel()| (or returned by |CreateChannelOnIOThread()|). Completes
// synchronously (and posts no tasks).
MOJO_SYSTEM_IMPL_EXPORT void DestroyChannelOnIOThread(
    ChannelInfo* channel_info);

typedef base::Closure DidDestroyChannelCallback;
// Like |DestroyChannelOnIOThread()|, but asynchronous and may be called from
// any thread. The callback will be called using |callback_thread_task_runner|
// if that is non-null, or otherwise it will be called on the "channel thread".
// The "channel thread" must remain alive and continue to process tasks until
// the callback has been executed.
MOJO_SYSTEM_IMPL_EXPORT void DestroyChannel(
    ChannelInfo* channel_info,
    const DidDestroyChannelCallback& callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner);

// Inform the channel that it will soon be destroyed (doing so is optional).
// This may be called from any thread, but the caller must ensure that this is
// called before |DestroyChannel()|.
MOJO_SYSTEM_IMPL_EXPORT void WillDestroyChannelSoon(ChannelInfo* channel_info);

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_EMBEDDER_H_
