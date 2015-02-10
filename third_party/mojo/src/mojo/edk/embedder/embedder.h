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
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace embedder {

struct Configuration;
class MasterProcessDelegate;
class PlatformSupport;
class SlaveProcessDelegate;

// Returns the global configuration. In general, you should not need to change
// the configuration, but if you do you must do it before calling |Init()|.
MOJO_SYSTEM_IMPL_EXPORT Configuration* GetConfiguration();

// Must be called first, or just after setting configuration parameters, to
// initialize the (global, singleton) system.
MOJO_SYSTEM_IMPL_EXPORT void Init(scoped_ptr<PlatformSupport> platform_support);

// Initializes a master process. To be called after |Init()|.
// |master_process_delegate| should live forever (or until after
// |mojo::embedder::test::Shutdown()|); its methods will be called using
// |delegate_thread_task_runner|, which must be the task runner for the thread
// calling |InitMaster()|. |io_thread_task_runner| should be the task runner for
// some I/O thread; this should be the same as that provided to
// |CreateChannel()| (or on which |CreateChannelOnIOThread()| is called).
// TODO(vtl): Remove the |io_thread_task_runner| argument from
// |CreateChannel()| (and eventually |CreateChannel()| altogether) and require
// that either this or |InitSlave()| be called. Currently, |CreateChannel()| can
// be used with different I/O threads, but this capability will be removed.
MOJO_SYSTEM_IMPL_EXPORT void InitMaster(
    scoped_refptr<base::TaskRunner> delegate_thread_task_runner,
    MasterProcessDelegate* master_process_delegate,
    scoped_refptr<base::TaskRunner> io_thread_task_runner);

// Initializes a slave process. Similar to |InitMaster()| (see above).
// |platform_handle| should be connected to the handle passed to |AddSlave()|.
// TODO(vtl): |AddSlave()| doesn't exist yet.
MOJO_SYSTEM_IMPL_EXPORT void InitSlave(
    scoped_refptr<base::TaskRunner> delegate_thread_task_runner,
    SlaveProcessDelegate* slave_process_delegate,
    scoped_refptr<base::TaskRunner> io_thread_task_runner,
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
              DidCreateChannelCallback callback,
              scoped_refptr<base::TaskRunner> callback_thread_task_runner);

// Destroys a channel that was created using |CreateChannel()| (or
// |CreateChannelOnIOThread()|); may be called from any thread. |channel_info|
// should be the value provided to the callback to |CreateChannel()| (or
// returned by |CreateChannelOnIOThread()|). If called from the I/O thread, this
// will complete synchronously (in particular, it will post no tasks).
// TODO(vtl): If called from some other thread, it'll post tasks to the I/O
// thread. This is obviously potentially problematic if you want to shut the I/O
// thread down.
MOJO_SYSTEM_IMPL_EXPORT void DestroyChannel(ChannelInfo* channel_info);

// Inform the channel that it will soon be destroyed (doing so is optional).
// This may be called from any thread, but the caller must ensure that this is
// called before |DestroyChannel()|.
MOJO_SYSTEM_IMPL_EXPORT void WillDestroyChannelSoon(ChannelInfo* channel_info);

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

// Start waiting the handle asynchronously. On success, |callback| will be
// called exactly once, when |handle| satisfies a signal in |signals| or it
// becomes known that it will never do so. |callback| will be executed on an
// arbitrary thread. It must not call any Mojo system or embedder functions.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
AsyncWait(MojoHandle handle,
          MojoHandleSignals signals,
          base::Callback<void(MojoResult)> callback);

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_EMBEDDER_H_
