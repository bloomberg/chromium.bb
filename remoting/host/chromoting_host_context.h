// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
#define REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {

class AutoThreadTaskRunner;

// A class that manages threads and running context for the chromoting host
// process.  This class is virtual only for testing purposes (see below).
class ChromotingHostContext {
 public:
  ~ChromotingHostContext();

  // Create threads and URLRequestContextGetter for use by a host.
  // During shutdown the caller should tear-down the ChromotingHostContext and
  // then continue to run until |ui_task_runner| is no longer referenced.
  // NULL is returned if any threads fail to start.
  static scoped_ptr<ChromotingHostContext> Create(
      scoped_refptr<AutoThreadTaskRunner> ui_task_runner);

  // Task runner for the thread used for audio capture and encoding.
  scoped_refptr<AutoThreadTaskRunner> audio_task_runner();

  // Task runner for the thread that is used for blocking file
  // IO. This thread is used by the URLRequestContext to read proxy
  // configuration and by NatConfig to read policy configs.
  scoped_refptr<AutoThreadTaskRunner> file_task_runner();

  // Task runner for the thread that is used by the InputInjector.
  //
  // TODO(sergeyu): Do we need a separate thread for InputInjector?
  // Can we use some other thread instead?
  scoped_refptr<AutoThreadTaskRunner> input_task_runner();

  // Task runner for the thread used for network IO. This thread runs
  // a libjingle message loop, and is the only thread on which
  // libjingle code may be run.
  scoped_refptr<AutoThreadTaskRunner> network_task_runner();

  // Task runner for the thread that is used for the UI. In the NPAPI
  // plugin this corresponds to the main plugin thread.
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner();

  // Task runner for the thread used by the ScreenRecorder to capture
  // the screen.
  scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner();

  // Task runner for the thread used to encode video streams.
  scoped_refptr<AutoThreadTaskRunner> video_encode_task_runner();

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter();

 private:
  ChromotingHostContext(AutoThreadTaskRunner* ui_task_runner);

  // Thread for audio capture and encoding.
  scoped_refptr<AutoThreadTaskRunner> audio_task_runner_;

  // Thread for I/O operations.
  scoped_refptr<AutoThreadTaskRunner> file_task_runner_;

  // Thread for input injection.
  scoped_refptr<AutoThreadTaskRunner> input_task_runner_;

  // Thread for network operations.
  scoped_refptr<AutoThreadTaskRunner> network_task_runner_;

  // Caller-supplied UI thread. This is usually the application main thread.
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;

  // Thread for screen capture.
  scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner_;

  // Thread for video encoding.
  scoped_refptr<AutoThreadTaskRunner> video_encode_task_runner_;

  // Serves URLRequestContexts that use the network and UI task runners.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingHostContext);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
