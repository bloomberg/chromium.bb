// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PEPPER_FLASH_FULLSCREEN_HOST_H_
#define CHROME_RENDERER_PEPPER_PEPPER_FLASH_FULLSCREEN_HOST_H_

#include <stdint.h>

#include "ppapi/host/resource_host.h"

namespace content {
class RendererPpapiHost;
}

class PepperFlashFullscreenHost : public ppapi::host::ResourceHost {
 public:
  PepperFlashFullscreenHost(content::RendererPpapiHost* host,
                            PP_Instance instance,
                            PP_Resource resource);

  PepperFlashFullscreenHost(const PepperFlashFullscreenHost&) = delete;
  PepperFlashFullscreenHost& operator=(const PepperFlashFullscreenHost&) =
      delete;

  ~PepperFlashFullscreenHost() override;

  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;
};

#endif  // CHROME_RENDERER_PEPPER_PEPPER_FLASH_FULLSCREEN_HOST_H_
