// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_BROWSER_FONT_SINGLETON_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_BROWSER_FONT_SINGLETON_HOST_H_

#include "base/macros.h"
#include "ppapi/host/resource_host.h"

namespace content {

class BrowserPpapiHost;

class PepperBrowserFontSingletonHost : public ppapi::host::ResourceHost {
 public:
  PepperBrowserFontSingletonHost(BrowserPpapiHost* host,
                                 PP_Instance instance,
                                 PP_Resource resource);
  ~PepperBrowserFontSingletonHost() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PepperBrowserFontSingletonHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_BROWSER_FONT_SINGLETON_HOST_H_
