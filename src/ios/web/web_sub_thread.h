// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_SUB_THREAD_H_
#define IOS_WEB_WEB_SUB_THREAD_H_

#include "base/macros.h"
#include "ios/web/web_thread_impl.h"

namespace web {

// This simple thread object is used for the specialized threads that are spun
// up during startup.
class WebSubThread : public WebThreadImpl {
 public:
  explicit WebSubThread(WebThread::ID identifier);
  WebSubThread(WebThread::ID identifier, base::MessageLoop* message_loop);
  ~WebSubThread() override;

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  // These methods encapsulate cleanup that needs to happen on the IO thread
  // before the embedder's |CleanUp()| function is called.
  void IOThreadPreCleanUp();

  DISALLOW_COPY_AND_ASSIGN(WebSubThread);
};

}  // namespace web

#endif  // IOS_WEB_WEB_SUB_THREAD_H_
