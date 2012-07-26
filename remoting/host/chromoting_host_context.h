// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
#define REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "remoting/jingle_glue/jingle_thread.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {

// A class that manages threads and running context for the chromoting host
// process.  This class is virtual only for testing purposes (see below).
class ChromotingHostContext {
 public:
  // Create a context.
  ChromotingHostContext(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  virtual ~ChromotingHostContext();

  // TODO(ajwong): Move the Start method out of this class. Then
  // create a static factory for construction, and destruction.  We
  // should be able to remove the need for virtual functions below
  // with that design, while preserving the relative simplicity of
  // this API.
  virtual bool Start();

  virtual JingleThread* jingle_thread();

  // Task runner for the thread that is used for the UI. In the NPAPI
  // plugin this corresponds to the main plugin thread.
  virtual base::SingleThreadTaskRunner* ui_task_runner();

  // Task runner for the thread used by the ScreenRecorder to capture
  // the screen.
  virtual base::SingleThreadTaskRunner* capture_task_runner();

  // Task runner for the thread used to encode video streams.
  virtual base::SingleThreadTaskRunner* encode_task_runner();

  // Task runner for the thread used for network IO. This thread runs
  // a libjingle message loop, and is the only thread on which
  // libjingle code may be run.
  virtual base::SingleThreadTaskRunner* network_task_runner();

  // Task runner for the thread that is used by the EventExecutor.
  //
  // TODO(sergeyu): Do we need a separate thread for EventExecutor?
  // Can we use some other thread instead?
  virtual base::SingleThreadTaskRunner* desktop_task_runner();

  // Task runner for the thread that is used for chromium's network
  // IO, particularly all HTTP requests (for OAuth and Relay servers).
  // Chromium's HTTP stack cannot be used on the network_task_runner()
  // because that thread runs libjingle's message loop, while
  // chromium's sockets must be used on a thread with a
  // MessageLoopForIO.
  //
  // TODO(sergeyu): Implement socket server for libjingle that works
  // on a regular chromium thread and use it for network_task_runner()
  // to avoid the need for io_task_runner().
  virtual base::SingleThreadTaskRunner* io_task_runner();

  // Task runner for the thread that is used for blocking file
  // IO. This thread is used by the URLRequestContext to read proxy
  // configuration and by NatConfig to read policy configs.
  virtual base::SingleThreadTaskRunner* file_task_runner();

  const scoped_refptr<net::URLRequestContextGetter>&
      url_request_context_getter();

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromotingHostContextTest, StartAndStop);

  // A thread that hosts all network operations.
  JingleThread jingle_thread_;

  // A thread that hosts screen capture.
  base::Thread capture_thread_;

  // A thread that hosts all encode operations.
  base::Thread encode_thread_;

  // A thread that hosts input injection.
  base::Thread desktop_thread_;

  // Thread for non-blocking IO operations.
  base::Thread io_thread_;

  // Thread for blocking IO operations.
  base::Thread file_thread_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingHostContext);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
