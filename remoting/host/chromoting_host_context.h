// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
#define REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "remoting/jingle_glue/jingle_thread.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {

// A class that manages threads and running context for the chromoting host
// process.  This class is virtual only for testing purposes (see below).
class ChromotingHostContext {
 public:
  // Create a context.
  ChromotingHostContext(base::MessageLoopProxy* ui_message_loop);
  virtual ~ChromotingHostContext();

  // TODO(ajwong): Move the Start method out of this class. Then
  // create a static factory for construction, and destruction.  We
  // should be able to remove the need for virtual functions below
  // with that design, while preserving the relative simplicity of
  // this API.
  virtual bool Start();

  virtual JingleThread* jingle_thread();

  virtual MessageLoop* main_message_loop();
  virtual MessageLoop* encode_message_loop();
  virtual base::MessageLoopProxy* network_message_loop();
  virtual MessageLoop* desktop_message_loop();
  virtual base::MessageLoopProxy* ui_message_loop();
  virtual base::MessageLoopProxy* io_message_loop();
  virtual base::MessageLoopProxy* file_message_loop();
  const scoped_refptr<net::URLRequestContextGetter>&
      url_request_context_getter();

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromotingHostContextTest, StartAndStop);

  // A thread that hosts all network operations.
  JingleThread jingle_thread_;

  // TODO(sergeyu): The "main" thread is used just by the
  // capturer. Consider renaming it.
  base::Thread main_thread_;

  // A thread that hosts all encode operations.
  base::Thread encode_thread_;

  // A thread that hosts desktop integration (capture, input injection, etc)
  // This is NOT a Chrome-style UI thread.
  base::Thread desktop_thread_;

  // Thread for non-blocking IO operations.
  base::Thread io_thread_;

  // Thread for blocking IO operations.
  base::Thread file_thread_;

  scoped_refptr<base::MessageLoopProxy> ui_message_loop_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingHostContext);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
