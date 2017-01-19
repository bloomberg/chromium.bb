// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_APP_RUNTIME_H_
#define REMOTING_CLIENT_IOS_APP_RUNTIME_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "remoting/base/auto_thread.h"
#include "remoting/client/chromoting_client_runtime.h"

// TODO(nicholss): Saving this for a pending CL that introduces audio.
//#include "remoting/client/ios/audio_player_ios.h"

namespace remoting {
namespace ios {

class AppRuntime {
 public:
  AppRuntime();

  scoped_refptr<AutoThreadTaskRunner> display_task_runner() {
    return runtime_->ui_task_runner();
  }

  scoped_refptr<AutoThreadTaskRunner> network_task_runner() {
    return runtime_->network_task_runner();
  }

  scoped_refptr<AutoThreadTaskRunner> file_task_runner() {
    return runtime_->file_task_runner();
  }

  // TODO(nicholss): Saving this for a pending CL that introduces audio.
  //  scoped_refptr<AutoThreadTaskRunner> audio_task_runner() {
  //    return _runtime->audio_task_runner();
  //  }

 private:
  // This object is ref-counted, so it cleans itself up.
  ~AppRuntime();

  // TODO(nicholss): Saving this for a pending CL that introduces GL rendering.
  //  void SetupOpenGl();

  // Chromium code's connection to the OBJ_C message loop.  Once created the
  // MessageLoop will live for the life of the program.  An attempt was made to
  // create the primary message loop earlier in the programs life, but a
  // MessageLoop requires ARC to be disabled.
  std::unique_ptr<base::MessageLoopForUI> ui_loop_;

  std::unique_ptr<remoting::ChromotingClientRuntime> runtime_;

  DISALLOW_COPY_AND_ASSIGN(AppRuntime);
};

}  // namespace ios
}  // namespace remoting

#endif  // REMOTING_CLIENT_IOS_APP_RUNTIME_H_
