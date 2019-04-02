// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_

#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"

namespace blink {

// PlzDedicatedWorker:
// WebDedicatedWorkerHostFactoryClient is the interface to access
// content::DedicatedWorkerHostFactoryClient from blink::DedicatedWorker.
class WebDedicatedWorkerHostFactoryClient {
 public:
  virtual ~WebDedicatedWorkerHostFactoryClient() = default;

  // Requests the creation of DedicatedWorkerHost in the browser process.
  // For non-PlzDedicatedWorker. This will be removed once PlzDedicatedWorker is
  // enabled by default.
  virtual void CreateWorkerHostDeprecated(
      const blink::WebSecurityOrigin& script_origin) = 0;
  // For PlzDedicatedWorker.
  virtual void CreateWorkerHost(
      const blink::WebURL& script_url,
      const blink::WebSecurityOrigin& script_origin,
      mojo::ScopedMessagePipeHandle blob_url_token) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_
