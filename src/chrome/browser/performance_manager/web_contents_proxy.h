// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_WEB_CONTENTS_PROXY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_WEB_CONTENTS_PROXY_H_

#include <cstdint>

#include "base/macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace performance_manager {

// A WebContentsProxy is used to post messages out of the performance manager
// sequence that are bound for a WebContents running on the UI thread. The
// object is bound to the UI thread. A WeakPtr<WebContentsProxy> is effectively
// equivalent to a WeakPtr<WebContents>.
class WebContentsProxy {
 public:
  WebContentsProxy();
  virtual ~WebContentsProxy();

  // Allows resolving this proxy to the underlying WebContents. This must only
  // be called on the UI thread.
  virtual content::WebContents* GetWebContents() const = 0;

  // Returns the ID of the last committed navigation in the main frame of the
  // web contents. This must only be called on the UI thread.
  virtual int64_t LastNavigationId() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsProxy);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_WEB_CONTENTS_PROXY_H_
