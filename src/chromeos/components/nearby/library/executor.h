// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_EXECUTOR_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_EXECUTOR_H_

namespace location {
namespace nearby {

// This abstract class is the superclass of all classes representing an
// Executor.
class Executor {
 public:
  virtual ~Executor() {}

  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ExecutorService.html#shutdown--
  virtual void shutdown() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_EXECUTOR_H_
