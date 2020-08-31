// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_CANCELABLE_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_CANCELABLE_H_

namespace location {
namespace nearby {

// An interface to provide a cancellation mechanism for objects that represent
// long-running operations.
class Cancelable {
 public:
  virtual ~Cancelable() {}

  virtual bool cancel() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_CANCELABLE_H_
