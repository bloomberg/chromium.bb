// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerStreamHandle_h
#define WebServiceWorkerStreamHandle_h

#include <memory>

#include "mojo/public/cpp/system/data_pipe.h"
#include "public/platform/WebCommon.h"

namespace blink {

// Contains the info to send back a body to the page over Mojo's data pipe.
class BLINK_PLATFORM_EXPORT WebServiceWorkerStreamHandle {
 public:
  // Listener can observe whether the data pipe is successfully closed at the
  // end of the body or it has accidentally finished.
  class Listener {
   public:
    virtual ~Listener(){};
    virtual void OnAborted() = 0;
    virtual void OnCompleted() = 0;
  };

  void SetListener(std::unique_ptr<Listener>);
  mojo::ScopedDataPipeConsumerHandle DrainStreamDataPipe();

#if INSIDE_BLINK
  WebServiceWorkerStreamHandle(mojo::ScopedDataPipeConsumerHandle stream)
      : stream_(std::move(stream)) {
    DCHECK(stream_.is_valid());
  }
  void Aborted();
  void Completed();
#endif

 private:
  mojo::ScopedDataPipeConsumerHandle stream_;
  std::unique_ptr<Listener> listener_;
};
}

#endif  // WebServiceWorkerStreamHandle_h
