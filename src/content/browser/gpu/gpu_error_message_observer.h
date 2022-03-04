/*
 * Copyright (C) 2019 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_GPU_ERROR_MESSAGE_OBSERVER_H
#define INCLUDED_GPU_ERROR_MESSAGE_OBSERVER_H

#include "content/public/browser/gpu_data_manager_observer.h"

#include <string>

namespace content {

// GpuErrorMessageObserver listens to the log message sent from GPU process to
// the browser process. More specifically, it observes warning, error & fatal
// messages and decides if terminating the GPU process is needed.
class GpuErrorMessageObserver : public GpuDataManagerObserver {
 public:
  class Delegate {
   public:
    virtual void OnFatalErrorDetected(const std::string& header,
                                      const std::string& message) = 0;
  };

  explicit GpuErrorMessageObserver(Delegate* delegate);

  ~GpuErrorMessageObserver() final;

  void OnAddLogMessage(int level,
                       const std::string& header,
                       const std::string& message) final;

 private:
  Delegate* delegate_;
};

}  // namespace content

#endif  // INCLUDED_GPU_ERROR_MESSAGE_OBSERVER_H
