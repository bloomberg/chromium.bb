// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SCREEN_ORIENTATION_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SCREEN_ORIENTATION_DELEGATE_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_lock_type.h"

namespace content {

class WebContents;

// Can be implemented to provide platform specific functionality for
// ScreenOrientationProvider.
class CONTENT_EXPORT ScreenOrientationDelegate {
 public:
  ScreenOrientationDelegate() {}
  virtual ~ScreenOrientationDelegate() {}

  // Returns true if the tab must be fullscreen in order for
  // ScreenOrientationProvider to respond to requests.
  virtual bool FullScreenRequired(WebContents* web_contents) = 0;

  // Lock display to the given orientation.
  virtual void Lock(WebContents* web_contents,
                    blink::WebScreenOrientationLockType lock_orientation) = 0;

  // Are ScreenOrientationProvider requests currently supported by the platform.
  virtual bool ScreenOrientationProviderSupported() = 0;

  // Unlocks the display, allowing hardware rotation to resume.
  virtual void Unlock(WebContents* web_contents) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationDelegate);
};

} // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SCREEN_ORIENTATION_DELEGATE_H_

