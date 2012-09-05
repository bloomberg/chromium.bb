// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
#define REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {
class AutoThreadTaskRunner;

// A class that manages threads and running context for the chromoting host
// process.  This class is virtual only for testing purposes (see below).
class ChromotingHostContext {
 public:
  // Create a context.
  ChromotingHostContext(
      scoped_refptr<AutoThreadTaskRunner> ui_task_runner);
  virtual ~ChromotingHostContext();

  void ReleaseTaskRunners();

  // TODO(ajwong): Move the Start method out of this class. Then
  // create a static factory for construction, and destruction.  We
  // should be able to remove the need for virtual functions below
  // with that design, while preserving the relative simplicity of
  // this API.
  virtual bool Start();

  // Task runner for the thread used for audio capture and encoding.
  virtual base::SingleThreadTaskRunner* audio_task_runner();

  // Task runner for the thread used by the ScreenRecorder to capture
  // the screen.
  virtual base::SingleThreadTaskRunner* capture_task_runner();

  // Task runner for the thread that is used by the EventExecutor.
  //
  // TODO(sergeyu): Do we need a separate thread for EventExecutor?
  // Can we use some other thread instead?
  virtual base::SingleThreadTaskRunner* desktop_task_runner();

  // Task runner for the thread used to encode video streams.
  virtual base::SingleThreadTaskRunner* encode_task_runner();

  // Task runner for the thread that is used for blocking file
  // IO. This thread is used by the URLRequestContext to read proxy
  // configuration and by NatConfig to read policy configs.
  virtual base::SingleThreadTaskRunner* file_task_runner();

  // Task runner for the thread used for network IO. This thread runs
  // a libjingle message loop, and is the only thread on which
  // libjingle code may be run.
  virtual base::SingleThreadTaskRunner* network_task_runner();

  // Task runner for the thread that is used for the UI. In the NPAPI
  // plugin this corresponds to the main plugin thread.
  virtual base::SingleThreadTaskRunner* ui_task_runner();

  const scoped_refptr<net::URLRequestContextGetter>&
      url_request_context_getter();

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromotingHostContextTest, StartAndStop);

  // A thread that hosts audio capture and encoding.
  base::Thread audio_thread_;

  // A thread that hosts screen capture.
  base::Thread capture_thread_;

  // A thread that hosts input injection.
  base::Thread desktop_thread_;

  // A thread that hosts all encode operations.
  base::Thread encode_thread_;

  // Thread for blocking IO operations.
  base::Thread file_thread_;

  // A thread that hosts all network operations.
  base::Thread network_thread_;

  // Task runners wrapping the above threads. These should be declared after
  // the corresponding threads to guarantee proper order of destruction.
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> desktop_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingHostContext);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
