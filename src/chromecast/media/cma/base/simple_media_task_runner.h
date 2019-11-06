// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_SIMPLE_MEDIA_TASK_RUNNER_H_
#define CHROMECAST_MEDIA_CMA_BASE_SIMPLE_MEDIA_TASK_RUNNER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/media/cma/base/media_task_runner.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
namespace media {

// This is a light version of task runner which post tasks immediately
// by ignoring the timestamps once receiving the request.
class SimpleMediaTaskRunner : public MediaTaskRunner {
 public:
  SimpleMediaTaskRunner(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // MediaTaskRunner implementation.
  bool PostMediaTask(const base::Location& from_here,
                     const base::Closure& task,
                     base::TimeDelta timestamp) override;

 private:
  ~SimpleMediaTaskRunner() override;

  scoped_refptr<base::SingleThreadTaskRunner> const task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMediaTaskRunner);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_SIMPLE_MEDIA_TASK_RUNNER_H_
