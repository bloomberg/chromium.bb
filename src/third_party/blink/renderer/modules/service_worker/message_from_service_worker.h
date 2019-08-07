// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_MESSAGE_FROM_SERVICE_WORKER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_MESSAGE_FROM_SERVICE_WORKER_H_

#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_object_info.h"

namespace blink {

// Holds info for a message event destined for ServiceWorkerContainer.onmessage.
// https://w3c.github.io/ServiceWorker/#dom-serviceworkercontainer-onmessage
struct MessageFromServiceWorker {
  MessageFromServiceWorker(WebServiceWorkerObjectInfo source,
                           blink::TransferableMessage message);
  virtual ~MessageFromServiceWorker();

  // The service worker that posted the message.
  WebServiceWorkerObjectInfo source;

  // The message.
  blink::TransferableMessage message;

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageFromServiceWorker);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_MESSAGE_FROM_SERVICE_WORKER_H_
