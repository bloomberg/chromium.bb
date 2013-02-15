// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_RESIZER_H_
#define REMOTING_HOST_DESKTOP_RESIZER_H_

#include <list>

#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkRect.h"

namespace remoting {

class DesktopResizer {
 public:
  virtual ~DesktopResizer() {}

  // Create a platform-specific DesktopResizer instance.
  static scoped_ptr<DesktopResizer> Create();

  // Return the current size of the desktop.
  virtual SkISize GetCurrentSize() = 0;

  // Get the list of supported sizes, which should ideally include |preferred|.
  // Implementations will generally do one of the following:
  //   1. Return the list of sizes supported by the underlying video driver,
  //      regardless of |preferred|.
  //   2. Return a list containing just |preferred|, perhaps after imposing
  //      some minimum size constraint. This will typically be the case if
  //      there are no constraints imposed by the underlying video driver.
  //   3. Return an empty list if resize is not supported.
  virtual std::list<SkISize> GetSupportedSizes(const SkISize& preferred) = 0;

  // Set the size of the desktop. |size| must be one of the sizes previously
  // returned by |GetSupportedSizes|. Note that implementations should fail
  // gracefully if the specified size is no longer supported, since monitor
  // configurations may change on the fly.
  virtual void SetSize(const SkISize& size) = 0;

  // Restore the original desktop size. The caller must provide the original
  // size of the desktop, as returned by |GetCurrentSize|, as a hint. However,
  // implementaions are free to ignore this. For example, virtual hosts will
  // typically ignore it to avoid unnecessary resize operations.
  virtual void RestoreSize(const SkISize& original) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_RESIZER_H_
