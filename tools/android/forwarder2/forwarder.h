// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_FORWARDER2_FORWARDER_H_
#define TOOLS_ANDROID_FORWARDER2_FORWARDER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "tools/android/forwarder2/self_deleter_helper.h"

namespace forwarder2 {

class PipeNotifier;
class Socket;

// Internal class that wraps a helper thread to forward traffic between
// |socket1| and |socket2|. After creating a new instance, call its Start()
// method to launch operations. Thread stops automatically if one of the socket
// disconnects, but ensures that all buffered writes to the other, still alive,
// socket, are written first. When this happens, the instance will delete itself
// automatically.
// Note that the instance will always be destroyed on the same thread that
// created it.
class Forwarder {
 public:
  // Callback used on error invoked by the Forwarder to self-delete.
  typedef base::Callback<void (scoped_ptr<Forwarder>)> ErrorCallback;

  Forwarder(scoped_ptr<Socket> socket1,
            scoped_ptr<Socket> socket2,
            PipeNotifier* deletion_notifier,
            const ErrorCallback& error_callback);

  ~Forwarder();

  void Start();

 private:
  void ThreadHandler();

  void OnThreadHandlerCompletion(const ErrorCallback& error_callback);

  SelfDeleterHelper<Forwarder> self_deleter_helper_;
  PipeNotifier* const deletion_notifier_;
  scoped_ptr<Socket> socket1_;
  scoped_ptr<Socket> socket2_;
  base::Thread thread_;
};

}  // namespace forwarder2

#endif  // TOOLS_ANDROID_FORWARDER2_FORWARDER_H_
