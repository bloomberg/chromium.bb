// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockWebPresentationClient_h
#define MockWebPresentationClient_h

#include "public/platform/modules/presentation/WebPresentationClient.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink {

class MockWebPresentationClient : public WebPresentationClient {
  void StartPresentation(
      const WebVector<WebURL>& presentation_urls,
      std::unique_ptr<WebPresentationConnectionCallbacks> callbacks) override {
    return startPresentation_(presentation_urls, callbacks);
  }
  void ReconnectPresentation(
      const WebVector<WebURL>& presentation_urls,
      const WebString& presentation_id,
      std::unique_ptr<WebPresentationConnectionCallbacks> callbacks) override {
    return reconnectPresentation_(presentation_urls, presentation_id,
                                  callbacks);
  }

  void GetAvailability(const WebVector<WebURL>& availability_urls,
                       std::unique_ptr<WebPresentationAvailabilityCallbacks>
                           callbacks) override {
    return getAvailability_(availability_urls, callbacks);
  }

 public:
  MOCK_METHOD1(SetController, void(WebPresentationController*));

  MOCK_METHOD1(SetReceiver, void(WebPresentationReceiver*));

  MOCK_METHOD2(startPresentation_,
               void(const WebVector<WebURL>& presentationUrls,
                    std::unique_ptr<WebPresentationConnectionCallbacks>&));

  MOCK_METHOD3(reconnectPresentation_,
               void(const WebVector<WebURL>& presentationUrls,
                    const WebString& presentationId,
                    std::unique_ptr<WebPresentationConnectionCallbacks>&));

  MOCK_METHOD2(getAvailability_,
               void(const WebVector<WebURL>& availabilityUrls,
                    std::unique_ptr<WebPresentationAvailabilityCallbacks>&));

  MOCK_METHOD1(StartListening, void(WebPresentationAvailabilityObserver*));

  MOCK_METHOD1(StopListening, void(WebPresentationAvailabilityObserver*));

  MOCK_METHOD1(SetDefaultPresentationUrls, void(const WebVector<WebURL>&));
};

}  // namespace blink

#endif  // MockWebPresentationClient_h
