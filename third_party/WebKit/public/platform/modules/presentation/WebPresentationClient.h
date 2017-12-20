// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationClient_h
#define WebPresentationClient_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCommon.h"

#include <memory>

namespace blink {

class WebPresentationConnectionCallbacks;
class WebPresentationReceiver;
class WebString;
class WebURL;
template <typename T>
class WebVector;

// The implementation the embedder has to provide for the Presentation API to
// work.
// It is expected this class will be removed when Presentation API is fully
// onion souped (crbug.com/749327).
class WebPresentationClient {
 public:
  virtual ~WebPresentationClient() = default;

  // Passes the Blink-side delegate to the embedder.
  virtual void SetReceiver(WebPresentationReceiver*) = 0;

  // Called when the frame requests to start a new presentation.
  virtual void StartPresentation(
      const WebVector<WebURL>& presentation_urls,
      std::unique_ptr<WebPresentationConnectionCallbacks>) = 0;

  // Called when the frame requests to reconnect to an existing presentation.
  virtual void ReconnectPresentation(
      const WebVector<WebURL>& presentation_urls,
      const WebString& presentation_id,
      std::unique_ptr<WebPresentationConnectionCallbacks>) = 0;
};

}  // namespace blink

#endif  // WebPresentationClient_h
