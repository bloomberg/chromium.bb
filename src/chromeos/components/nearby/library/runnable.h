// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_RUNNABLE_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_RUNNABLE_H_

namespace location {
namespace nearby {

// The Runnable interface should be implemented by any class whose instances are
// intended to be executed by a thread. The class must define a method named
// run() with no arguments.
//
// https://docs.oracle.com/javase/8/docs/api/java/lang/Runnable.html
class Runnable {
 public:
  virtual ~Runnable() {}

  virtual void run() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_RUNNABLE_H_
