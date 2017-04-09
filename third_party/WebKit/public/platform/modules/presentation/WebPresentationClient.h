// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationClient_h
#define WebPresentationClient_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCommon.h"

#include <memory>

namespace blink {

class WebPresentationAvailabilityObserver;
class WebPresentationController;
struct WebPresentationError;
class WebPresentationConnectionCallbacks;
class WebPresentationConnectionProxy;
class WebPresentationReceiver;
class WebString;
class WebURL;
template <typename T>
class WebVector;

// Callback for .getAvailability().
using WebPresentationAvailabilityCallbacks =
    WebCallbacks<bool, const WebPresentationError&>;

// The implementation the embedder has to provide for the Presentation API to
// work.
class WebPresentationClient {
 public:
  virtual ~WebPresentationClient() {}

  // Passes the Blink-side delegate to the embedder.
  virtual void SetController(WebPresentationController*) = 0;

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

  // Called when the frame requests to terminate a presentation.
  virtual void TerminatePresentation(const WebURL& presentation_url,
                                     const WebString& presentation_id) = 0;

  // Called when the frame requests to send String message to a presentation
  // via a presentation connection.
  // |proxy|: proxy of Blink connection object initiating send String message
  //          request. Does not pass ownership.
  virtual void SendString(const WebURL& presentation_url,
                          const WebString& presentation_id,
                          const WebString& message,
                          const WebPresentationConnectionProxy*) = 0;

  // Called when the frame requests to send ArrayBuffer/View data to a
  // presentation via a presentation connection.
  // Embedder copies the |data| and the ownership is not transferred.
  // |proxy|: proxy of Blink connection object initiating send ArrayBuffer
  //          request. Does not pass ownership.
  virtual void SendArrayBuffer(const WebURL& presentation_url,
                               const WebString& presentation_id,
                               const uint8_t* data,
                               size_t length,
                               const WebPresentationConnectionProxy*) = 0;

  // Called when the frame requests to send Blob data to a presentation via a
  // presentation connection.
  // TODO(mfoltz): Combine with sendArrayBuffer?
  // Embedder copies the |data| and the ownership is not transferred.
  // |proxy|: proxy of blink connection object initiating send Blob data
  //          request. Does not pass ownership.
  virtual void SendBlobData(const WebURL& presentation_url,
                            const WebString& presentation_id,
                            const uint8_t* data,
                            size_t length,
                            const WebPresentationConnectionProxy*) = 0;

  // Called when the frame requests to close its connection to the presentation.
  virtual void CloseConnection(const WebURL& presentation_url,
                               const WebString& presentation_id,
                               const WebPresentationConnectionProxy*) = 0;

  // Called when the frame wants to know the availability of a presentation
  // display for |availabilityUrl|.
  virtual void GetAvailability(
      const WebVector<WebURL>& availability_urls,
      std::unique_ptr<WebPresentationAvailabilityCallbacks>) = 0;

  // Start listening to changes in presentation displays availability. The
  // observer will be notified in case of a change. The observer is
  // respensible to call stopListening() before being destroyed.
  virtual void StartListening(WebPresentationAvailabilityObserver*) = 0;

  // Stop listening to changes in presentation displays availability. The
  // observer will no longer be notified in case of a change.
  virtual void StopListening(WebPresentationAvailabilityObserver*) = 0;

  // Called when a defaultRequest has been set. It sends the url associated
  // with it for the embedder.
  virtual void SetDefaultPresentationUrls(const WebVector<WebURL>&) = 0;
};

}  // namespace blink

#endif  // WebPresentationClient_h
